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
#include "qgsjsonutils.h"
#include "qgsmultipolygon.h"

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

//  loadIndex( );
  mPolygonBounds.reset( new QgsGeometry( new QgsMultiPolygon() ) );

  parseFile();
  if ( mIndex && !mIndex->isValid() )
  {
    appendError( mIndex->error() );
  }
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

  if ( file.open( QFile::ReadOnly ) )
  {
    res = QgsJsonUtils::parseJson( file.readAll() ).toMap();
  }

  if ( res.isEmpty() ||
       !res.contains( QLatin1String( "vpc" ) ) ||
       !res.contains( QLatin1String( "metadata" ) ) ||
       !res.contains( QLatin1String( "files" ) )
     )
  {
    // invalid, todo: verbose error checks
    return;
  }
  const QVariantMap meta = res.value( QStringLiteral( "metadata" ) ).toMap();
  mCrs.createFromString( meta.value( QStringLiteral( "crs" ) ).toString() );
  const QgsCoordinateTransform transform( QgsCoordinateReferenceSystem::fromEpsgId( 4326 ), mCrs, QgsCoordinateTransformContext() );
  const QList<QVariant> files = res.value( QStringLiteral( "files" ) ).toList();

  for ( const QVariant &f : files )
  {
    QVariantMap map = f.toMap();

    QgsPointCloudSubLayer i;
    const QString filename = map.value( QStringLiteral( "filename" ) ).toString();

    if ( filename.startsWith( QStringLiteral( "http" ), Qt::CaseSensitivity::CaseInsensitive ) )
    {
      i.uri = filename;
    }
    else
    {
      i.uri = fInfo.absoluteDir().absoluteFilePath( filename );
    }
    i.count = map.value( QStringLiteral( "count" ) ).toLongLong();
    const QList<QVariant> bbox = map.value( QStringLiteral( "bbox" ) ).toList();
    QgsRectangle extentWgs = QgsRectangle( bbox.at( 0 ).toDouble(),
                                           bbox.at( 1 ).toDouble(),
                                           bbox.at( 3 ).toDouble(),
                                           bbox.at( 4 ).toDouble()
                                         );

    i.extent = transform.transformBoundingBox( extentWgs );

    mSubLayers.push_back( i );

    mPolygonBounds->addPart( QgsGeometry::fromRect( i.extent ) );
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

