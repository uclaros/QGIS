/***************************************************************************
    qgsvectorfieldstreamlinesettings.h
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

#ifndef QGSVECTORFIELDSTREAMLINESETTINGS_H
#define QGSVECTORFIELDSTREAMLINESETTINGS_H

#include "qgis_core.h"

#include <QDomElement>

/**
 * \ingroup core
 *
 * \brief Represents a streamline renderer settings for vector datasets displayed by streamlines.
 *
 * \note The API is considered EXPERIMENTAL and can be changed without a notice
 *
 * \since QGIS 3.12
 */
class CORE_EXPORT QgsVectorFieldStreamlineSettings
{
  public:
    //! Method used to define start points that are used to draw streamlines
    enum class SeedingStartPointsMethod
    {

      /**
       * Seeds start points on the vertices mesh or user regular grid
       */
      MeshGridded = 0,

      /**
       * Seeds start points randomly on the mesh
       */
      Random
    };

    //! Returns the method used for seeding start points of strealines
    SeedingStartPointsMethod seedingMethod() const;
    //! Sets the method used for seeding start points of strealines
    void setSeedingMethod( const SeedingStartPointsMethod &seedingMethod );
    //! Returns the density used for seeding start points
    double seedingDensity() const;
    //! Sets the density used for seeding start points
    void setSeedingDensity( double seedingDensity );
    //! Reads configuration from the given DOM element
    void readXml( const QDomElement &elem );
    //! Writes configuration to a new DOM element
    QDomElement writeXml( QDomDocument &doc ) const;

  private:
    QgsVectorFieldStreamlineSettings::SeedingStartPointsMethod mSeedingMethod = SeedingStartPointsMethod::MeshGridded;
    double mSeedingDensity = 0.15;
};

#endif // QGSVECTORFIELDSTREAMLINESETTINGS_H
