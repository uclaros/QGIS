/***************************************************************************
  qgsflatterraingenerator.cpp
  --------------------------------------
  Date                 : July 2017
  Copyright            : (C) 2017 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsflatterraingenerator.h"

#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DCore/QTransform>
#include <QVector3D>

#include "qgs3dmapsettings.h"
#include "qgschunknode_p.h"
#include "qgsterrainentity_p.h"
#include "qgsterraintileentity_p.h"
#include "qgs3dutils.h"
/// @cond PRIVATE


//---------------


FlatTerrainChunkLoader::FlatTerrainChunkLoader( QgsTerrainEntity *terrain, QgsChunkNode *node )
  : QgsTerrainTileLoader( terrain, node )
{
  loadTexture();
}


Qt3DCore::QEntity *FlatTerrainChunkLoader::createEntity( Qt3DCore::QEntity *parent )
{
  QgsTerrainTileEntity *entity = new QgsTerrainTileEntity( mNode->tileId() );

  // make geometry renderer

  // simple quad geometry shared by all tiles
  // QPlaneGeometry by default is 1x1 with mesh resolution QSize(2,2), centered at 0
  // TODO: the geometry could be shared inside Terrain instance (within terrain-generator specific data?)
  mTileGeometry = new Qt3DExtras::QPlaneGeometry;

  Qt3DRender::QGeometryRenderer *mesh = new Qt3DRender::QGeometryRenderer;
  mesh->setGeometry( mTileGeometry ); // takes ownership if the component has no parent
  entity->addComponent( mesh ); // takes ownership if the component has no parent

  // create material

  const Qgs3DMapSettings &map = terrain()->map3D();

  // create transform

  Qt3DCore::QTransform *transform = nullptr;
  transform = new Qt3DCore::QTransform();
  entity->addComponent( transform );

  // set up transform according to the extent covered by the quad geometry
  const QgsAABB bbox = mNode->bbox();

  const QgsAABB mapFullExtent = Qgs3DUtils::mapToWorldExtent( map.extent(), bbox.yMin, bbox.yMax, map.origin() );

  const QgsAABB commonExtent = QgsAABB( std::max( bbox.xMin, mapFullExtent.xMin ),
                                        bbox.yMin,
                                        std::max( bbox.zMin, mapFullExtent.zMin ),
                                        std::min( bbox.xMax, mapFullExtent.xMax ),
                                        bbox.yMax,
                                        std::min( bbox.zMax, mapFullExtent.zMax )
                                      );
  const double xSide = commonExtent.xExtent();
  const double zSide = commonExtent.zExtent();

  transform->setScale3D( QVector3D( xSide, 1, zSide ) );
  transform->setTranslation( QVector3D( commonExtent.xMin + xSide / 2, 0, commonExtent.zMin + zSide / 2 ) );

  createTextureComponent( entity, map.isTerrainShadingEnabled(), map.terrainShadingMaterial(), !map.layers().empty() );

  entity->setParent( parent );
  return entity;
}

/// @endcond

// ---------------

QgsChunkLoader *QgsFlatTerrainGenerator::createChunkLoader( QgsChunkNode *node ) const
{
  return new FlatTerrainChunkLoader( mTerrain, node );
}

QgsTerrainGenerator *QgsFlatTerrainGenerator::clone() const
{
  QgsFlatTerrainGenerator *cloned = new QgsFlatTerrainGenerator;
  cloned->mCrs = mCrs;
  cloned->mExtent = mExtent;
  cloned->updateTilingScheme();
  return cloned;
}

QgsTerrainGenerator::Type QgsFlatTerrainGenerator::type() const
{
  return QgsTerrainGenerator::Flat;
}

QgsRectangle QgsFlatTerrainGenerator::extent() const
{
  return mTerrainTilingScheme.tileToExtent( 0, 0, 0 );
}

void QgsFlatTerrainGenerator::rootChunkHeightRange( float &hMin, float &hMax ) const
{
  hMin = 0;
  hMax = 0;
}

void QgsFlatTerrainGenerator::writeXml( QDomElement &elem ) const
{
  const QgsRectangle r = mExtent;
  QDomElement elemExtent = elem.ownerDocument().createElement( QStringLiteral( "extent" ) );
  elemExtent.setAttribute( QStringLiteral( "xmin" ), QString::number( r.xMinimum() ) );
  elemExtent.setAttribute( QStringLiteral( "xmax" ), QString::number( r.xMaximum() ) );
  elemExtent.setAttribute( QStringLiteral( "ymin" ), QString::number( r.yMinimum() ) );
  elemExtent.setAttribute( QStringLiteral( "ymax" ), QString::number( r.yMaximum() ) );
  elem.appendChild( elemExtent );

  // crs is not read/written - it should be the same as destination crs of the map
}

void QgsFlatTerrainGenerator::readXml( const QDomElement &elem )
{
  const QDomElement elemExtent = elem.firstChildElement( QStringLiteral( "extent" ) );
  const double xmin = elemExtent.attribute( QStringLiteral( "xmin" ) ).toDouble();
  const double xmax = elemExtent.attribute( QStringLiteral( "xmax" ) ).toDouble();
  const double ymin = elemExtent.attribute( QStringLiteral( "ymin" ) ).toDouble();
  const double ymax = elemExtent.attribute( QStringLiteral( "ymax" ) ).toDouble();

  setExtent( QgsRectangle( xmin, ymin, xmax, ymax ) );

  // crs is not read/written - it should be the same as destination crs of the map
}

QVector<QgsChunkNode *> QgsFlatTerrainGenerator::createChildren( QgsChunkNode *node ) const
{
  QVector<QgsChunkNode *> children;

  if ( node->level() >= mMaxLevel )
    return children;

  const QgsChunkNodeId nodeId = node->tileId();
  const float childError = node->error() / 2;
  const QgsAABB bbox = node->bbox();
  float xc = bbox.xCenter(), zc = bbox.zCenter();

  for ( int i = 0; i < 4; ++i )
  {
    int dx = i & 1, dy = !!( i & 2 );
    const QgsChunkNodeId childId( nodeId.d + 1, nodeId.x * 2 + dx, nodeId.y * 2 + ( dy ? 0 : 1 ) );  // TODO: inverse dy?
    // the Y and Z coordinates below are intentionally flipped, because
    // in chunk node IDs the X,Y axes define horizontal plane,
    // while in our 3D scene the X,Z axes define the horizontal plane
    const float chXMin = dx ? xc : bbox.xMin;
    const float chXMax = dx ? bbox.xMax : xc;
    const float chZMin = dy ? zc : bbox.zMin;
    const float chZMax = dy ? bbox.zMax : zc;
    const float chYMin = bbox.yMin;
    const float chYMax = bbox.yMax;
    QgsAABB childBbox = QgsAABB( chXMin, chYMin, chZMin, chXMax, chYMax, chZMax );
    QgsRectangle childRect = Qgs3DUtils::worldToMapExtent( childBbox, mTerrain->map3D().origin() );
    if ( mExtent.intersects( childRect ) )
      children << new QgsChunkNode( childId, childBbox, childError, node );
  }
  return children;
}

void QgsFlatTerrainGenerator::setCrs( const QgsCoordinateReferenceSystem &crs )
{
  mCrs = crs;
  updateTilingScheme();
}

void QgsFlatTerrainGenerator::setExtent( const QgsRectangle &extent )
{
  if ( mExtent == extent )
    return;

  mExtent = extent;
  updateTilingScheme();

  emit extentChanged();
}

void QgsFlatTerrainGenerator::updateTilingScheme()
{
  if ( mExtent.isNull() )
  {
    mTerrainTilingScheme = QgsTilingScheme();
  }
  else
  {
    // the real extent will be a square where the given extent fully fits
    mTerrainTilingScheme = QgsTilingScheme( mExtent, mCrs );
  }
}
