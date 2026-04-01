/***************************************************************************
    qgsdoomengine.cpp
    -----------------
    Date                 : March 2026
    Copyright            : (C) 2026 QGIS Development Team
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsdoomengine.h"

// doomgeneric headers are C
extern "C"
{
#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_system.h"
#include "z_zone.h"
}

#include <QThread>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

// Global singleton accessed from the C callbacks
static QgsDoomEngine *sDoomEngineInstance = nullptr;

// Exception thrown by our I_Error override to terminate the doom loop
// without calling exit() from a background thread.
struct QgsDoomFatalError : std::runtime_error
{
    explicit QgsDoomFatalError( const char *msg )
      : std::runtime_error( msg )
    {}
};

// -----------------------------------------------------------------------
// i_system.c replacements
//
// i_system.c is excluded from the doomgeneric build so that we can
// provide a custom I_Error that throws instead of calling exit(-1).
// All other symbols from i_system.c are provided here as stubs.
// -----------------------------------------------------------------------

struct AtExitEntry
{
    atexit_func_t func;
    boolean run_on_error;
    AtExitEntry *next;
};
static AtExitEntry *sAtExitList = nullptr;

extern "C"
{
  void I_AtExit( atexit_func_t func, boolean run_on_error )
  {
    AtExitEntry *entry = static_cast<AtExitEntry *>( malloc( sizeof( AtExitEntry ) ) );
    entry->func = func;
    entry->run_on_error = run_on_error;
    entry->next = sAtExitList;
    sAtExitList = entry;
  }

  void I_Tactile( int /*on*/, int /*off*/, int /*total*/ )
  {}

  byte *I_ZoneBase( int *size )
  {
    const int defaultRamMiB = 6;
    *size = defaultRamMiB * 1024 * 1024;
    byte *mem = static_cast<byte *>( malloc( static_cast<size_t>( *size ) ) );
    fprintf( stderr, "zone memory: %p, %x allocated for zone\n", static_cast<void *>( mem ), *size );
    return mem;
  }

  boolean I_ConsoleStdout( void )
  {
    return 0;
  }

  void I_PrintBanner( char *msg )
  {
    puts( msg );
  }

  void I_PrintDivider( void )
  {
    for ( int i = 0; i < 75; ++i )
      putchar( '=' );
    putchar( '\n' );
  }

  void I_PrintStartupBanner( char *gamedescription )
  {
    I_PrintDivider();
    I_PrintBanner( gamedescription );
    I_PrintDivider();
  }

  void I_Quit( void )
  {
    // Signal the game loop to stop on the next iteration and return normally.
    // We deliberately skip the atexit handlers: running them from the doom
    // background thread initialises KDE TLS objects (KDirWatch / KF6Solid)
    // whose destructors later crash when the thread exits because they try
    // to use Qt APIs that require a running event loop.
    // Returning from I_Quit is safe — doom will complete the current tick and
    // the while(mRunning) loop then exits naturally.
    if ( QgsDoomEngine *engine = QgsDoomEngine::instance() )
      engine->requestStop();
  }

  // Override doom's I_Error so it throws instead of calling exit(-1).
  // This is safe because there is no setjmp/longjmp between the doom call
  // stack and our QThread lambda, so the exception unwinds cleanly.
  void I_Error( char *error, ... )
  {
    char buf[512];
    va_list args;
    va_start( args, error );
    vsnprintf( buf, sizeof( buf ), error, args );
    va_end( args );
    fprintf( stderr, "Doom I_Error: %s\n", buf );
    throw QgsDoomFatalError( buf );
  }

  // DOS memory emulation (used by RANGECHECK path in v_video.c)
  static const unsigned char sDosMemDump[10] = { 0x9E, 0x0F, 0xC9, 0x00, 0x65, 0x04, 0x70, 0x00, 0x16, 0x00 };

  boolean I_GetMemoryValue( unsigned int offset, void *value, int size )
  {
    if ( offset + static_cast<unsigned int>( size ) > 10 )
      return 0;
    switch ( size )
    {
      case 1:
        *static_cast<unsigned char *>( value ) = sDosMemDump[offset];
        return 1;
      case 2:
        *static_cast<unsigned short *>( value ) = static_cast<unsigned short>( sDosMemDump[offset] | ( sDosMemDump[offset + 1] << 8 ) );
        return 1;
      case 4:
        *static_cast<unsigned int *>( value ) = static_cast<unsigned int>( sDosMemDump[offset] | ( sDosMemDump[offset + 1] << 8 ) | ( sDosMemDump[offset + 2] << 16 ) | ( sDosMemDump[offset + 3] << 24 ) );
        return 1;
    }
    return 0;
  }

} // extern "C" (i_system replacements)

// -----------------------------------------------------------------------
// QgsDoomEngine
// -----------------------------------------------------------------------

QgsDoomEngine::QgsDoomEngine( QObject *parent )
  : QObject( parent )
{
  sDoomEngineInstance = this;
  mElapsedTimer.start();
}

QgsDoomEngine::~QgsDoomEngine()
{
  stop();
  if ( sDoomEngineInstance == this )
    sDoomEngineInstance = nullptr;
}

QgsDoomEngine *QgsDoomEngine::instance()
{
  return sDoomEngineInstance;
}

