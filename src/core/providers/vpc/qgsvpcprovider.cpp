/***************************************************************************
                         qgsvpcprovider.cpp
                         ------------------
    begin                : March 2023
    copyright            : (C) 2023 by Stefanos Natsis
    email                : uclaros at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgis.h"
#include "qgslogger.h"
#include "qgsproviderregistry.h"
#include "qgsvpcprovider.h"
#include "qgscopcpointcloudindex.h"
#include "qgsremotecopcpointcloudindex.h"
#include "qgspointcloudsublayer.h"
#include "qgsruntimeprofiler.h"
#include "qgsapplication.h"
#include "qgsprovidersublayerdetails.h"
#include "qgsproviderutils.h"
#include "qgsthreadingutils.h"
#include <nlohmann/json.hpp>
#include "qgsgeometry.h"
#include "qgsmultipolygon.h"
#include "qgscoordinatetransform.h"

#include <QIcon>

///@cond PRIVATE

#define PROVIDER_KEY QStringLiteral( "vpc" )
#define PROVIDER_DESCRIPTION QStringLiteral( "Virtual point cloud data provider" )

QgsVpcProvider::QgsVpcProvider(
  const QString &uri,
  const QgsDataProvider::ProviderOptions &options,
  QgsDataProvider::ReadFlags flags )
  : QgsPointCloudDataProvider( uri, options, flags )
{
  bool isRemote = uri.startsWith( QStringLiteral( "http" ), Qt::CaseSensitivity::CaseInsensitive );
  if ( isRemote )
    mIndex.reset( nullptr );
  else
    mIndex.reset( nullptr );

  std::unique_ptr< QgsScopedRuntimeProfile > profile;
  if ( QgsApplication::profiler()->groupIsActive( QStringLiteral( "projectload" ) ) )
    profile = std::make_unique< QgsScopedRuntimeProfile >( tr( "Open data source" ), QStringLiteral( "projectload" ) );

  mPolygonBounds.reset( new QgsGeometry( new QgsMultiPolygon() ) );

  parseFile();
}

QgsVpcProvider::~QgsVpcProvider() = default;

QgsCoordinateReferenceSystem QgsVpcProvider::crs() const
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  return mCrs;
}

QgsRectangle QgsVpcProvider::extent() const
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  return mExtent;
}

QgsPointCloudAttributeCollection QgsVpcProvider::attributes() const
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  // TODO: how to handle attributes?
  const QVector<QgsPointCloudAttribute> attributes =
  {
    QgsPointCloudAttribute( QStringLiteral( "X" ), QgsPointCloudAttribute::Int32 ),
    QgsPointCloudAttribute( QStringLiteral( "Y" ), QgsPointCloudAttribute::Int32 ),
    QgsPointCloudAttribute( QStringLiteral( "Z" ), QgsPointCloudAttribute::Int32 )
  };
  return QgsPointCloudAttributeCollection( attributes );
}

bool QgsVpcProvider::isValid() const
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  return !mSubLayers.isEmpty();
}

QString QgsVpcProvider::name() const
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  return PROVIDER_KEY;
}

QString QgsVpcProvider::description() const
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  return PROVIDER_DESCRIPTION;
}

QgsPointCloudIndex *QgsVpcProvider::index() const
{
  // non fatal for now -- 2d rendering of point clouds is not thread safe and calls this
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS_NON_FATAL

  return nullptr; // todo: vpc has no index of itself?
}

qint64 QgsVpcProvider::pointCount() const
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  return static_cast<qint64>( mPointCount );
}

void QgsVpcProvider::loadIndex( )
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  // Index already loaded -> no need to load
  if ( mIndex->isValid() )
    return;
  mIndex->load( dataSourceUri() );
}

QVariantMap QgsVpcProvider::originalMetadata() const
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  return QVariantMap();//mIndex->originalMetadata();
}

void QgsVpcProvider::generateIndex()
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  //no-op, index is always generated
}

void QgsVpcProvider::parseFile()
{
  QVariantMap res;
  QFile file( dataSourceUri() );
  const QFileInfo fInfo( file );

  nlohmann::json data;
  if ( file.open( QFile::ReadOnly ) )
  {
    try
    {
      data = json::parse( file.readAll().toStdString() );
    }
    catch ( std::exception &e )
    {
      appendError( QgsErrorMessage( QStringLiteral( "JSON parsing error: %1" ).arg( QString::fromStdString( e.what() ) ), QString() ) );
      return;
    }
  }

  if ( data["type"] != "FeatureCollection" ||
       !data.contains( "features" ) )
  {
    appendError( QgsErrorMessage( QStringLiteral( "Invalid VPC file" ), QString( "asd" ) ) );
    return;
  }

  for ( const auto &f : data["features"] )
  {
    if ( !f.contains( "type" ) || f["type"] != "Feature" ||
         !f.contains( "stac_version" ) ||
         !f.contains( "assets" ) || !f["assets"].is_object() ||
         !f.contains( "properties" ) || !f["properties"].is_object() )
    {
      QgsDebugMsg( QStringLiteral( "Malformed STAC item: %1" ).arg( QString::fromStdString( f ) ) );
      continue;
    }

    if ( f["stac_version"] != "1.0.0" )
    {
      QgsDebugMsg( QStringLiteral( "Unsupported STAC version: %1" ).arg( QString::fromStdString( f["stac_version"] ) ) );
      continue;
    }

    nlohmann::json firstAsset = *f["assets"].begin();

    QgsPointCloudSubLayer si;

    si.uri = QString::fromStdString( firstAsset["href"] );
    if ( f["properties"].contains( "pc:count" ) )
      si.count = f["properties"]["pc:count"];

    if ( !mCrs.isValid() )
    {
      if ( f["properties"].contains( "proj:epsg" ) )
        mCrs.createFromSrsId( f["properties"]["proj:epsg"].get<long>() );
      else if ( f["properties"].contains( "proj:wkt2" ) )
        mCrs.createFromString( QString::fromStdString( f["properties"]["proj:wkt2"] ) );
    }

    if ( f["properties"].contains( "proj:bbox" ) )
    {
      nlohmann::json nativeBbox = f["properties"]["proj:bbox"];
      si.extent = QgsRectangle( nativeBbox[0].get<double>(), nativeBbox[1].get<double>(),
                                nativeBbox[3].get<double>(), nativeBbox[4].get<double>() );
    }
    else if ( f.contains( "bbox" ) && mCrs.isValid() )
    {
      nlohmann::json bboxWgs = f["bbox"];
      QgsRectangle bbox = QgsRectangle( bboxWgs[0].get<double>(), bboxWgs[1].get<double>(),
                                        bboxWgs[2].get<double>(), bboxWgs[3].get<double>() );

      QgsCoordinateTransform transform( QgsCoordinateReferenceSystem::fromEpsgId( 4326 ), mCrs, QgsCoordinateTransformContext() );
      try
      {
        si.extent = transform.transformBoundingBox( bbox );
      }
      catch ( ... )
      {
        QgsDebugMsg( QStringLiteral( "Cannot transform bbox to layer crs." ) );
        continue;
      }
    }
    else
    {
      QgsDebugMsg( QStringLiteral( "Missing extent information." ) );
      continue;
    }

    if ( si.uri.startsWith( QLatin1String( "./" ) ) )
    {
      // resolve relative path
      si.uri = fInfo.absoluteDir().absoluteFilePath( si.uri );
    }

    for ( auto &schemaItem : f["properties"]["pc:schemas"] )
    {
      // todo: handle attributes
//          vpcf.schema.push_back(VirtualPointCloud::SchemaItem(schemaItem["name"], schemaItem["type"], schemaItem["size"].get<int>()));
    }

    mSubLayers.push_back( si );
    mPolygonBounds->addPart( QgsGeometry::fromRect( si.extent ) );
  }
  mExtent = mPolygonBounds->boundingBox();
}

QgsGeometry QgsVpcProvider::polygonBounds() const
{
  return *mPolygonBounds;
  QgsGeometry bounds = QgsGeometry::fromMultiPolygonXY( QgsMultiPolygonXY() );
  for ( const auto &i : std::as_const( mSubLayers ) )
  {
    bounds.addPart( QgsGeometry::fromRect( i.extent ) );
  }
  return bounds;
}

QVector<QgsPointCloudIndex *> QgsVpcProvider::indexes() const
{
  QVector<QgsPointCloudIndex *> v;

  for ( const auto &i : std::as_const( mSubLayers ) )
  {
    if ( i.index && i.index->isValid() )
      v.append( i.index.get() );
  }
  return v;
}

void QgsVpcProvider::loadIndex( int i )
{
  QGIS_PROTECT_QOBJECT_THREAD_ACCESS

  if ( i >= mSubLayers.size() )
    return;

  QgsPointCloudSubLayer &sl = mSubLayers[ i ];
  // Index already loaded -> no need to load
  if ( sl.index )
    return;

  if ( sl.uri.startsWith( QStringLiteral( "http" ), Qt::CaseSensitivity::CaseInsensitive ) )
  {
    sl.index.reset( new QgsRemoteCopcPointCloudIndex() );
  }
  else
  {
    sl.index.reset( new QgsCopcPointCloudIndex() );
  }
  sl.index->load( sl.uri );
}


QgsVpcProviderMetadata::QgsVpcProviderMetadata():
  QgsProviderMetadata( PROVIDER_KEY, PROVIDER_DESCRIPTION )
{
}

QIcon QgsVpcProviderMetadata::icon() const
{
  return QgsApplication::getThemeIcon( QStringLiteral( "mIconPointCloudLayer.svg" ) );
}

QgsVpcProvider *QgsVpcProviderMetadata::createProvider( const QString &uri, const QgsDataProvider::ProviderOptions &options, QgsDataProvider::ReadFlags flags )
{
  return new QgsVpcProvider( uri, options, flags );
}

QList<QgsProviderSublayerDetails> QgsVpcProviderMetadata::querySublayers( const QString &uri, Qgis::SublayerQueryFlags, QgsFeedback * ) const
{
  const QVariantMap parts = decodeUri( uri );
  if ( parts.value( QStringLiteral( "file-name" ) ).toString().endsWith( ".vpc", Qt::CaseSensitivity::CaseInsensitive ) )
  {
    QgsProviderSublayerDetails details;
    details.setUri( uri );
    details.setProviderKey( QStringLiteral( "vpc" ) );
    details.setType( Qgis::LayerType::PointCloud );
    details.setName( QgsProviderUtils::suggestLayerNameFromFilePath( uri ) );
    return {details};
  }
  else
  {
    return {};
  }
}

int QgsVpcProviderMetadata::priorityForUri( const QString &uri ) const
{
  const QVariantMap parts = decodeUri( uri );
  if ( parts.value( QStringLiteral( "file-name" ) ).toString().endsWith( ".vpc", Qt::CaseSensitivity::CaseInsensitive ) )
    return 100;

  return 0;
}

QList<Qgis::LayerType> QgsVpcProviderMetadata::validLayerTypesForUri( const QString &uri ) const
{
  const QVariantMap parts = decodeUri( uri );
  if ( parts.value( QStringLiteral( "file-name" ) ).toString().endsWith( ".vpc", Qt::CaseSensitivity::CaseInsensitive ) )
    return QList< Qgis::LayerType>() << Qgis::LayerType::PointCloud;

  return QList< Qgis::LayerType>();
}

QVariantMap QgsVpcProviderMetadata::decodeUri( const QString &uri ) const
{
  QVariantMap uriComponents;
  QUrl url = QUrl::fromUserInput( uri );
  uriComponents.insert( QStringLiteral( "file-name" ), url.fileName() );
  uriComponents.insert( QStringLiteral( "path" ), uri );
  return uriComponents;
}

QString QgsVpcProviderMetadata::filters( QgsProviderMetadata::FilterType type )
{
  switch ( type )
  {
    case QgsProviderMetadata::FilterType::FilterVector:
    case QgsProviderMetadata::FilterType::FilterRaster:
    case QgsProviderMetadata::FilterType::FilterMesh:
    case QgsProviderMetadata::FilterType::FilterMeshDataset:
      return QString();

    case QgsProviderMetadata::FilterType::FilterPointCloud:
      return QObject::tr( "Virtual Point Clouds" ) + QStringLiteral( " (*.vpc *.VPC)" );
  }
  return QString();
}

QgsProviderMetadata::ProviderCapabilities QgsVpcProviderMetadata::providerCapabilities() const
{
  return FileBasedUris;
}

QList<Qgis::LayerType> QgsVpcProviderMetadata::supportedLayerTypes() const
{
  return { Qgis::LayerType::PointCloud };
}

QString QgsVpcProviderMetadata::encodeUri( const QVariantMap &parts ) const
{
  const QString path = parts.value( QStringLiteral( "path" ) ).toString();
  return path;
}

QgsProviderMetadata::ProviderMetadataCapabilities QgsVpcProviderMetadata::capabilities() const
{
  return ProviderMetadataCapability::LayerTypesForUri
         | ProviderMetadataCapability::PriorityForUri
         | ProviderMetadataCapability::QuerySublayers;
}
///@endcond

