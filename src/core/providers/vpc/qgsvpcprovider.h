/***************************************************************************
                         qgsvpcprovider.h
                         ----------------
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

#ifndef QGSVPCPROVIDER_H
#define QGSVPCPROVIDER_H

#include "qgspointclouddataprovider.h"
#include "qgsprovidermetadata.h"

#include <memory>

///@cond PRIVATE
#define SIP_NO_FILE

class QgsCopcPointCloudIndex;
class QgsRemoteCopcPointCloudIndex;

class QgsVpcProvider: public QgsPointCloudDataProvider
{
    Q_OBJECT
  public:
    struct subIndex
    {
      QgsPointCloudIndex *index;
      QString uri;
      QgsRectangle extent;
      qint64 count;
    };

    QgsVpcProvider( const QString &uri,
                    const QgsDataProvider::ProviderOptions &providerOptions,
                    QgsDataProvider::ReadFlags flags = QgsDataProvider::ReadFlags() );

    ~QgsVpcProvider();

    QgsCoordinateReferenceSystem crs() const override;

    QgsRectangle extent() const override;
    QgsPointCloudAttributeCollection attributes() const override;
    bool isValid() const override;
    QString name() const override;
    QString description() const override;
    QgsPointCloudIndex *index() const override;
    qint64 pointCount() const override;
    QVariantMap originalMetadata() const override;
    void loadIndex( ) override;
    void generateIndex( ) override;
    PointCloudIndexGenerationState indexingState( ) override { return PointCloudIndexGenerationState::Indexed; }

  private:
    void parseFile();
    QVector<subIndex> mSubIndexes;
    std::unique_ptr<QgsPointCloudIndex> mIndex;

    QStringList mUriList;
    QgsRectangle mExtent;
    qint64 mPointCount;
    QgsCoordinateReferenceSystem mCrs;
};

class QgsVpcProviderMetadata : public QgsProviderMetadata
{
    Q_OBJECT
  public:
    QgsVpcProviderMetadata();
    QIcon icon() const override;
    QgsProviderMetadata::ProviderMetadataCapabilities capabilities() const override;
    QgsVpcProvider *createProvider( const QString &uri, const QgsDataProvider::ProviderOptions &options, QgsDataProvider::ReadFlags flags = QgsDataProvider::ReadFlags() ) override;
    QList< QgsProviderSublayerDetails > querySublayers( const QString &uri, Qgis::SublayerQueryFlags flags = Qgis::SublayerQueryFlags(), QgsFeedback *feedback = nullptr ) const override;
    int priorityForUri( const QString &uri ) const override;
    QList< Qgis::LayerType > validLayerTypesForUri( const QString &uri ) const override;
    QString encodeUri( const QVariantMap &parts ) const override;
    QVariantMap decodeUri( const QString &uri ) const override;
    QString filters( FilterType type ) override;
    ProviderCapabilities providerCapabilities() const override;
    QList< Qgis::LayerType > supportedLayerTypes() const override;
};

///@endcond
#endif // QGSVPCPROVIDER_H
