/***************************************************************************
    qgsgeometrycheck.h
    ---------------------
    begin                : September 2014
    copyright            : (C) 2014 by Sandro Mani / Sourcepole AG
    email                : smani at sourcepole dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGS_GEOMETRY_CHECK_H
#define QGS_GEOMETRY_CHECK_H

#include <QApplication>
#include <limits>
#include <QStringList>
#include <QPointer>

#include "qgis_analysis.h"
#include "qgsfeature.h"
#include "qgsvectorlayer.h"
#include "geometry/qgsgeometry.h"
#include "qgsgeometrycheckerutils.h"
#include "qgssettings.h"

class QgsGeometryCheckError;
class QgsFeaturePool;

/**
 * \ingroup analysis
 * This class manages all known geometry check factories.
 *
 * QgsGeometryCheckRegistry is not usually directly created, but rather accessed through
 * QgsAnalysis::geometryCheckRegistry().
 *
 * \note This class is a technology preview and unstable API.
 * \since QGIS 3.4
 */
class ANALYSIS_EXPORT QgsGeometryCheck
{
    Q_GADGET
    Q_DECLARE_TR_FUNCTIONS( QgsGeometryCheck )

  public:

    /**
     * A list of layers and feature ids for each of these layers.
     * In C++, the member `ids` can be accessed directly.
     * In Python some accessor methods will need to be written.
     *
     * \since QGIS 3.4
     */
    struct ANALYSIS_EXPORT LayerFeatureIds
    {
      LayerFeatureIds() = default;
      LayerFeatureIds( const QMap<QString, QgsFeatureIds> &ids ) SIP_SKIP;

      QMap<QString, QgsFeatureIds> ids SIP_SKIP;

#ifndef SIP_RUN
      QMap<QString, QgsFeatureIds> toMap() const
      {
        return ids;
      }

      bool isEmpty() const
      {
        return ids.isEmpty();
      }
#endif
    };

    /**
     * Description of a change to indicate at which level a change occurred.
     *
     * \since Python bindings since QGIS 3.4
     */
    enum ChangeWhat
    {
      ChangeFeature, //!< This change happens on feature level
      ChangePart,    //!< This change happens on part level
      ChangeRing,    //!< This change happens on ring level
      ChangeNode     //!< This change happens on node level
    };

    /**
     * Description of the type of a change.
     *
     * \since Python bindings since QGIS 3.4
     */
    enum ChangeType
    {
      ChangeAdded,   //!< Something has been added
      ChangeRemoved, //!< Something has been removed
      ChangeChanged  //!< Something has been updated
    };

    /**
     * The type of a check.
     *
     * \since Python bindings since QGIS 3.4
     */
    enum CheckType
    {
      FeatureNodeCheck, //!< The check controls individual nodes
      FeatureCheck,     //!< The check controls geometries as a whole
      LayerCheck        //!< The check controls a whole layer (topology checks)
    };

    /**
     * Flags for geometry checks.
     */
    enum Flag
    {
      AvailableInValidation = 1 << 1 //!< This geometry check should be available in layer validation on the vector layer peroperties
    };
    Q_DECLARE_FLAGS( Flags, Flag )
    Q_FLAG( Flags )

    /**
     * Descripts a change to fix a geometry.
     *
     * \since Python bindings since QGIS 3.4
     */
    struct Change
    {
      Change() = default;

      /**
       * Create a new Change
       */
      Change( QgsGeometryCheck::ChangeWhat _what, QgsGeometryCheck::ChangeType _type, QgsVertexId _vidx = QgsVertexId() )
        : what( _what )
        , type( _type )
        , vidx( _vidx )
      {}

      /**
       * What level this change affects.
       */
      QgsGeometryCheck::ChangeWhat what;

      /**
       * What action this change performs.
       */
      QgsGeometryCheck::ChangeType type;

      /**
       * The index of the part / ring / vertex, depending on \see what.
       */
      QgsVertexId vidx;
      bool operator==( const QgsGeometryCheck::Change &other )
      {
        return what == other.what && type == other.type && vidx == other.vidx;
      }
    };

    /**
     * A collection of changes.
     * Grouped by layer id and feature id.
     */
    typedef QMap<QString, QMap<QgsFeatureId, QList<QgsGeometryCheck::Change> > > Changes;

    /**
     * Create a new geometry check.
     */
    QgsGeometryCheck( const QgsGeometryCheckContext *context, const QVariantMap &configuration );
    virtual ~QgsGeometryCheck() = default;

#ifndef SIP_RUN

