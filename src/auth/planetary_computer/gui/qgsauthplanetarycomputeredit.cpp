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
}


bool QgsAuthPlanetaryComputerEdit::validateConfig()
{
  const bool curvalid = !lineEdit->text().isEmpty();
  if ( mValid != curvalid )
  {
    mValid = curvalid;
    emit validityChanged( curvalid );
  }
  return curvalid;
}


QgsStringMap QgsAuthPlanetaryComputerEdit::configMap() const
{
  QgsStringMap config;
  config.insert( QStringLiteral( "clientId" ), lineEdit->text() );

  return config;
}


void QgsAuthPlanetaryComputerEdit::loadConfig( const QgsStringMap &configmap )
{
  clearConfig();

  mConfigMap = configmap;
  lineEdit->setText( configmap.value( QStringLiteral( "clientId" ) ) );

  validateConfig();
}


void QgsAuthPlanetaryComputerEdit::resetConfig()
{
  loadConfig( mConfigMap );
}


void QgsAuthPlanetaryComputerEdit::clearConfig()
{
  lineEdit->clear();
}
