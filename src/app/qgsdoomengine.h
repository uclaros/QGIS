/***************************************************************************
    qgsdoomengine.h
    ---------------
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

#ifndef QGSDOOMENGINE_H
#define QGSDOOMENGINE_H

#include <atomic>

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>

/**
 * \brief Qt wrapper around the doomgeneric library.
 *
 * Manages the doom game loop in a background thread and provides
 * thread-safe access to the current rendered frame and keyboard input queue.
 *
 * Also implements the required doomgeneric C platform callbacks
 * (DG_Init, DG_DrawFrame, DG_GetKey, DG_SleepMs, DG_GetTicksMs, DG_SetWindowTitle).
 */
class QgsDoomEngine : public QObject
{
    Q_OBJECT

  public:
    explicit QgsDoomEngine( QObject *parent = nullptr );
    ~QgsDoomEngine() override;

    //! Returns the global engine instance (used by C callbacks)
    static QgsDoomEngine *instance();

    /**
     * Starts the doom engine using the given WAD file path.
     * If the engine is already running this is a no-op.
     * On first call, initialises doomgeneric; on subsequent calls re-starts the tick loop.
     */
    void start( const QString &wadPath );

    /**
     * Stops the doom tick loop and waits for the background thread to finish.
     */
    void stop();

    //! Enqueues a key event (doom key code + pressed/released). Called from the main thread.
    void addKeyEvent( unsigned char doomKey, bool pressed );

    //! Signals the doom loop to stop on the next tick. Called from I_Quit (doom thread).
    void requestStop() { mRunning = false; }

    //! Returns a copy of the latest rendered frame. Thread-safe.
    QImage currentFrame() const;

    //! Converts a Qt::Key value to a doom key code. Returns 0 if unmapped.
    static unsigned char qtKeyToDoom( int qtKey );

    // Internal: called from DG_DrawFrame (background thread)
    void onNewFrame();

    //! Returns milliseconds since engine start. Used by DG_GetTicksMs callback.
    uint32_t elapsedMs() const;

    //! Dequeues the next key event. Returns true if one was available. Used by DG_GetKey callback.
    bool dequeueKey( int *pressed, unsigned char *doomKey );

  signals:
    //! Emitted in the main thread when the doom loop has exited (normal quit or fatal error).
    void engineFinished();

  private:
    mutable QMutex mFrameMutex;
    QImage mFrameBuffer;

    QMutex mKeyMutex;

    struct KeyEvent
    {
        unsigned char key;
        bool pressed;
    };
    QQueue<KeyEvent> mKeyQueue;

    std::atomic<bool> mRunning { false };
    bool mInitialized = false;

    QThread *mThread = nullptr;
    QElapsedTimer mElapsedTimer;

    // Storage for argv strings passed to doomgeneric_Create — must outlive the doom loop
    QByteArray mArgProgName;
    QByteArray mArgIwad;
    QByteArray mArgWadPath;
};

#endif // QGSDOOMENGINE_H
