/***************************************************************************
    qgsdoomlayer.h
    --------------
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

#ifndef QGSDOOMLAYER_H
#define QGSDOOMLAYER_H

#include "qgsmaplayerrenderer.h"
#include "qgspluginlayer.h"

class QgsDoomEngine;
class QgsMapCanvas;
class QgsRenderContext;

/**
 * \brief Renderer for QgsDoomLayer — paints the current DOOM frame onto the canvas.
 */
class QgsDoomLayerRenderer : public QgsMapLayerRenderer
{
  public:
    QgsDoomLayerRenderer( const QString &layerId, QgsDoomEngine *engine, QgsRenderContext &context );
    bool render() override;

  private:
    QgsDoomEngine *mEngine = nullptr;
};

/**
 * \brief A QgsPluginLayer that renders a live DOOM game session inside the QGIS map canvas.
 *
 * Activated by typing "doom" in the coordinates status bar widget.
 * The layer installs a keyboard event filter on the map canvas to feed input
 * into the DOOM engine.  Press Escape to exit DOOM mode.
 */
class QgsDoomLayer : public QgsPluginLayer
{
    Q_OBJECT

  public:
    static const QString LAYER_TYPE;

    /**
     * Creates a doom layer associated with \a canvas.
     * The doom engine is started immediately using the given \a wadPath.
     */
    QgsDoomLayer( QgsMapCanvas *canvas, const QString &wadPath );
    ~QgsDoomLayer() override;

    QgsMapLayerRenderer *createMapRenderer( QgsRenderContext &rendererContext ) override;

    //! Not cloneable — only one DOOM session per application
    QgsDoomLayer *clone() const override { return nullptr; }

    bool readXml( const QDomNode &, QgsReadWriteContext & ) override { return true; }
    bool writeXml( QDomNode &, QDomDocument &, const QgsReadWriteContext & ) const override { return true; }
    bool readSymbology( const QDomNode &, QString &, QgsReadWriteContext &, StyleCategories ) override { return true; }
    bool writeSymbology( QDomNode &, QDomDocument &, QString &, const QgsReadWriteContext &, StyleCategories ) const override { return true; }
    void setTransformContext( const QgsCoordinateTransformContext & ) override {}

  protected:
    bool eventFilter( QObject *obj, QEvent *event ) override;

  private slots:
    //! Removes this layer from the project and tears down the doom engine.
    void stopDoom();

  private:
    QgsMapCanvas *mCanvas = nullptr;
    QgsDoomEngine *mEngine = nullptr;
    bool mStopping = false;
};

#endif // QGSDOOMLAYER_H
