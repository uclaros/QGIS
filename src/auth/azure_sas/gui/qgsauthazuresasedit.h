/***************************************************************************
    qgsauthazuresasedit.h
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

#ifndef QGSAUTHAZURESASEDIT_H
#define QGSAUTHAZURESASEDIT_H

#include <QWidget>
#include "qgsauthmethodedit.h"
#include "ui_qgsauthazuresasedit.h"

#include "qgsauthconfig.h"


class QgsAuthAzureSasEdit : public QgsAuthMethodEdit, private Ui::QgsAuthAzureSasEdit
{
    Q_OBJECT

  public:
    explicit QgsAuthAzureSasEdit( QWidget *parent = nullptr );

    bool validateConfig() override;

    QgsStringMap configMap() const override;

  public slots:
    void loadConfig( const QgsStringMap &configmap ) override;

    void resetConfig() override;

    void clearConfig() override;

  private slots:
    void addHeaderPair();

    void removeHeaderPair();

    void clearHeaderPairs();

    void populateHeaderPairs( const QgsStringMap &headerpairs, bool append = false );

    void headerTableSelectionChanged();

    void headerTableCellChanged( const int row, const int column );

  private:
    QgsStringMap mConfigMap;
    bool mValid = false;

    bool emptyHeadersKeysPresent();

    void addHeaderPairRow( const QString &key, const QString &val );

    QgsStringMap headerPairs() const;
};

#endif // QGSAUTHAZURESASEDIT_H