void QgsDoomEngine::start( const QString &wadPath )
{
  if ( mRunning )
    return;

  mRunning = true;

  // Capture strings before entering the thread lambda
  mArgProgName = QByteArray( "qgis" );
  mArgIwad = QByteArray( "-iwad" );
  mArgWadPath = wadPath.toLocal8Bit();

  mThread = QThread::create( [this]() {
    try
    {
      if ( !mInitialized )
      {
        char *argv[] = { mArgProgName.data(), mArgIwad.data(), mArgWadPath.data(), nullptr };
        mInitialized = true;
        doomgeneric_Create( 3, argv );
      }

      while ( mRunning )
      {
        doomgeneric_Tick();
      }
    }
    catch ( const QgsDoomFatalError &e )
    {
      fprintf( stderr, "Doom thread terminated: %s\n", e.what() );
      mRunning = false;
    }
  } );

  // Emit engineFinished() in the main thread when the doom loop exits for any reason.
  connect( mThread, &QThread::finished, this, &QgsDoomEngine::engineFinished, Qt::QueuedConnection );

  mThread->start();
}

void QgsDoomEngine::stop()
{
  mRunning = false;
  if ( mThread )
  {
    mThread->wait( 3000 );
    delete mThread;
    mThread = nullptr;
  }
}

unsigned char QgsDoomEngine::qtKeyToDoom( int qtKey )
{
  switch ( qtKey )
  {
    case Qt::Key_Right:
      return KEY_RIGHTARROW;
    case Qt::Key_Left:
      return KEY_LEFTARROW;
    case Qt::Key_Up:
      return KEY_UPARROW;
    case Qt::Key_Down:
      return KEY_DOWNARROW;
    case Qt::Key_Escape:
      return KEY_ESCAPE;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      return KEY_ENTER;
    case Qt::Key_Tab:
      return KEY_TAB;
    case Qt::Key_Backspace:
      return KEY_BACKSPACE;
    case Qt::Key_Pause:
      return KEY_PAUSE;
    case Qt::Key_Equal:
      return KEY_EQUALS;
    case Qt::Key_Minus:
      return KEY_MINUS;
    case Qt::Key_F1:
      return KEY_F1;
    case Qt::Key_F2:
      return KEY_F2;
    case Qt::Key_F3:
      return KEY_F3;
    case Qt::Key_F4:
      return KEY_F4;
    case Qt::Key_F5:
      return KEY_F5;
    case Qt::Key_F6:
      return KEY_F6;
    case Qt::Key_F7:
      return KEY_F7;
    case Qt::Key_F8:
      return KEY_F8;
    case Qt::Key_F9:
      return KEY_F9;
    case Qt::Key_F10:
      return KEY_F10;
    case Qt::Key_F11:
      return KEY_F11;
    case Qt::Key_F12:
      return KEY_F12;
    case Qt::Key_Shift:
      return KEY_RSHIFT;
    case Qt::Key_Control:
      return KEY_FIRE; // doom's key_fire defaults to KEY_FIRE, not KEY_RCTRL
    case Qt::Key_Alt:
      return KEY_RALT;
    case Qt::Key_Space:
      return KEY_USE;
    default:
      if ( qtKey >= 32 && qtKey <= 126 )
        return static_cast<unsigned char>( std::tolower( qtKey ) );
      return 0;
  }
}

void QgsDoomEngine::addKeyEvent( unsigned char doomKey, bool pressed )
{
  QMutexLocker lock( &mKeyMutex );
  mKeyQueue.enqueue( { doomKey, pressed } );
}

QImage QgsDoomEngine::currentFrame() const
{
  QMutexLocker lock( &mFrameMutex );
  return mFrameBuffer;
}

uint32_t QgsDoomEngine::elapsedMs() const
{
  return static_cast<uint32_t>( mElapsedTimer.elapsed() );
}

bool QgsDoomEngine::dequeueKey( int *pressed, unsigned char *doomKey )
{
  QMutexLocker lock( &mKeyMutex );
  if ( mKeyQueue.isEmpty() )
    return false;
  const KeyEvent ev = mKeyQueue.dequeue();
  *pressed = ev.pressed ? 1 : 0;
  *doomKey = ev.key;
  return true;
}

void QgsDoomEngine::onNewFrame()
{
  if ( !DG_ScreenBuffer )
    return;

  QImage frame( DOOMGENERIC_RESX, DOOMGENERIC_RESY, QImage::Format_RGB32 );
  std::memcpy( frame.bits(), DG_ScreenBuffer, static_cast<size_t>( DOOMGENERIC_RESX ) * DOOMGENERIC_RESY * sizeof( uint32_t ) );

  {
    QMutexLocker lock( &mFrameMutex );
    mFrameBuffer = std::move( frame );
  }
}

// -----------------------------------------------------------------------
// doomgeneric C platform callbacks
// -----------------------------------------------------------------------

extern "C"
{
  void DG_Init()
  {
    // Nothing to do — Qt timing is already initialised
  }

  void DG_DrawFrame()
  {
    if ( QgsDoomEngine *engine = QgsDoomEngine::instance() )
      engine->onNewFrame();
  }

  void DG_SleepMs( uint32_t ms )
  {
    QThread::msleep( ms );
  }

  uint32_t DG_GetTicksMs()
  {
    if ( QgsDoomEngine *engine = QgsDoomEngine::instance() )
      return engine->elapsedMs();
    return 0;
  }

  int DG_GetKey( int *pressed, unsigned char *doomKey )
  {
    if ( QgsDoomEngine *engine = QgsDoomEngine::instance() )
      return engine->dequeueKey( pressed, doomKey ) ? 1 : 0;
    return 0;
  }

  void DG_SetWindowTitle( const char * /*title*/ )
  {
    // no window title to set — DOOM renders inside QGIS map canvas
  }

} // extern "C"
