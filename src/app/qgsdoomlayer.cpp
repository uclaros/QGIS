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
#include "qgsmaptopixel.h"
#include "qgsproject.h"
#include "qgsrasterblock.h"
#include "qgsrasterprojector.h"
#include "qgsrendercontext.h"

#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QString>

// doomgeneric header for DOOMGENERIC_RESX / DOOMGENERIC_RESY
extern "C"
{
#include "doomgeneric.h"
}

using namespace Qt::StringLiterals;

// -----------------------------------------------------------------------
// QgsDoomProvider
// -----------------------------------------------------------------------

QgsDoomProvider::QgsDoomProvider( QgsDoomEngine *engine, const QgsRectangle &extent )
  : mEngine( engine )
  , mExtent( extent )
{}

QgsRasterInterface *QgsDoomProvider::clone() const
{
  return new QgsDoomProvider( mEngine, mExtent );
}

Qgis::DataType QgsDoomProvider::dataType( int /*bandNo*/ ) const
{
  return Qgis::DataType::ARGB32;
}

int QgsDoomProvider::xSize() const
{
  return DOOMGENERIC_RESX;
}

int QgsDoomProvider::ySize() const
{
  return DOOMGENERIC_RESY;
}

QgsRasterBlock *QgsDoomProvider::block( int /*bandNo*/, const QgsRectangle &extent, int width, int height, QgsRasterBlockFeedback * /*feedback*/ )
{
  QImage output( width, height, QImage::Format_ARGB32 );
  output.fill( Qt::transparent );

  const QImage frame = mEngine->currentFrame();
  if ( !frame.isNull() )
  {
    const QgsRectangle intersection = extent.intersect( mExtent );
    if ( !intersection.isEmpty() )
    {
      // Map the intersection to a sub-rect of the doom frame (source pixels)
      const double fx0 = ( intersection.xMinimum() - mExtent.xMinimum() ) / mExtent.width();
      const double fx1 = ( intersection.xMaximum() - mExtent.xMinimum() ) / mExtent.width();
      const double fy0 = ( mExtent.yMaximum() - intersection.yMaximum() ) / mExtent.height();
      const double fy1 = ( mExtent.yMaximum() - intersection.yMinimum() ) / mExtent.height();
      const QRectF srcRect( fx0 * frame.width(), fy0 * frame.height(), ( fx1 - fx0 ) * frame.width(), ( fy1 - fy0 ) * frame.height() );

      // Map the intersection to a sub-rect of the output image (destination pixels)
      const double dx0 = ( intersection.xMinimum() - extent.xMinimum() ) / extent.width();
      const double dx1 = ( intersection.xMaximum() - extent.xMinimum() ) / extent.width();
      const double dy0 = ( extent.yMaximum() - intersection.yMaximum() ) / extent.height();
      const double dy1 = ( extent.yMaximum() - intersection.yMinimum() ) / extent.height();
      const QRectF dstRect( dx0 * width, dy0 * height, ( dx1 - dx0 ) * width, ( dy1 - dy0 ) * height );

      QPainter p( &output );
      p.setRenderHint( QPainter::SmoothPixmapTransform );
      p.drawImage( dstRect, frame, srcRect );
    }
  }

  auto outputBlock = std::make_unique<QgsRasterBlock>();
  outputBlock->setImage( &output );
  return outputBlock.release();
}

// -----------------------------------------------------------------------
// QgsDoomLayerRenderer
// -----------------------------------------------------------------------

QgsDoomLayerRenderer::QgsDoomLayerRenderer( const QString &layerId, QgsDoomEngine *engine, const QgsRectangle &extent, const QgsCoordinateReferenceSystem &layerCrs, QgsRenderContext &context )
  : QgsMapLayerRenderer( layerId, &context )
  , mEngine( engine )
  , mExtent( extent )
  , mLayerCrs( layerCrs )
{}

bool QgsDoomLayerRenderer::render()
{
  QPainter *painter = renderContext()->painter();
  if ( !painter )
    return true;

  // Determine the destination CRS from the render context's coordinate transform.
  // Falls back to the layer CRS (no reprojection) if the transform is not set.
  QgsCoordinateReferenceSystem destCrs = renderContext()->coordinateTransform().destinationCrs();
  if ( !destCrs.isValid() )
    destCrs = mLayerCrs;

  // Build the raster pipeline: doom frame source → projector
  QgsDoomProvider provider( mEngine, mExtent );

  QgsRasterProjector projector;
  projector.setCrs( mLayerCrs, destCrs, renderContext()->transformContext() );
  projector.setInput( &provider );

  // renderContext()->extent() is in the layer's CRS (QGIS pre-transforms the map
  // extent into layer CRS for clipping).  QgsRasterProjector::block() needs the
  // extent in the *destination* CRS instead.  Derive it from mapToPixel(), which
  // is always expressed in the destination (map) CRS.
  const QSize outSize = renderContext()->outputSize();
  const QgsMapToPixel &mtp = renderContext()->mapToPixel();
  const QgsPointXY topLeft = mtp.toMapCoordinates( 0, 0 );
  const QgsPointXY bottomRight = mtp.toMapCoordinates( outSize.width(), outSize.height() );
  const QgsRectangle destExtent( topLeft, bottomRight );

  std::unique_ptr<QgsRasterBlock> block( projector.block( 1, destExtent, outSize.width(), outSize.height() ) );
  if ( !block )
    return true;

  const QImage img = block->image();
  if ( img.isNull() )
    return true;

  painter->drawImage( QPoint( 0, 0 ), img );
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

  // Fix the layer's CRS and extent to the canvas state at creation time so that
  // the doom frame is geo-referenced and pans/scales with the map.
  setCrs( QgsProject::instance()->crs() );
  setExtent( canvas->extent() );

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
  return new QgsDoomLayerRenderer( id(), mEngine, extent(), crs(), rendererContext );
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
