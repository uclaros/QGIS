/***************************************************************************
    qgsauthplanetarycomputeredit.h
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

#ifndef QGSAUTHPLANETARYCOMPUTEREDIT_H
#define QGSAUTHPLANETARYCOMPUTEREDIT_H

#include <QWidget>
#include "qgsauthmethodedit.h"
#include "ui_qgsauthplanetarycomputeredit.h"

#include "qgsauthconfig.h"


class QgsAuthPlanetaryComputerEdit : public QgsAuthMethodEdit, private Ui::QgsAuthPlanetaryComputerEdit
{
    Q_OBJECT

  public:
    explicit QgsAuthPlanetaryComputerEdit( QWidget *parent = nullptr );

    bool validateConfig() override;

    QgsStringMap configMap() const override;

  public slots:
    void loadConfig( const QgsStringMap &configmap ) override;

    void resetConfig() override;

    void clearConfig() override;

  private:
    QgsStringMap mConfigMap;
    bool mValid = false;
};

#endif // QGSAUTHPLANETARYCOMPUTEREDIT_H
