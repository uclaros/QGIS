/***************************************************************************
    qgsauthplanetarycomputermethod.cpp
    ------------------------
    begin                : October 2025
    copyright            : (C) 2025 by Stefanos Natsis
    author               : Stefanos Natsis
    email                : uclaros at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsauthplanetarycomputermethod.h"
#include "moc_qgsauthplanetarycomputermethod.cpp"

#include "qgsauthmanager.h"
#include "qgslogger.h"
#include "qgsapplication.h"
#include "qgsmessagelog.h"
#include "qgsblockingnetworkrequest.h"
#include "qgsauthmethodregistry.h"
#include "qgsnetworkaccessmanager.h"

#include <nlohmann/json.hpp>

#ifdef HAVE_GUI
#include "qgsauthplanetarycomputeredit.h"
#endif

#include <QNetworkProxy>
#include <QMutexLocker>
#include <QUuid>
#include <QUrlQuery>


const QString QgsAuthPlanetaryComputerMethod::AUTH_METHOD_KEY = QStringLiteral( "PlanetaryComputer" );
const QString QgsAuthPlanetaryComputerMethod::AUTH_METHOD_DESCRIPTION = QStringLiteral( "MS Planetary Computer" );
const QString QgsAuthPlanetaryComputerMethod::AUTH_METHOD_DISPLAY_DESCRIPTION = tr( "MS Planetary Computer" );
const QString QgsAuthPlanetaryComputerMethod::BLOB_STORAGE_SAS_SIGN_URL = QStringLiteral( "https://planetarycomputer.microsoft.com/api/sas/v1/token/%1/%2" );
const QString QgsAuthPlanetaryComputerMethod::OPEN_SAS_SIGN_URL = QStringLiteral( "https://planetarycomputer.microsoft.com/api/sas/v1/sign?href=" );
const QString QgsAuthPlanetaryComputerMethod::PRO_SAS_SIGN_URL = QStringLiteral( "%1://%2/sas/sign?api-version=2025-04-30-preview&href=" );
const QString QgsAuthPlanetaryComputerMethod::BLOB_STORAGE_DOMAIN = QStringLiteral( ".blob.core.windows.net" );

// const QString QgsAuthPlanetaryComputerMethod::OAUTH_REQUEST_URL = QStringLiteral( "https://login.microsoftonline.com/{tenant}/oauth2/v2.0/authorize" );
// const QString QgsAuthPlanetaryComputerMethod::OAUTH_TOKEN_URL = QStringLiteral( "https://login.microsoftonline.com/{tenant}/oauth2/v2.0/token" );
const QString QgsAuthPlanetaryComputerMethod::OAUTH_REQUEST_URL = QStringLiteral( "https://login.microsoftonline.com/common/oauth2/authorize" );

QMap<QString, QgsAuthMethodConfig> QgsAuthPlanetaryComputerMethod::sAuthConfigCache = QMap<QString, QgsAuthMethodConfig>();
QMap<QString, QgsAuthPlanetaryComputerMethod::SasToken> QgsAuthPlanetaryComputerMethod::sSasTokensCache = QMap<QString, QgsAuthPlanetaryComputerMethod::SasToken>();


QgsAuthPlanetaryComputerMethod::QgsAuthPlanetaryComputerMethod()
{
  setVersion( 2 );
  setExpansions( QgsAuthMethod::NetworkRequest | QgsAuthMethod::DataSourceUri );
  setDataProviders( QStringList() << QStringLiteral( "ows" ) << QStringLiteral( "wfs" ) // convert to lowercase
                                  << QStringLiteral( "wcs" ) << QStringLiteral( "wms" ) << QStringLiteral( "gdal" ) << QStringLiteral( "stac" ) );

  qDebug() << "\n********** AUTH METHOD CREATED *************";
}

QString QgsAuthPlanetaryComputerMethod::key() const
{
  return AUTH_METHOD_KEY;
}

QString QgsAuthPlanetaryComputerMethod::description() const
{
  return AUTH_METHOD_DESCRIPTION;
}

QString QgsAuthPlanetaryComputerMethod::displayDescription() const
{
  return AUTH_METHOD_DISPLAY_DESCRIPTION;
}

bool QgsAuthPlanetaryComputerMethod::updateNetworkRequest( QNetworkRequest &request, const QString &authcfg, const QString &dataprovider )
{
  Q_UNUSED( dataprovider )
  const QgsAuthMethodConfig config = getMethodConfig( authcfg );
  if ( !config.isValid() )
  {
    QgsDebugError( QStringLiteral( "Update request config FAILED for authcfg: %1: config invalid" ).arg( authcfg ) );
    return false;
  }

  // we only need to update requests for planetary computer pro
  // but we don't want to update blob storage requets as those are signed with a sas token
  if ( mOauth2 && !request.url().host().endsWith( BLOB_STORAGE_DOMAIN ) )
  {
    mOauth2->updateNetworkRequest( request, authcfg );
    auto t = request.rawHeader( "Authorization" );
    qDebug() << "Authorization: " << t;
  }

  return true;
}

bool QgsAuthPlanetaryComputerMethod::updateDataSourceUriItems( QStringList &connectionItems, const QString &authcfg, const QString &dataprovider )
{
  Q_UNUSED( dataprovider )
  const QMutexLocker locker( &mMutex );
  const QgsAuthMethodConfig config = getMethodConfig( authcfg );
  if ( !config.isValid() )
  {
    QgsDebugError( QStringLiteral( "Update URI items FAILED for authcfg: %1: config invalid" ).arg( authcfg ) );
    return false;
  }

  const bool isPro = config.config( QStringLiteral( "serverType" ) ) == QLatin1String( "pro" );
  QString signUrl;
  if ( isPro )
  {
    QUrl rootUrl( config.config( QStringLiteral( "rootUrl" ) ) );
    signUrl = PRO_SAS_SIGN_URL.arg( rootUrl.scheme(), rootUrl.host() );
  }
  else
  {
    signUrl = OPEN_SAS_SIGN_URL;
  }

  // We trim the vsicurl prefix from the uri before creating the url, we'll add it back after fetching the token if needed
  QString uri = connectionItems.constFirst();
  const bool isVsi = uri.startsWith( QLatin1String( "/vsicurl/" ) );
  if ( isVsi )
  {
    uri.remove( 0, 9 );
  }

  QUrl url( uri );
  const QString token( sasTokenForUrl( url, signUrl, authcfg ) );

  if ( !token.isEmpty() )
  {
    const QString query( url.query() );
    if ( query.isEmpty() )
      url.setQuery( token );
    else if ( !token.isEmpty() )
      url.setQuery( QStringLiteral( "%1&%2" ).arg( query, token ) );

    if ( isVsi )
      connectionItems.replace( 0, QStringLiteral( "/vsicurl/%1" ).arg( url.toString() ) );
    else
      connectionItems.replace( 0, url.toString() );
  }

  return true;
}

void QgsAuthPlanetaryComputerMethod::clearCachedConfig( const QString &authcfg )
{
  removeMethodConfig( authcfg );
}

void QgsAuthPlanetaryComputerMethod::updateMethodConfig( QgsAuthMethodConfig &config )
{
  Q_UNUSED( config );
  // NOTE: add updates as method version() increases due to config storage changes
}

QgsAuthMethodConfig QgsAuthPlanetaryComputerMethod::getMethodConfig( const QString &authcfg, bool fullconfig )
{
  const QMutexLocker locker( &mMutex );
  QgsAuthMethodConfig config;

  // check if it is cached
  if ( sAuthConfigCache.contains( authcfg ) )
  {
    config = sAuthConfigCache.value( authcfg );
    QgsDebugMsgLevel( QStringLiteral( "Retrieved config for authcfg: %1" ).arg( authcfg ), 2 );
    return config;
  }

  // else build basic bundle
  if ( !QgsApplication::authManager()->loadAuthenticationConfig( authcfg, config, fullconfig ) )
  {
    QgsDebugError( QStringLiteral( "Retrieve config FAILED for authcfg: %1" ).arg( authcfg ) );
    return QgsAuthMethodConfig();
  }

  // cache bundle
  putMethodConfig( authcfg, config );

  // initialize the oauth2 method if server is planetary computer pro
  if ( !mOauth2 && config.config( QStringLiteral( "serverType" ) ) == QLatin1String( "pro" ) )
    mOauth2.reset( QgsAuthMethodRegistry::instance()->createAuthMethod( QStringLiteral( "OAuth2" ) ) );

  return config;
}

void QgsAuthPlanetaryComputerMethod::putMethodConfig( const QString &authcfg, const QgsAuthMethodConfig &config )
{
  const QMutexLocker locker( &mMutex );
  QgsDebugMsgLevel( QStringLiteral( "Putting Planetary Computer config for authcfg: %1" ).arg( authcfg ), 2 );
  sAuthConfigCache.insert( authcfg, config );
}

void QgsAuthPlanetaryComputerMethod::removeMethodConfig( const QString &authcfg )
{
  const QMutexLocker locker( &mMutex );
  if ( sAuthConfigCache.contains( authcfg ) )
  {
    sAuthConfigCache.remove( authcfg );
    QgsDebugMsgLevel( QStringLiteral( "Removed Planetary Computer config for authcfg: %1" ).arg( authcfg ), 2 );
  }
}

QString QgsAuthPlanetaryComputerMethod::sasTokenForUrl( const QUrl &url, const QString &signUrl, const QString &authcfg )
{
  QString token;

  if ( !url.host().endsWith( BLOB_STORAGE_DOMAIN ) )
    return token;

  const QString account = url.host().remove( BLOB_STORAGE_DOMAIN );
  const QStringList path = url.path().split( '/', Qt::SkipEmptyParts );
  if ( path.isEmpty() )
    return token;

  const QString container = path.constFirst();

  SasToken sas = retrieveSasToken( account, container );

  if ( sas.isValid() )
    return sas.token;

  const QString requestUrl = signUrl + url.toString();
  QNetworkRequest request = QNetworkRequest( requestUrl );
  // for planetary computer pro we need to apply the oauth2 token first
  if ( mOauth2 )
  {
    mOauth2->updateNetworkRequest( request, authcfg );
  }
  QgsNetworkReplyContent content = QgsNetworkAccessManager::instance()->blockingGet( request );

  if ( content.error() != QNetworkReply::NoError )
  {
    QgsDebugError( QStringLiteral( "Error getting SAS token" ) );
    return token;
  }

  try
  {
    nlohmann::json j = nlohmann::json::parse( content.content() );
    // The collectionId signing endpoints return the plain SAS token
    if ( j.contains( "token" ) )
    {
      token = QString::fromStdString( j.at( "token" ) );
      sas.token = token;
      qDebug() << "Received token: " << token;
    }
    // The url signing endpoints return the complete SAS signed url, so we need to extract the token
    if ( j.contains( "href" ) )
    {
      QString href = QString::fromStdString( j.at( "href" ) );
      token = href.remove( url.toString() + '?' );
      sas.token = token;
      qDebug() << "Received token: " << token;
    }
    // All endpoints return the UTC expiry date
    if ( j.contains( "msft:expiry" ) )
    {
      const QString expiry = QString::fromStdString( j.at( "msft:expiry" ) );
      sas.expiry = QDateTime::fromString( expiry, Qt::ISODate );
      qDebug() << "Expires at: " << expiry;
    }
    if ( sas.isValid() )
      storeSasToken( account, container, sas );
  }
  catch ( nlohmann::json::exception &ex )
  {
    QgsDebugError( QStringLiteral( "Error parsing SAS token reply : %1" ).arg( ex.what() ) );
  }

  return token;
}

void QgsAuthPlanetaryComputerMethod::storeSasToken( const QString &account, const QString &container, const SasToken &token )
{
  const QMutexLocker locker( &mMutex );
  sSasTokensCache.insert( QStringLiteral( "%1/%2" ).arg( account, container ), token );
}

QgsAuthPlanetaryComputerMethod::SasToken QgsAuthPlanetaryComputerMethod::retrieveSasToken( const QString &account, const QString &container )
{
  const QMutexLocker locker( &mMutex );
  SasToken sas = sSasTokensCache.value( QStringLiteral( "%1/%2" ).arg( account, container ) );
  return sas;
}

#ifdef HAVE_GUI
QWidget *QgsAuthPlanetaryComputerMethod::editWidget( QWidget *parent ) const
{
  return new QgsAuthPlanetaryComputerEdit( parent );
}
#endif

//////////////////////////////////////////////
// Plugin externals
//////////////////////////////////////////////


#ifndef HAVE_STATIC_PROVIDERS
QGISEXTERN QgsAuthMethodMetadata *authMethodMetadataFactory()
{
  return new QgsAuthPlanetaryComputerMethodMetadata();
}
#endif
