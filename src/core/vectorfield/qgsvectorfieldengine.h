/***************************************************************************
    qgsvectorfieldengine.h
    ---------------------
    begin                : September 2026
    copyright            : (C) 2026 by Stefanos Natsis
    email                : uclaros at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSVECTORFIELDENGINE_H
#define QGSVECTORFIELDENGINE_H

#include <memory>

#include "qgsvectorfieldsettings.h"

#include <QSize>

#define SIP_NO_FILE

class QgsCoordinateTransform;
class QgsPointXY;
class QgsRenderContext;
class QgsScopedQPainterState;

class QgsVectorFieldEngine
{
  public:
    QgsVectorFieldEngine( double datasetMagMaximumValue, double datasetMagMinimumValue, const QgsVectorFieldSettings &settings, QgsRenderContext &context, QSize size );

    void drawArrow( const QgsPointXY &lineStart, double xVal, double yVal, double magnitude );
    void drawWindBarb( const QgsPointXY &lineStart, double xVal, double yVal, double magnitude );

  private:
    //! Calculates the end point of the arrow based on start point and vector data
    bool calcVectorLineEnd(
      QgsPointXY &lineEnd,
      double &vectorLength,
      double &cosAlpha,
      double &sinAlpha, //out
      const QgsPointXY &lineStart,
      double xVal,
      double yVal,
      double magnitude //in
    );

    const double mMinMag = 0.0;
    const double mMaxMag = 0.0;

    QgsRenderContext &mContext;
    const QgsVectorFieldSettings mCfg;
    QgsInterpolatedLineColor mVectorColoring;

    QSize mOutputSize;

    std::unique_ptr<QgsCoordinateTransform> mGeographicTransform;
    std::unique_ptr<QgsScopedQPainterState> mScopedPainterState;
};

#endif // QGSVECTORFIELDENGINE_H
