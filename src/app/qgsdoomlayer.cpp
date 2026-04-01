/***************************************************************************
    qgsdoomlayer.cpp
    ----------------
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

#include "qgsdoomlayer.h"

#include "qgsdoomengine.h"
#include "qgsmapcanvas.h"
#include "qgsproject.h"
#include "qgsrendercontext.h"

#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QString>

using namespace Qt::StringLiterals;

// -----------------------------------------------------------------------
// QgsDoomLayerRenderer
// -----------------------------------------------------------------------

QgsDoomLayerRenderer::QgsDoomLayerRenderer( const QString &layerId, QgsDoomEngine *engine, QgsRenderContext &context )
  : QgsMapLayerRenderer( layerId, &context )
  , mEngine( engine )
{}

bool QgsDoomLayerRenderer::render()
{
  const QImage frame = mEngine->currentFrame();
  if ( frame.isNull() )
    return true;

  QPainter *painter = renderContext()->painter();
  if ( !painter )
    return true;

  // Paint doom frame scaled to fill the entire canvas paint device
  const QRect deviceRect( 0, 0, painter->device()->width(), painter->device()->height() );
  painter->drawImage( deviceRect, frame );

  return true;
}

// -----------------------------------------------------------------------
// QgsDoomLayer
// -----------------------------------------------------------------------

const QString QgsDoomLayer::LAYER_TYPE = u"doom"_s;

QgsDoomLayer::QgsDoomLayer( QgsMapCanvas *canvas, const QString &wadPath )
  : QgsPluginLayer( LAYER_TYPE, tr( "DOOM" ) )
  , mCanvas( canvas )
{
  setValid( true );

  setAutoRefreshInterval( 50 ); // ~20 fps redraws
  setAutoRefreshMode( Qgis::AutoRefreshMode::RedrawOnly );

  mEngine = new QgsDoomEngine( this );
  connect( mEngine, &QgsDoomEngine::engineFinished, this, &QgsDoomLayer::stopDoom, Qt::QueuedConnection );
  mEngine->start( wadPath );

  mCanvas->installEventFilter( this );
  mCanvas->setFocus();
}

QgsDoomLayer::~QgsDoomLayer()
{
  if ( mCanvas )
    mCanvas->removeEventFilter( this );
}

QgsMapLayerRenderer *QgsDoomLayer::createMapRenderer( QgsRenderContext &rendererContext )
{
  return new QgsDoomLayerRenderer( id(), mEngine, rendererContext );
}

bool QgsDoomLayer::eventFilter( QObject *obj, QEvent *event )
{
  if ( obj != mCanvas || mStopping )
    return false;

  // Prevent QGIS keyboard shortcuts (e.g. Ctrl+Z) from stealing keys doom needs.
  // Qt sends ShortcutOverride before KeyPress; accepting it suppresses shortcut handling.
  if ( event->type() == QEvent::ShortcutOverride )
  {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
    if ( keyEvent->key() == Qt::Key_Escape || QgsDoomEngine::qtKeyToDoom( keyEvent->key() ) != 0 )
    {
      event->accept();
      return true;
    }
    return false;
  }

  if ( event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease )
  {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
    const bool pressed = ( event->type() == QEvent::KeyPress );

    const unsigned char doomKey = QgsDoomEngine::qtKeyToDoom( keyEvent->key() );
    if ( doomKey != 0 )
    {
      mEngine->addKeyEvent( doomKey, pressed );
      return true;
    }
  }

  return false;
}

void QgsDoomLayer::stopDoom()
{
  if ( mStopping )
    return;
  mStopping = true;

  mEngine->stop();

  if ( mCanvas )
    mCanvas->removeEventFilter( this );

  // Remove from project — this will trigger layer deletion
  QgsProject::instance()->removeMapLayer( id() );
}
