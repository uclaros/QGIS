/***************************************************************************
    qgsauthplanetarycomputeredit.cpp
    ------------------------
    begin                : July 2025
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

#include "qgsauthplanetarycomputeredit.h"
#include "moc_qgsauthplanetarycomputeredit.cpp"
#include "ui_qgsauthplanetarycomputeredit.h"

#include "qgslogger.h"

QgsAuthPlanetaryComputerEdit::QgsAuthPlanetaryComputerEdit( QWidget *parent )
  : QgsAuthMethodEdit( parent )
{
  setupUi( this );

  connect( cbType, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsAuthPlanetaryComputerEdit::updateServerType ); // also updates GUI
  connect( leRootUrl, &QLineEdit::textChanged, this, [this] { validateConfig(); } );
  connect( leClientId, &QLineEdit::textChanged, this, [this] { validateConfig(); } );
}


bool QgsAuthPlanetaryComputerEdit::validateConfig()
{
  bool curvalid = true;
  if ( cbType->currentIndex() == 1 )
  {
    curvalid = !leClientId->text().isEmpty() && !leRootUrl->text().isEmpty();
  }
  if ( mValid != curvalid )
  {
    mValid = curvalid;
    emit validityChanged( curvalid );
  }
  return curvalid;
}


QgsStringMap QgsAuthPlanetaryComputerEdit::configMap() const
{
  const bool isPro = cbType->currentIndex() == 1;
  const QString clientId = leClientId->text();
  const QString rootUrl = leRootUrl->text();
  const QString tenantId = leTenantId->text();

  QgsStringMap config;

  if ( isPro )
  {
    const QString requestUrl = QStringLiteral( "https://login.microsoftonline.com/%1/oauth2/v2.0/authorize" ).arg( tenantId.isEmpty() ? QStringLiteral( "organizations" ) : tenantId );
    const QString tokenUrl = QStringLiteral( "https://login.microsoftonline.com/%1/oauth2/v2.0/token" ).arg( tenantId.isEmpty() ? QStringLiteral( "organizations" ) : tenantId );
    const QString json = QStringLiteral(
                           "{"
                           "\"accessMethod\": 0,"
                           "\"apiKey\": null,"
                           "\"clientId\": \"%1\","
                           "\"clientSecret\": null,"
                           "\"configType\": 1,"
                           "\"customHeader\": null,"
                           "\"description\": \"\","
                           "\"extraTokens\": {},"
                           "\"grantFlow\": 3,"
                           "\"id\": null,"
                           "\"name\": null,"
                           "\"objectName\": \"\","
                           "\"password\": null,"
                           "\"persistToken\": false,"
                           "\"queryPairs\": {},"
                           "\"redirectHost\": \"127.0.0.1\","
                           "\"redirectPort\": 7070,"
                           "\"redirectUrl\": null,"
                           "\"refreshTokenUrl\": null,"
                           "\"requestTimeout\": 30,"
                           "\"requestUrl\": \"%2\","
                           "\"scope\": \"https://geocatalog.spatio.azure.com/.default\","
                           "\"tokenUrl\": \"%3\","
                           "\"username\": null,"
                           "\"version\": 1"
                           "}"
    )
                           .arg( clientId, requestUrl, tokenUrl );


    config.insert( QStringLiteral( "oauth2config" ), json );
    config.insert( QStringLiteral( "serverType" ), QStringLiteral( "pro" ) );
    config.insert( QStringLiteral( "clientId" ), clientId );
    config.insert( QStringLiteral( "rootUrl" ), rootUrl );
    config.insert( QStringLiteral( "tenantId" ), tenantId );
  }
  else
  {
    config.insert( QStringLiteral( "serverType" ), QStringLiteral( "public" ) );
  }
  return config;
}


void QgsAuthPlanetaryComputerEdit::loadConfig( const QgsStringMap &configmap )
{
  clearConfig();

  mConfigMap = configmap;
  updateServerType( configmap.value( QStringLiteral( "serverType" ) ) == QLatin1String( "pro" ) ? 1 : 0 );
  leClientId->setText( configmap.value( QStringLiteral( "clientId" ) ) );
  leRootUrl->setText( configmap.value( QStringLiteral( "rootUrl" ) ) );
  leTenantId->setText( configmap.value( QStringLiteral( "tenantId" ) ) );

  validateConfig();
}


void QgsAuthPlanetaryComputerEdit::resetConfig()
{
  loadConfig( mConfigMap );
}


void QgsAuthPlanetaryComputerEdit::clearConfig()
{
  leClientId->clear();
  leRootUrl->clear();
  leTenantId->clear();
}


void QgsAuthPlanetaryComputerEdit::updateServerType( int indx )
{
  if ( cbType->currentIndex() != indx )
  {
    whileBlocking( cbType )->setCurrentIndex( indx );
  }

  const bool isPro = indx == 1;

  lblRootUrl->setVisible( isPro );
  leRootUrl->setVisible( isPro );
  lblClientId->setVisible( isPro );
  leClientId->setVisible( isPro );
  lblTenantId->setVisible( isPro );
  leTenantId->setVisible( isPro );

  validateConfig();
}
