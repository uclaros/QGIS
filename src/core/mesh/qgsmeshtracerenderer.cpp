/***************************************************************************
                         qgsmeshtracerenderer.cpp
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

#include "qgsmeshtracerenderer.h"

#include "qgsmeshlayerutils.h"
#include "qgsmeshvectorfieldvaluesource.h"
#include "qgsvectorfieldsettings.h"
#include "qgsvectorfieldtraceanimationgenerator.h"

QgsMeshVectorTraceAnimationGenerator::QgsMeshVectorTraceAnimationGenerator(
  const QgsTriangularMesh &triangularMesh,
  const QgsMeshDataBlock &dataSetVectorValues,
  const QgsMeshDataBlock &scalarActiveFaceFlagValues,
  bool dataIsOnVertices,
  const QgsRenderContext &rendererContext,
  const QgsRectangle &layerExtent,
  double magMax,
  const QgsVectorFieldSettings &vectorSettings
)
  : mGenerator(
      std::make_unique<QgsVectorFieldTraceAnimationGenerator>(
        QgsMeshVectorFieldValueSource::create(
          triangularMesh,
          dataSetVectorValues,
          scalarActiveFaceFlagValues,
          QgsMeshLayerUtils::calculateMagnitudes( dataSetVectorValues ),
          dataIsOnVertices ? QgsMeshDatasetGroupMetadata::DataOnVertices : QgsMeshDatasetGroupMetadata::DataOnFaces,
          layerExtent,
          magMax
        ),
        rendererContext,
        vectorSettings
      )
    )
{}

QgsMeshVectorTraceAnimationGenerator::QgsMeshVectorTraceAnimationGenerator( QgsMeshLayer *layer, const QgsRenderContext &rendererContext )
  : mGenerator( QgsVectorFieldTraceAnimationGenerator::fromMeshLayer( layer, rendererContext ) )
{}

QgsMeshVectorTraceAnimationGenerator::QgsMeshVectorTraceAnimationGenerator( const QgsMeshVectorTraceAnimationGenerator &other )
  : mGenerator( other.mGenerator ? std::make_unique<QgsVectorFieldTraceAnimationGenerator>( *other.mGenerator ) : nullptr )
{}

QgsMeshVectorTraceAnimationGenerator::~QgsMeshVectorTraceAnimationGenerator() = default;

void QgsMeshVectorTraceAnimationGenerator::seedRandomParticles( int count )
{
  if ( mGenerator )
    mGenerator->seedRandomParticles( count );
}

QImage QgsMeshVectorTraceAnimationGenerator::imageRendered()
{
  return mGenerator ? mGenerator->imageRendered() : QImage();
}

void QgsMeshVectorTraceAnimationGenerator::setFPS( int FPS )
{
  if ( mGenerator )
    mGenerator->setFPS( FPS );
}

void QgsMeshVectorTraceAnimationGenerator::setMaxSpeedPixel( int max )
{
  if ( mGenerator )
    mGenerator->setMaxSpeedPixel( max );
}

void QgsMeshVectorTraceAnimationGenerator::setParticlesLifeTime( double particleLifeTime )
{
  if ( mGenerator )
    mGenerator->setParticlesLifeTime( particleLifeTime );
}

void QgsMeshVectorTraceAnimationGenerator::setParticlesColor( const QColor &c )
{
  if ( mGenerator )
    mGenerator->setParticlesColor( c );
}

void QgsMeshVectorTraceAnimationGenerator::setParticlesSize( double width )
{
  if ( mGenerator )
    mGenerator->setParticlesSize( width );
}

void QgsMeshVectorTraceAnimationGenerator::setTailFactor( double fct )
{
  if ( mGenerator )
    mGenerator->setTailFactor( fct );
}

void QgsMeshVectorTraceAnimationGenerator::setMinimumTailLength( int l )
{
  if ( mGenerator )
    mGenerator->setMinimumTailLength( l );
}

void QgsMeshVectorTraceAnimationGenerator::setTailPersitence( double p )
{
  if ( mGenerator )
    mGenerator->setTailPersitence( p );
}

QgsMeshVectorTraceAnimationGenerator &QgsMeshVectorTraceAnimationGenerator::operator=( const QgsMeshVectorTraceAnimationGenerator &other )
{
  if ( &other == this )
    return *this;

  mGenerator = other.mGenerator ? std::make_unique<QgsVectorFieldTraceAnimationGenerator>( *other.mGenerator ) : nullptr;

  return ( *this );
}
