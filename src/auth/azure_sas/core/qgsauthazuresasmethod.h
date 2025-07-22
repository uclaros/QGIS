/***************************************************************************
    qgsauthazuresasmethod.h
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

#ifndef QGSAUTHAZURESASMETHOD_H
#define QGSAUTHAZURESASMETHOD_H

#include <QObject>
#include <QMutex>

#include "qgsauthconfig.h"
#include "qgsauthmethod.h"
#include "qgsauthmethodmetadata.h"


class QgsAuthAzureSasMethod : public QgsAuthMethod
{
    Q_OBJECT

  public:
    static const QString AUTH_METHOD_KEY;
    static const QString AUTH_METHOD_DESCRIPTION;
    static const QString AUTH_METHOD_DISPLAY_DESCRIPTION;

    explicit QgsAuthAzureSasMethod();

    // QgsAuthMethod interface
    QString key() const override;

    QString description() const override;

    QString displayDescription() const override;

    bool updateNetworkRequest( QNetworkRequest &request, const QString &authcfg, const QString &dataprovider = QString() ) override;

    void clearCachedConfig( const QString &authcfg ) override;
    void updateMethodConfig( QgsAuthMethodConfig &config ) override;

#ifdef HAVE_GUI
    QWidget *editWidget( QWidget *parent ) const override;
#endif

  private:
    QgsAuthMethodConfig getMethodConfig( const QString &authcfg, bool fullconfig = true );

    void putMethodConfig( const QString &authcfg, const QgsAuthMethodConfig &config );

    void removeMethodConfig( const QString &authcfg );

    static QMap<QString, QgsAuthMethodConfig> sAuthConfigCache;
};


class QgsAuthAzureSasMethodMetadata : public QgsAuthMethodMetadata
{
  public:
    QgsAuthAzureSasMethodMetadata()
      : QgsAuthMethodMetadata( QgsAuthAzureSasMethod::AUTH_METHOD_KEY, QgsAuthAzureSasMethod::AUTH_METHOD_DESCRIPTION )
    {}
    QgsAuthAzureSasMethod *createAuthMethod() const override { return new QgsAuthAzureSasMethod; }
    //QStringList supportedDataProviders() const override;
};

#endif // QGSAUTHAZURESASMETHOD_H
