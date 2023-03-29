/***************************************************************************
                         qgspointcloudsublayer.h
                         --------------------
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

#ifndef QGSPOINTCLOUDSUBLAYER_H
#define QGSPOINTCLOUDSUBLAYER_H

#include <memory>
#include <QString>
#include "qgsrectangle.h"

///@cond PRIVATE
#define SIP_NO_FILE

class QgsPointCloudIndex;

class QgsPointCloudSubLayer
{
  public:
    std::shared_ptr<QgsPointCloudIndex> index;
    QString uri;
    QgsRectangle extent;
    qint64 count;
};

///@endcond
#endif // QGSPOINTCLOUDSUBLAYER_H