    /**
     * Returns the configuration value with the \a name, saved in the QGIS settings for
     * this geometry check. If no configuration could be found, \a defaultValue is returned.
     */
    template <class T>
    T configurationValue( const QString &name, const QVariant &defaultValue = QVariant() )
    {
      return mConfiguration.value( name, QgsSettings().value( "/geometry_checker/" + id() + "/" + name, defaultValue ) ).value<T>();
    }
#endif

    /**
     * Returns if this geometry check is compatible with \a layer.
     * By default it checks for the geometry type in \a compatibleGeometryTypes().
     *
     * \since QGIS 3.4
     */
    virtual bool isCompatible( QgsVectorLayer *layer ) const;

    /**
     * A list of geometry types for which this check can be performed.
     *
     * \since QGIS 3.4
     */
    virtual QList<QgsWkbTypes::GeometryType> compatibleGeometryTypes() const = 0;

    /**
     * Flags for this geometry check.
     */
    virtual QgsGeometryCheck::Flags flags() const;

    /**
     * The main worker method.
     * Check all features available from \a featurePools and write errors found to \a errors.
     * Other status messages can be written to \a messages.
     * Progress should be reported to \a feedback. Only features and layers listed in \a ids should be checked.
     *
     * \since QGIS 3.4
     */
    virtual void collectErrors( const QMap<QString, QgsFeaturePool *> &featurePools, QList<QgsGeometryCheckError *> &errors SIP_INOUT, QStringList &messages SIP_INOUT, QgsFeedback *feedback, const LayerFeatureIds &ids = QgsGeometryCheck::LayerFeatureIds() ) const = 0;

    /**
     * Fix the error \a error with the specified \a method.
     *
     * \since QGIS 3.4
     */
    virtual void fixError( const QMap<QString, QgsFeaturePool *> &featurePools, QgsGeometryCheckError *error, int method, const QMap<QString, int> &mergeAttributeIndices, Changes &changes SIP_INOUT ) const SIP_SKIP;

    /**
     * Returns a list of descriptions for available resolutions for errors. The index will be passed as ``method`` to \see fixError().
     *
     * \since QGIS 3.4
     */
    virtual QStringList resolutionMethods() const = 0;

    /**
     * Returns a human readable description for this check.
     *
     * \since QGIS 3.4
     */
    virtual QString description() const = 0;

    /**
     * Returns an id for this check.
     *
     * \since QGIS 3.4
     */
    virtual QString id() const = 0;

    /**
     * Returns the check type.
     *
     * \since QGIS 3.4
     */
    virtual CheckType checkType() const = 0;

    /**
     * Returns the context
     *
     * \since QGIS 3.4
     */
    const QgsGeometryCheckContext *context() const { return mContext; }

  protected:

    /**
     * Returns all layers and feature ids.
     *
     * \note Not available in Python bindings
     * \since QGIS 3.4
     */
    QMap<QString, QgsFeatureIds> allLayerFeatureIds( const QMap<QString, QgsFeaturePool *> &featurePools ) const SIP_SKIP;

    /**
     * Replaces a part in a feature geometry.
     *
     * \note Not available in Python bindings
     * \since QGIS 3.4
     */
    void replaceFeatureGeometryPart( const QMap<QString, QgsFeaturePool *> &featurePools, const QString &layerId, QgsFeature &feature, int partIdx, QgsAbstractGeometry *newPartGeom, Changes &changes ) const SIP_SKIP;

    /**
     * Deletes a part of a feature geometry.
     *
     * \note Not available in Python bindings
     * \since QGIS 3.4
     */
    void deleteFeatureGeometryPart( const QMap<QString, QgsFeaturePool *> &featurePools, const QString &layerId, QgsFeature &feature, int partIdx, Changes &changes ) const SIP_SKIP;

    /**
     * Deletes a ring in a feature geometry.
     *
     * \note Not available in Python bindings
     * \since QGIS 3.4
     */
    void deleteFeatureGeometryRing( const QMap<QString, QgsFeaturePool *> &featurePools, const QString &layerId, QgsFeature &feature, int partIdx, int ringIdx, Changes &changes ) const SIP_SKIP;

    const QgsGeometryCheckContext *mContext;
    QVariantMap mConfiguration;

    /**
     * Determines the scale factor of a layer to the map coordinate reference system.
     *
     * \note Not available in Python bindings
     * \since QGIS 3.4
     */
    double scaleFactor( const QPointer<QgsVectorLayer> &layer ) const SIP_SKIP;
};

#endif // QGS_GEOMETRY_CHECK_H
