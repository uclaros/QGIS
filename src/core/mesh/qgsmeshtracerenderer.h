/***************************************************************************
                         qgsmeshtracerenderer.h
                         -------------------------
    begin                : November 2019
    copyright            : (C) 2019 by Vincent Cloarec
    email                : vcloarec at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMESHTRACERENDERER_H
#define QGSMESHTRACERENDERER_H

#include <memory>

#include "qgis.h"
#include "qgis_core.h"
#include "qgsmeshlayer.h"
#include "qgsrendercontext.h"
#include "qgstriangularmesh.h"

#include <QSize>
#include <QVector>

class QgsVectorFieldSettings;
class QgsVectorFieldTraceAnimationGenerator;

/**
 * \ingroup core
 *
 * \brief A wrapper for QgsMeshParticuleTracesField used to render the particles.
 *
 * Available for Python binding
 *
 * \since QGIS 3.12
 * \deprecated QGIS 4.2. Use QgsVectorFieldTraceAnimationGenerator instead.
 */
class CORE_EXPORT QgsMeshVectorTraceAnimationGenerator
{
  public:
    //!Constructor to use from QgsMeshVectorRenderer
    Q_DECL_DEPRECATED QgsMeshVectorTraceAnimationGenerator(
      const QgsTriangularMesh &triangularMesh,
      const QgsMeshDataBlock &dataSetVectorValues,
      const QgsMeshDataBlock &scalarActiveFaceFlagValues,
      bool dataIsOnVertices,
      const QgsRenderContext &rendererContext,
      const QgsRectangle &layerExtent,
      double magMax,
      const QgsVectorFieldSettings &vectorSettings
    ) SIP_SKIP;

    //!Constructor to use with Python binding
    Q_DECL_DEPRECATED QgsMeshVectorTraceAnimationGenerator( QgsMeshLayer *layer, const QgsRenderContext &rendererContext );

    QgsMeshVectorTraceAnimationGenerator( const QgsMeshVectorTraceAnimationGenerator &other );

    ~QgsMeshVectorTraceAnimationGenerator();

    //! seeds particles in the vector fields
    void seedRandomParticles( int count );

    //! Moves all the particles using frame per second (fps) to calculate the displacement and return the rendered frame
    QImage imageRendered();

    //! Sets the number of frames per seconds that will be rendered
    void setFPS( int FPS );

    //! Sets the max number of pixels that can be go through by the particles in 1 second
    void setMaxSpeedPixel( int max );

    //! Sets maximum life time of particles in seconds
    void setParticlesLifeTime( double particleLifeTime );

    //! Sets colors of particle
    void setParticlesColor( const QColor &c );

    //! Sets particle size in px
    void setParticlesSize( double width );

    //! Sets the tail factor, used to adjust the length of the tail. 0 : minimum length, >1 increase the tail
    void setTailFactor( double fct );

    //! Sets the minimum tail length
    void setMinimumTailLength( int l );

    //! Sets the visual persistence of the tail
    void setTailPersitence( double p );

    QgsMeshVectorTraceAnimationGenerator &operator=( const QgsMeshVectorTraceAnimationGenerator &other );

  private:
    std::unique_ptr<QgsVectorFieldTraceAnimationGenerator> mGenerator;
};

#endif // QGSMESHTRACERENDERER_H
