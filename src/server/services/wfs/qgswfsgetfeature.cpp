/***************************************************************************
                              qgswfsgetfeature.cpp
                              -------------------------
  begin                : December 20 , 2016
  copyright            : (C) 2007 by Marco Hugentobler  (original code)
                         (C) 2012 by René-Luc D'Hont    (original code)
                         (C) 2014 by Alessandro Pasotti (original code)
                         (C) 2017 by David Marteau
  email                : marco dot hugentobler at karto dot baug dot ethz dot ch
                         a dot pasotti at itopen dot it
                         david dot marteau at 3liz dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgswfsutils.h"
#include "qgsserverprojectutils.h"
#include "qgsfields.h"
#include "qgsfieldformatterregistry.h"
#include "qgsfieldformatter.h"
#include "qgsdatetimefieldformatter.h"
#include "qgsexpression.h"
#include "qgsgeometry.h"
#include "qgsmaplayer.h"
#include "qgsfeatureiterator.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
#include "qgsfilterrestorer.h"
#include "qgsproject.h"
#include "qgsogcutils.h"
#include "qgsjsonutils.h"

#include "qgswfsgetfeature.h"

#include <QStringList>

namespace QgsWfs
{

  namespace
  {
    struct createFeatureParams
    {
      int precision;

      const QgsCoordinateReferenceSystem &crs;

      const QgsAttributeList &attributeIndexes;

      const QString &typeName;

      bool withGeom;

      const QString &geometryName;

      const QgsCoordinateReferenceSystem &outputCrs;
    };

    QString createFeatureGeoJSON( QgsFeature *feat, const createFeatureParams &params );

    QString encodeValueToText( const QVariant &value, const QgsEditorWidgetSetup &setup );

    QDomElement createFeatureGML2( QgsFeature *feat, QDomDocument &doc, const createFeatureParams &params, const QgsProject *project );

    QDomElement createFeatureGML3( QgsFeature *feat, QDomDocument &doc, const createFeatureParams &params, const QgsProject *project );

    void hitGetFeature( const QgsServerRequest &request, QgsServerResponse &response, const QgsProject *project,
                        QgsWfsParameters::Format format, int numberOfFeatures, const QStringList &typeNames );

    void startGetFeature( const QgsServerRequest &request, QgsServerResponse &response, const QgsProject *project,
                          QgsWfsParameters::Format format, int prec, QgsCoordinateReferenceSystem &crs,
                          QgsRectangle *rect, const QStringList &typeNames );

    void setGetFeature( QgsServerResponse &response, QgsWfsParameters::Format format, QgsFeature *feat, int featIdx,
                        const createFeatureParams &params, const QgsProject *project );

    void endGetFeature( QgsServerResponse &response, QgsWfsParameters::Format format );

    QgsServerRequest::Parameters mRequestParameters;
    QgsWfsParameters mWfsParameters;
    /* GeoJSON Exporter */
    QgsJsonExporter mJsonExporter;
  }

  void writeGetFeature( QgsServerInterface *serverIface, const QgsProject *project,
                        const QString &version, const QgsServerRequest &request,
                        QgsServerResponse &response )
  {
    Q_UNUSED( version );

    mRequestParameters = request.parameters();
    mWfsParameters = QgsWfsParameters( QUrlQuery( request.url() ) );
    mWfsParameters.dump();
    getFeatureRequest aRequest;

    QDomDocument doc;
    QString errorMsg;

    if ( doc.setContent( mRequestParameters.value( QStringLiteral( "REQUEST_BODY" ) ), true, &errorMsg ) )
    {
      QDomElement docElem = doc.documentElement();
      aRequest = parseGetFeatureRequestBody( docElem, project );
    }
    else
    {
      aRequest = parseGetFeatureParameters( project );
    }

    // store typeName
    QStringList typeNameList;

    // Request metadata
    bool onlyOneLayer = ( aRequest.queries.size() == 1 );
    QgsRectangle requestRect;
    QgsCoordinateReferenceSystem requestCrs;
    int requestPrecision = 6;
    if ( !onlyOneLayer )
      requestCrs = QgsCoordinateReferenceSystem( 4326, QgsCoordinateReferenceSystem::EpsgCrsId );

    QList<getFeatureQuery>::iterator qIt = aRequest.queries.begin();
    for ( ; qIt != aRequest.queries.end(); ++qIt )
    {
      typeNameList << ( *qIt ).typeName;
    }

    // get layers and
    // update the request metadata
    QStringList wfsLayerIds = QgsServerProjectUtils::wfsLayerIds( *project );
    QMap<QString, QgsMapLayer *> mapLayerMap;
    for ( int i = 0; i < wfsLayerIds.size(); ++i )
    {
      QgsMapLayer *layer = project->mapLayer( wfsLayerIds.at( i ) );
      if ( !layer )
      {
        continue;
      }
      if ( layer->type() != QgsMapLayer::LayerType::VectorLayer )
      {
        continue;
      }

      QString name = layerTypeName( layer );

      if ( typeNameList.contains( name ) )
      {
        // store layers
        mapLayerMap[name] = layer;
        // update request metadata
        if ( onlyOneLayer )
        {
          requestRect = layer->extent();
          requestCrs = layer->crs();
        }
        else
        {
          QgsCoordinateTransform transform( layer->crs(), requestCrs, project );
          try
          {
            if ( requestRect.isEmpty() )
            {
              requestRect = transform.transform( layer->extent() );
            }
            else
            {
              requestRect.combineExtentWith( transform.transform( layer->extent() ) );
            }
          }
          catch ( QgsException &cse )
          {
            Q_UNUSED( cse );
            requestRect = QgsRectangle( -180.0, -90.0, 180.0, 90.0 );
          }
        }
      }
    }

    QgsAccessControl *accessControl = serverIface->accessControls();

    //scoped pointer to restore all original layer filters (subsetStrings) when pointer goes out of scope
    //there's LOTS of potential exit paths here, so we avoid having to restore the filters manually
    std::unique_ptr< QgsOWSServerFilterRestorer > filterRestorer( new QgsOWSServerFilterRestorer() );

    // features counters
    long sentFeatures = 0;
    long iteratedFeatures = 0;
    // sent features
    QgsFeature feature;
    qIt = aRequest.queries.begin();
    for ( ; qIt != aRequest.queries.end(); ++qIt )
    {
      getFeatureQuery &query = *qIt;
      QString typeName = query.typeName;

      if ( !mapLayerMap.keys().contains( typeName ) )
      {
        throw QgsRequestNotWellFormedException( QStringLiteral( "TypeName '%1' unknown" ).arg( typeName ) );
      }

      QgsMapLayer *layer = mapLayerMap[typeName];
      if ( accessControl && !accessControl->layerReadPermission( layer ) )
      {
        throw QgsSecurityAccessException( QStringLiteral( "Feature access permission denied" ) );
      }

      QgsVectorLayer *vlayer = qobject_cast<QgsVectorLayer *>( layer );
      if ( !vlayer )
      {
        throw QgsRequestNotWellFormedException( QStringLiteral( "TypeName '%1' layer error" ).arg( typeName ) );
      }

      //test provider
      QgsVectorDataProvider *provider = vlayer->dataProvider();
      if ( !provider )
      {
        throw QgsRequestNotWellFormedException( QStringLiteral( "TypeName '%1' layer's provider error" ).arg( typeName ) );
      }

      if ( accessControl )
      {
        QgsOWSServerFilterRestorer::applyAccessControlLayerFilters( accessControl, vlayer, filterRestorer->originalFilters() );
      }

      //is there alias info for this vector layer?
      QMap< int, QString > layerAliasInfo;
      QgsStringMap aliasMap = vlayer->attributeAliases();
      QgsStringMap::const_iterator aliasIt = aliasMap.constBegin();
      for ( ; aliasIt != aliasMap.constEnd(); ++aliasIt )
      {
        int attrIndex = vlayer->fields().lookupField( aliasIt.key() );
        if ( attrIndex != -1 )
        {
          layerAliasInfo.insert( attrIndex, aliasIt.value() );
        }
      }

      // get propertyList from query
      QStringList propertyList = query.propertyList;

      //Using pending attributes and pending fields
      QgsAttributeList attrIndexes = vlayer->attributeList();
      QgsFields fields = vlayer->fields();
      bool withGeom = true;
      if ( !propertyList.isEmpty() && propertyList.first() != QStringLiteral( "*" ) )
      {
        withGeom = false;
        QStringList::const_iterator plstIt;
        QList<int> idxList;
        // build corresponding propertyname
        QList<QString> propertynames;
        QList<QString> fieldnames;
        for ( int idx = 0; idx < fields.count(); ++idx )
        {
          fieldnames.append( fields[idx].name() );
          propertynames.append( fields.field( idx ).name().replace( ' ', '_' ).replace( cleanTagNameRegExp, QString() ) );
        }
        QString fieldName;
        for ( plstIt = propertyList.begin(); plstIt != propertyList.end(); ++plstIt )
        {
          fieldName = *plstIt;
          int fieldNameIdx = propertynames.indexOf( fieldName );
          if ( fieldNameIdx == -1 )
          {
            fieldNameIdx = fieldnames.indexOf( fieldName );
          }
          if ( fieldNameIdx > -1 )
          {
            idxList.append( fieldNameIdx );
          }
          else if ( fieldName == QStringLiteral( "geometry" ) )
          {
            withGeom = true;
          }
        }
        if ( !idxList.isEmpty() )
        {
          attrIndexes = idxList;
        }
      }

      //excluded attributes for this layer
      const QSet<QString> &layerExcludedAttributes = vlayer->excludeAttributesWfs();
      if ( !attrIndexes.isEmpty() && !layerExcludedAttributes.isEmpty() )
      {
        foreach ( const QString &excludedAttribute, layerExcludedAttributes )
        {
          int fieldNameIdx = fields.indexOf( excludedAttribute );
          if ( fieldNameIdx > -1 && attrIndexes.contains( fieldNameIdx ) )
          {
            attrIndexes.removeOne( fieldNameIdx );
          }
        }
      }


      // update request
      QgsFeatureRequest featureRequest = query.featureRequest;

      // expression context
      QgsExpressionContext expressionContext;
      expressionContext << QgsExpressionContextUtils::globalScope()
                        << QgsExpressionContextUtils::projectScope( project )
                        << QgsExpressionContextUtils::layerScope( vlayer );
      featureRequest.setExpressionContext( expressionContext );

      // geometry flags
      if ( vlayer->wkbType() == QgsWkbTypes::NoGeometry )
        featureRequest.setFlags( featureRequest.flags() | QgsFeatureRequest::NoGeometry );
      else
        featureRequest.setFlags( featureRequest.flags() | ( withGeom ? QgsFeatureRequest::NoFlags : QgsFeatureRequest::NoGeometry ) );
      // subset of attributes
      featureRequest.setSubsetOfAttributes( attrIndexes );

      if ( accessControl )
      {
        accessControl->filterFeatures( vlayer, featureRequest );

        QStringList attributes = QStringList();
        for ( int idx : attrIndexes )
        {
          attributes.append( vlayer->fields().field( idx ).name() );
        }
        featureRequest.setSubsetOfAttributes(
          accessControl->layerAttributes( vlayer, attributes ),
          vlayer->fields() );
      }

      if ( onlyOneLayer )
      {
        requestPrecision = QgsServerProjectUtils::wfsLayerPrecision( *project, vlayer->id() );
      }

      if ( aRequest.maxFeatures > 0 )
      {
        featureRequest.setLimit( aRequest.maxFeatures + aRequest.startIndex - sentFeatures );
      }
      // specific layer precision
      int layerPrecision = QgsServerProjectUtils::wfsLayerPrecision( *project, vlayer->id() );
      // specific layer crs
      QgsCoordinateReferenceSystem layerCrs = vlayer->crs();

      // Geometry name
      QString geometryName = aRequest.geometryName;
      if ( !withGeom )
      {
        geometryName = QLatin1String( "NONE" );
      }
      // outputCrs
      QgsCoordinateReferenceSystem outputCrs = vlayer->crs();
      if ( !query.srsName.isEmpty() )
      {
        outputCrs = QgsCoordinateReferenceSystem::fromOgcWmsCrs( query.srsName );
      }

      if ( !featureRequest.filterRect().isEmpty() )
      {
        QgsCoordinateTransform transform( outputCrs, vlayer->crs(), project );
        try
        {
          featureRequest.setFilterRect( transform.transform( featureRequest.filterRect() ) );
        }
        catch ( QgsException &cse )
        {
          Q_UNUSED( cse );
        }
        if ( onlyOneLayer )
        {
          requestRect = featureRequest.filterRect();
        }
      }

      // Iterate through features
      QgsFeatureIterator fit = vlayer->getFeatures( featureRequest );

      if ( mWfsParameters.resultType() == QgsWfsParameters::ResultType::HITS )
      {
        while ( fit.nextFeature( feature ) && ( aRequest.maxFeatures == -1 || sentFeatures < aRequest.maxFeatures ) )
        {
          if ( iteratedFeatures >= aRequest.startIndex )
          {
            ++sentFeatures;
          }
          ++iteratedFeatures;
        }
      }
      else
      {
        const createFeatureParams cfp = { layerPrecision,
                                          layerCrs,
                                          attrIndexes,
                                          typeName,
                                          withGeom,
                                          geometryName,
                                          outputCrs
                                        };
        while ( fit.nextFeature( feature ) && ( aRequest.maxFeatures == -1 || sentFeatures < aRequest.maxFeatures ) )
        {
          if ( iteratedFeatures == aRequest.startIndex )
            startGetFeature( request, response, project, aRequest.outputFormat, requestPrecision, requestCrs, &requestRect, typeNameList );

          if ( iteratedFeatures >= aRequest.startIndex )
          {
            setGetFeature( response, aRequest.outputFormat, &feature, sentFeatures, cfp, project );
            ++sentFeatures;
          }
          ++iteratedFeatures;
        }
      }
    }

#ifdef HAVE_SERVER_PYTHON_PLUGINS
    //force restoration of original layer filters
    filterRestorer.reset();
#endif

    if ( mWfsParameters.resultType() == QgsWfsParameters::ResultType::HITS )
    {
      hitGetFeature( request, response, project, aRequest.outputFormat, sentFeatures, typeNameList );
    }
    else
    {
      // End of GetFeature
      if ( iteratedFeatures <= aRequest.startIndex )
        startGetFeature( request, response, project, aRequest.outputFormat, requestPrecision, requestCrs, &requestRect, typeNameList );
      endGetFeature( response, aRequest.outputFormat );
    }

  }

  getFeatureRequest parseGetFeatureParameters( const QgsProject *project )
  {
    getFeatureRequest request;
    request.maxFeatures = mWfsParameters.maxFeaturesAsInt();;
    request.startIndex = mWfsParameters.startIndexAsInt();
    request.outputFormat = mWfsParameters.outputFormat();

    // Verifying parameters mutually exclusive
    QStringList fidList = mWfsParameters.featureIds();
    bool paramContainsFeatureIds = !fidList.isEmpty();
    QStringList filterList = mWfsParameters.filters();
    bool paramContainsFilters = !filterList.isEmpty();
    QString bbox = mWfsParameters.bbox();
    bool paramContainsBbox = !bbox.isEmpty();
    if ( ( paramContainsFeatureIds
           && ( paramContainsFilters || paramContainsBbox ) )
         || ( paramContainsFilters
              && ( paramContainsFeatureIds || paramContainsBbox ) )
         || ( paramContainsBbox
              && ( paramContainsFeatureIds || paramContainsFilters ) )
       )
    {
      throw QgsRequestNotWellFormedException( QStringLiteral( "FEATUREID FILTER and BBOX parameters are mutually exclusive" ) );
    }

    // Get and split PROPERTYNAME parameter
    QStringList propertyNameList = mWfsParameters.propertyNames();

    // Manage extra parameter GeometryName
    request.geometryName = mWfsParameters.geometryNameAsString().toUpper();

    QStringList typeNameList;
    // parse FEATUREID
    if ( paramContainsFeatureIds )
    {
      // Verifying the 1:1 mapping between FEATUREID and PROPERTYNAME
      if ( !propertyNameList.isEmpty() && propertyNameList.size() != fidList.size() )
      {
        throw QgsRequestNotWellFormedException( QStringLiteral( "There has to be a 1:1 mapping between each element in a FEATUREID and the PROPERTYNAME list" ) );
      }
      if ( propertyNameList.isEmpty() )
      {
        for ( int i = 0; i < fidList.size(); ++i )
        {
          propertyNameList << QStringLiteral( "*" );
        }
      }

      QMap<QString, QgsFeatureIds> fidsMap;

      QStringList::const_iterator fidIt = fidList.constBegin();
      QStringList::const_iterator propertyNameIt = propertyNameList.constBegin();
      for ( ; fidIt != fidList.constEnd(); ++fidIt )
      {
        // Get FeatureID
        QString fid = *fidIt;
        fid = fid.trimmed();
        // Get PropertyName for this FeatureID
        QString propertyName;
        if ( propertyNameIt != propertyNameList.constEnd() )
        {
          propertyName = *propertyNameIt;
        }
        // testing typename in the WFS featureID
        if ( !fid.contains( '.' ) )
        {
          throw QgsRequestNotWellFormedException( QStringLiteral( "FEATUREID has to have TYPENAME in the values" ) );
        }

        QString typeName = fid.section( '.', 0, 0 );
        fid = fid.section( '.', 1, 1 );
        if ( !typeNameList.contains( typeName ) )
        {
          typeNameList << typeName;
        }

        // each Feature requested by FEATUREID can have each own property list
        QString key = QStringLiteral( "%1(%2)" ).arg( typeName ).arg( propertyName );
        QgsFeatureIds fids;
        if ( fidsMap.contains( key ) )
        {
          fids = fidsMap.value( key );
        }
        fids.insert( fid.toInt() );
        fidsMap.insert( key, fids );

        if ( propertyNameIt != propertyNameList.constEnd() )
        {
          ++propertyNameIt;
        }
      }

      QMap<QString, QgsFeatureIds>::const_iterator fidsMapIt = fidsMap.constBegin();
      while ( fidsMapIt != fidsMap.constEnd() )
      {
        QString key = fidsMapIt.key();

        //Extract TypeName and PropertyName from key
        QRegExp rx( "([^()]+)\\(([^()]+)\\)" );
        if ( rx.indexIn( key, 0 ) == -1 )
        {
          throw QgsRequestNotWellFormedException( QStringLiteral( "Error getting properties for FEATUREID" ) );
        }
        QString typeName = rx.cap( 1 );
        QString propertyName = rx.cap( 2 );

        getFeatureQuery query;
        query.typeName = typeName;
        query.srsName = mWfsParameters.srsName();

        // Parse PropertyName
        if ( propertyName != QStringLiteral( "*" ) )
        {
          QStringList propertyList;

          QStringList attrList = propertyName.split( ',' );
          QStringList::const_iterator alstIt;
          for ( alstIt = attrList.begin(); alstIt != attrList.end(); ++alstIt )
          {
            QString fieldName = *alstIt;
            fieldName = fieldName.trimmed();
            if ( fieldName.contains( ':' ) )
            {
              fieldName = fieldName.section( ':', 1, 1 );
            }
            if ( fieldName.contains( '/' ) )
            {
              if ( fieldName.section( '/', 0, 0 ) != typeName )
              {
                throw QgsRequestNotWellFormedException( QStringLiteral( "PropertyName text '%1' has to contain TypeName '%2'" ).arg( fieldName ).arg( typeName ) );
              }
              fieldName = fieldName.section( '/', 1, 1 );
            }
            propertyList.append( fieldName );
          }
          query.propertyList = propertyList;
        }

        QgsFeatureIds fids = fidsMapIt.value();
        QgsFeatureRequest featureRequest( fids );

        query.featureRequest = featureRequest;
        request.queries.append( query );
        fidsMapIt++;
      }
      return request;
    }

    if ( !mRequestParameters.contains( QStringLiteral( "TYPENAME" ) ) )
    {
      throw QgsRequestNotWellFormedException( QStringLiteral( "TYPENAME is mandatory except if FEATUREID is used" ) );
    }

    typeNameList = mWfsParameters.typeNames();
    // Verifying the 1:1 mapping between TYPENAME and PROPERTYNAME
    if ( !propertyNameList.isEmpty() && typeNameList.size() != propertyNameList.size() )
    {
      throw QgsRequestNotWellFormedException( QStringLiteral( "There has to be a 1:1 mapping between each element in a TYPENAME and the PROPERTYNAME list" ) );
    }
    if ( propertyNameList.isEmpty() )
    {
      for ( int i = 0; i < typeNameList.size(); ++i )
      {
        propertyNameList << QStringLiteral( "*" );
      }
    }

    // Create queries based on TypeName and propertyName
    QStringList::const_iterator typeNameIt = typeNameList.constBegin();
    QStringList::const_iterator propertyNameIt = propertyNameList.constBegin();
    for ( ; typeNameIt != typeNameList.constEnd(); ++typeNameIt )
    {
      QString typeName = *typeNameIt;
      typeName = typeName.trimmed();
      // Get PropertyName for this typeName
      QString propertyName;
      if ( propertyNameIt != propertyNameList.constEnd() )
      {
        propertyName = *propertyNameIt;
      }

      getFeatureQuery query;
      query.typeName = typeName;
      query.srsName = mWfsParameters.srsName();

      // Parse PropertyName
      if ( propertyName != QStringLiteral( "*" ) )
      {
        QStringList propertyList;

        QStringList attrList = propertyName.split( ',' );
        QStringList::const_iterator alstIt;
        for ( alstIt = attrList.begin(); alstIt != attrList.end(); ++alstIt )
        {
          QString fieldName = *alstIt;
          fieldName = fieldName.trimmed();
          if ( fieldName.contains( ':' ) )
          {
            fieldName = fieldName.section( ':', 1, 1 );
          }
          if ( fieldName.contains( '/' ) )
          {
            if ( fieldName.section( '/', 0, 0 ) != typeName )
            {
              throw QgsRequestNotWellFormedException( QStringLiteral( "PropertyName text '%1' has to contain TypeName '%2'" ).arg( fieldName ).arg( typeName ) );
            }
            fieldName = fieldName.section( '/', 1, 1 );
          }
          propertyList.append( fieldName );
        }
        query.propertyList = propertyList;
      }

      request.queries.append( query );

      if ( propertyNameIt != propertyNameList.constEnd() )
      {
        ++propertyNameIt;
      }
    }

    // Manage extra parameter exp_filter
    QStringList expFilterList = mWfsParameters.expFilters();
    if ( !expFilterList.isEmpty() )
    {
      // Verifying the 1:1 mapping between TYPENAME and EXP_FILTER but without exception
      if ( request.queries.size() == expFilterList.size() )
      {
        // set feature request filter expression based on filter element
        QList<getFeatureQuery>::iterator qIt = request.queries.begin();
        QStringList::const_iterator expFilterIt = expFilterList.constBegin();
        for ( ; qIt != request.queries.end(); ++qIt )
        {
          getFeatureQuery &query = *qIt;
          // Get Filter for this typeName
          QString expFilter;
          if ( expFilterIt != expFilterList.constEnd() )
          {
            expFilter = *expFilterIt;
          }
          std::shared_ptr<QgsExpression> filter( new QgsExpression( expFilter ) );
          if ( filter )
          {
            if ( filter->hasParserError() )
            {
              QgsMessageLog::logMessage( filter->parserErrorString() );
            }
            else
            {
              if ( filter->needsGeometry() )
              {
                query.featureRequest.setFlags( QgsFeatureRequest::NoFlags );
              }
              query.featureRequest.setFilterExpression( filter->expression() );
            }
          }
        }
      }
      else
      {
        QgsMessageLog::logMessage( "There has to be a 1:1 mapping between each element in a TYPENAME and the EXP_FILTER list" );
      }
    }

    if ( paramContainsBbox )
    {

      // get bbox extent
      QgsRectangle extent = mWfsParameters.bboxAsRectangle();

      // handle WFS 1.1.0 optional CRS
      if ( mWfsParameters.bbox().split( ',' ).size() == 5 && ! mWfsParameters.srsName().isEmpty() )
      {
        QString crs( mWfsParameters.bbox().split( ',' )[4] );
        if ( crs != mWfsParameters.srsName() )
        {
          QgsCoordinateReferenceSystem sourceCrs( crs );
          QgsCoordinateReferenceSystem destinationCrs( mWfsParameters.srsName() );
          if ( sourceCrs.isValid() && destinationCrs.isValid( ) )
          {
            QgsGeometry extentGeom = QgsGeometry::fromRect( extent );
            QgsCoordinateTransform transform;
            transform.setSourceCrs( sourceCrs );
            transform.setDestinationCrs( destinationCrs );
            try
            {
              if ( extentGeom.transform( transform ) == 0 )
              {
                extent = QgsRectangle( extentGeom.boundingBox() );
              }
            }
            catch ( QgsException &cse )
            {
              Q_UNUSED( cse );
            }
          }
        }
      }

      // set feature request filter rectangle
      QList<getFeatureQuery>::iterator qIt = request.queries.begin();
      for ( ; qIt != request.queries.end(); ++qIt )
      {
        getFeatureQuery &query = *qIt;
        query.featureRequest.setFilterRect( extent );
      }
      return request;
    }
    else if ( paramContainsFilters )
    {
      // Verifying the 1:1 mapping between TYPENAME and FILTER
      if ( request.queries.size() != filterList.size() )
      {
        throw QgsRequestNotWellFormedException( QStringLiteral( "There has to be a 1:1 mapping between each element in a TYPENAME and the FILTER list" ) );
      }

      // set feature request filter expression based on filter element
      QList<getFeatureQuery>::iterator qIt = request.queries.begin();
      QStringList::const_iterator filterIt = filterList.constBegin();
      for ( ; qIt != request.queries.end(); ++qIt )
      {
        getFeatureQuery &query = *qIt;
        // Get Filter for this typeName
        QDomDocument filter;
        if ( filterIt != filterList.constEnd() )
        {
          QString errorMsg;
          if ( !filter.setContent( *filterIt, true, &errorMsg ) )
          {
            throw QgsRequestNotWellFormedException( QStringLiteral( "error message: %1. The XML string was: %2" ).arg( errorMsg, *filterIt ) );
          }
        }

        QDomElement filterElem = filter.firstChildElement();
        query.featureRequest = parseFilterElement( query.typeName, filterElem, project );

        if ( filterIt != filterList.constEnd() )
        {
          ++filterIt;
        }
      }
      return request;
    }

    QStringList sortByList = mWfsParameters.sortBy();
    if ( !sortByList.isEmpty() && request.queries.size() == sortByList.size() )
    {
      // add order by to feature request
      QList<getFeatureQuery>::iterator qIt = request.queries.begin();
      QStringList::const_iterator sortByIt = sortByList.constBegin();
      for ( ; qIt != request.queries.end(); ++qIt )
      {
        getFeatureQuery &query = *qIt;
        // Get sortBy for this typeName
        QString sortBy;
        if ( sortByIt != sortByList.constEnd() )
        {
          sortBy = *sortByIt;
        }
        for ( const QString &attribute : sortBy.split( ',' ) )
        {
          if ( attribute.endsWith( QLatin1String( " D" ) ) || attribute.endsWith( QLatin1String( "+D" ) ) )
          {
            query.featureRequest.addOrderBy( attribute.left( attribute.size() - 2 ), false );
          }
          else if ( attribute.endsWith( QLatin1String( " DESC" ) ) || attribute.endsWith( QLatin1String( "+DESC" ) ) )
          {
            query.featureRequest.addOrderBy( attribute.left( attribute.size() - 5 ), false );
          }
          else if ( attribute.endsWith( QLatin1String( " A" ) ) || attribute.endsWith( QLatin1String( "+A" ) ) )
          {
            query.featureRequest.addOrderBy( attribute.left( attribute.size() - 2 ) );
          }
          else if ( attribute.endsWith( QLatin1String( " ASC" ) ) || attribute.endsWith( QLatin1String( "+ASC" ) ) )
          {
            query.featureRequest.addOrderBy( attribute.left( attribute.size() - 4 ) );
          }
          else
          {
            query.featureRequest.addOrderBy( attribute );
          }
        }
      }
    }

    return request;
  }

  getFeatureRequest parseGetFeatureRequestBody( QDomElement &docElem, const QgsProject *project )
  {
    getFeatureRequest request;
    request.maxFeatures = mWfsParameters.maxFeaturesAsInt();;
    request.startIndex = mWfsParameters.startIndexAsInt();
    request.outputFormat = mWfsParameters.outputFormat();

    QDomNodeList queryNodes = docElem.elementsByTagName( QStringLiteral( "Query" ) );
    QDomElement queryElem;
    for ( int i = 0; i < queryNodes.size(); i++ )
    {
      queryElem = queryNodes.at( i ).toElement();
      getFeatureQuery query = parseQueryElement( queryElem, project );
      request.queries.append( query );
    }
    return request;
  }

  void parseSortByElement( QDomElement &sortByElem, QgsFeatureRequest &featureRequest, const QString &typeName )
  {
    QDomNodeList sortByNodes = sortByElem.childNodes();
    if ( sortByNodes.size() )
    {
      for ( int i = 0; i < sortByNodes.size(); i++ )
      {
        QDomElement sortPropElem = sortByNodes.at( i ).toElement();
        QDomNodeList sortPropChildNodes = sortPropElem.childNodes();
        if ( sortPropChildNodes.size() )
        {
          QString fieldName;
          bool ascending = true;
          for ( int j = 0; j < sortPropChildNodes.size(); j++ )
          {
            QDomElement sortPropChildElem = sortPropChildNodes.at( j ).toElement();
            if ( sortPropChildElem.tagName() == QLatin1String( "PropertyName" ) )
            {
              fieldName = sortPropChildElem.text().trimmed();
            }
            else if ( sortPropChildElem.tagName() == QLatin1String( "SortOrder" ) )
            {
              QString sortOrder = sortPropChildElem.text().trimmed().toUpper();
              if ( sortOrder == QLatin1String( "DESC" ) || sortOrder == QLatin1String( "D" ) )
                ascending = false;
            }
          }
          // clean fieldName
          if ( fieldName.contains( ':' ) )
          {
            fieldName = fieldName.section( ':', 1, 1 );
          }
          if ( fieldName.contains( '/' ) )
          {
            if ( fieldName.section( '/', 0, 0 ) != typeName )
            {
              throw QgsRequestNotWellFormedException( QStringLiteral( "PropertyName text '%1' has to contain TypeName '%2'" ).arg( fieldName ).arg( typeName ) );
            }
            fieldName = fieldName.section( '/', 1, 1 );
          }
          // addOrderBy
          if ( !fieldName.isEmpty() )
            featureRequest.addOrderBy( fieldName, ascending );
        }
      }
    }
  }

  getFeatureQuery parseQueryElement( QDomElement &queryElem, const QgsProject *project )
  {
    QString typeName = queryElem.attribute( QStringLiteral( "typeName" ), QString() );
    if ( typeName.contains( ':' ) )
    {
      typeName = typeName.section( ':', 1, 1 );
    }

    QgsFeatureRequest featureRequest;
    QStringList propertyList;
    QDomNodeList queryChildNodes = queryElem.childNodes();
    if ( queryChildNodes.size() )
    {
      QDomElement sortByElem;
      for ( int q = 0; q < queryChildNodes.size(); q++ )
      {
        QDomElement queryChildElem = queryChildNodes.at( q ).toElement();
        if ( queryChildElem.tagName() == QLatin1String( "PropertyName" ) )
        {
          QString fieldName = queryChildElem.text().trimmed();
          if ( fieldName.contains( ':' ) )
          {
            fieldName = fieldName.section( ':', 1, 1 );
          }
          if ( fieldName.contains( '/' ) )
          {
            if ( fieldName.section( '/', 0, 0 ) != typeName )
            {
              throw QgsRequestNotWellFormedException( QStringLiteral( "PropertyName text '%1' has to contain TypeName '%2'" ).arg( fieldName ).arg( typeName ) );
            }
            fieldName = fieldName.section( '/', 1, 1 );
          }
          propertyList.append( fieldName );
        }
        else if ( queryChildElem.tagName() == QLatin1String( "Filter" ) )
        {
          featureRequest = parseFilterElement( typeName, queryChildElem, project );
        }
        else if ( queryChildElem.tagName() == QLatin1String( "SortBy" ) )
        {
          sortByElem = queryChildElem;
        }
      }
      parseSortByElement( sortByElem, featureRequest, typeName );
    }

    // srsName attribute
    QString srsName = queryElem.attribute( QStringLiteral( "srsName" ), QString() );

    getFeatureQuery query;
    query.typeName = typeName;
    query.srsName = srsName;
    query.featureRequest = featureRequest;
    query.propertyList = propertyList;
    return query;
  }

  namespace
  {
    static QSet< QString > sParamFilter
    {
      QStringLiteral( "REQUEST" ),
      QStringLiteral( "FORMAT" ),
      QStringLiteral( "OUTPUTFORMAT" ),
      QStringLiteral( "BBOX" ),
      QStringLiteral( "FEATUREID" ),
      QStringLiteral( "TYPENAME" ),
      QStringLiteral( "FILTER" ),
      QStringLiteral( "EXP_FILTER" ),
      QStringLiteral( "MAXFEATURES" ),
      QStringLiteral( "STARTINDEX" ),
      QStringLiteral( "PROPERTYNAME" ),
      QStringLiteral( "_DC" )
    };


    void hitGetFeature( const QgsServerRequest &request, QgsServerResponse &response, const QgsProject *project, QgsWfsParameters::Format format,
                        int numberOfFeatures, const QStringList &typeNames )
    {
      QDateTime now = QDateTime::currentDateTime();
      QString fcString;

      if ( format == QgsWfsParameters::Format::GeoJSON )
      {
        response.setHeader( "Content-Type", "application/vnd.geo+json; charset=utf-8" );
        fcString = QStringLiteral( "{\"type\": \"FeatureCollection\",\n" );
        fcString += QStringLiteral( " \"timeStamp\": \"%1\"\n" ).arg( now.toString( Qt::ISODate ) );
        fcString += QStringLiteral( " \"numberOfFeatures\": %1\n" ).arg( QString::number( numberOfFeatures ) );
        fcString += QLatin1String( "}" );
      }
      else
      {
        if ( format == QgsWfsParameters::Format::GML2 )
          response.setHeader( "Content-Type", "text/xml; subtype=gml/2.1.2; charset=utf-8" );
        else
          response.setHeader( "Content-Type", "text/xml; subtype=gml/3.1.1; charset=utf-8" );

        //Prepare url
        QString hrefString = serviceUrl( request, project );

        QUrl mapUrl( hrefString );

        QUrlQuery query( mapUrl );
        query.addQueryItem( QStringLiteral( "SERVICE" ), QStringLiteral( "WFS" ) );
        //Set version
        if ( mWfsParameters.version().isEmpty() )
          query.addQueryItem( QStringLiteral( "VERSION" ), implementationVersion() );
        else if ( mWfsParameters.versionAsNumber() >= QgsProjectVersion( 1, 1, 0 ) )
          query.addQueryItem( QStringLiteral( "VERSION" ), QStringLiteral( "1.1.0" ) );
        else
          query.addQueryItem( QStringLiteral( "VERSION" ), QStringLiteral( "1.0.0" ) );

        for ( auto param : query.queryItems() )
        {
          if ( sParamFilter.contains( param.first.toUpper() ) )
            query.removeAllQueryItems( param.first );
        }

        query.addQueryItem( QStringLiteral( "REQUEST" ), QStringLiteral( "DescribeFeatureType" ) );
        query.addQueryItem( QStringLiteral( "TYPENAME" ), typeNames.join( ',' ) );
        if ( mWfsParameters.versionAsNumber() >= QgsProjectVersion( 1, 1, 0 ) )
        {
          if ( format == QgsWfsParameters::Format::GML2 )
            query.addQueryItem( QStringLiteral( "OUTPUTFORMAT" ), QStringLiteral( "text/xml; subtype=gml/2.1.2" ) );
          else
            query.addQueryItem( QStringLiteral( "OUTPUTFORMAT" ), QStringLiteral( "text/xml; subtype=gml/3.1.1" ) );
        }
        else
          query.addQueryItem( QStringLiteral( "OUTPUTFORMAT" ), QStringLiteral( "XMLSCHEMA" ) );

        mapUrl.setQuery( query );

        hrefString = mapUrl.toString();

        //wfs:FeatureCollection valid
        fcString = QStringLiteral( "<wfs:FeatureCollection" );
        fcString += " xmlns:wfs=\"" + WFS_NAMESPACE + "\"";
        fcString += " xmlns:ogc=\"" + OGC_NAMESPACE + "\"";
        fcString += " xmlns:gml=\"" + GML_NAMESPACE + "\"";
        fcString += QLatin1String( " xmlns:ows=\"http://www.opengis.net/ows\"" );
        fcString += QLatin1String( " xmlns:xlink=\"http://www.w3.org/1999/xlink\"" );
        fcString += " xmlns:qgs=\"" + QGS_NAMESPACE + "\"";
        fcString += QLatin1String( " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" );
        fcString += " xsi:schemaLocation=\"" + WFS_NAMESPACE + " http://schemas.opengis.net/wfs/1.0.0/wfs.xsd " + QGS_NAMESPACE + " " + hrefString.replace( QLatin1String( "&" ), QLatin1String( "&amp;" ) ) + "\"";
        fcString += "\n timeStamp=\"" + now.toString( Qt::ISODate ) + "\"";
        fcString += "\n numberOfFeatures=\"" + QString::number( numberOfFeatures ) + "\"";
        fcString += QLatin1String( ">\n" );
        fcString += QStringLiteral( "</wfs:FeatureCollection>" );
      }

      response.write( fcString.toUtf8() );
      response.flush();
    }

    void startGetFeature( const QgsServerRequest &request, QgsServerResponse &response, const QgsProject *project, QgsWfsParameters::Format format,
                          int prec, QgsCoordinateReferenceSystem &crs, QgsRectangle *rect, const QStringList &typeNames )
    {
      QString fcString;

      std::unique_ptr< QgsRectangle > transformedRect;

      if ( format == QgsWfsParameters::Format::GeoJSON )
      {
        response.setHeader( "Content-Type", "application/vnd.geo+json; charset=utf-8" );

        if ( crs.isValid() && !rect->isEmpty() )
        {
          QgsGeometry exportGeom = QgsGeometry::fromRect( *rect );
          QgsCoordinateTransform transform;
          transform.setSourceCrs( crs );
          transform.setDestinationCrs( QgsCoordinateReferenceSystem( 4326, QgsCoordinateReferenceSystem::EpsgCrsId ) );
          try
          {
            if ( exportGeom.transform( transform ) == 0 )
            {
              transformedRect.reset( new QgsRectangle( exportGeom.boundingBox() ) );
              rect = transformedRect.get();
            }
          }
          catch ( QgsException &cse )
          {
            Q_UNUSED( cse );
          }
        }
        // EPSG:4326 max extent is -180, -90, 180, 90
        rect = new QgsRectangle( rect->intersect( QgsRectangle( -180.0, -90.0, 180.0, 90.0 ) ) );

        fcString = QStringLiteral( "{\"type\": \"FeatureCollection\",\n" );
        fcString += " \"bbox\": [ " + qgsDoubleToString( rect->xMinimum(), prec ) + ", " + qgsDoubleToString( rect->yMinimum(), prec ) + ", " + qgsDoubleToString( rect->xMaximum(), prec ) + ", " + qgsDoubleToString( rect->yMaximum(), prec ) + "],\n";
        fcString += QLatin1String( " \"features\": [\n" );
        response.write( fcString.toUtf8() );
      }
      else
      {
        if ( format == QgsWfsParameters::Format::GML2 )
          response.setHeader( "Content-Type", "text/xml; subtype=gml/2.1.2; charset=utf-8" );
        else
          response.setHeader( "Content-Type", "text/xml; subtype=gml/3.1.1; charset=utf-8" );

        //Prepare url
        QString hrefString = serviceUrl( request, project );

        QUrl mapUrl( hrefString );

        QUrlQuery query( mapUrl );
        query.addQueryItem( QStringLiteral( "SERVICE" ), QStringLiteral( "WFS" ) );
        //Set version
        if ( mWfsParameters.version().isEmpty() )
          query.addQueryItem( QStringLiteral( "VERSION" ), implementationVersion() );
        else if ( mWfsParameters.versionAsNumber() >= QgsProjectVersion( 1, 1, 0 ) )
          query.addQueryItem( QStringLiteral( "VERSION" ), QStringLiteral( "1.1.0" ) );
        else
          query.addQueryItem( QStringLiteral( "VERSION" ), QStringLiteral( "1.0.0" ) );

        for ( auto param : query.queryItems() )
        {
          if ( sParamFilter.contains( param.first.toUpper() ) )
            query.removeAllQueryItems( param.first );
        }

        query.addQueryItem( QStringLiteral( "REQUEST" ), QStringLiteral( "DescribeFeatureType" ) );
        query.addQueryItem( QStringLiteral( "TYPENAME" ), typeNames.join( ',' ) );
        if ( mWfsParameters.versionAsNumber() >= QgsProjectVersion( 1, 1, 0 ) )
        {
          if ( format == QgsWfsParameters::Format::GML2 )
            query.addQueryItem( QStringLiteral( "OUTPUTFORMAT" ), QStringLiteral( "text/xml; subtype=gml/2.1.2" ) );
          else
            query.addQueryItem( QStringLiteral( "OUTPUTFORMAT" ), QStringLiteral( "text/xml; subtype=gml/3.1.1" ) );
        }
        else
          query.addQueryItem( QStringLiteral( "OUTPUTFORMAT" ), QStringLiteral( "XMLSCHEMA" ) );

        mapUrl.setQuery( query );

        hrefString = mapUrl.toString();

        //wfs:FeatureCollection valid
        fcString = QStringLiteral( "<wfs:FeatureCollection" );
        fcString += " xmlns:wfs=\"" + WFS_NAMESPACE + "\"";
        fcString += " xmlns:ogc=\"" + OGC_NAMESPACE + "\"";
        fcString += " xmlns:gml=\"" + GML_NAMESPACE + "\"";
        fcString += QLatin1String( " xmlns:ows=\"http://www.opengis.net/ows\"" );
        fcString += QLatin1String( " xmlns:xlink=\"http://www.w3.org/1999/xlink\"" );
        fcString += " xmlns:qgs=\"" + QGS_NAMESPACE + "\"";
        fcString += QLatin1String( " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" );
        fcString += " xsi:schemaLocation=\"" + WFS_NAMESPACE + " http://schemas.opengis.net/wfs/1.0.0/wfs.xsd " + QGS_NAMESPACE + " " + hrefString.replace( QLatin1String( "&" ), QLatin1String( "&amp;" ) ) + "\"";
        fcString += QLatin1String( ">\n" );

        response.write( fcString.toUtf8() );
        response.flush();

        QDomDocument doc;
        QDomElement bbElem = doc.createElement( QStringLiteral( "gml:boundedBy" ) );
        if ( format == QgsWfsParameters::Format::GML3 )
        {
          QDomElement envElem = QgsOgcUtils::rectangleToGMLEnvelope( rect, doc, prec );
          if ( !envElem.isNull() )
          {
            if ( crs.isValid() )
            {
              envElem.setAttribute( QStringLiteral( "srsName" ), crs.authid() );
            }
            bbElem.appendChild( envElem );
            doc.appendChild( bbElem );
          }
        }
        else
        {
          QDomElement boxElem = QgsOgcUtils::rectangleToGMLBox( rect, doc, prec );
          if ( !boxElem.isNull() )
          {
            if ( crs.isValid() )
            {
              boxElem.setAttribute( QStringLiteral( "srsName" ), crs.authid() );
            }
            bbElem.appendChild( boxElem );
            doc.appendChild( bbElem );
          }
        }
        response.write( doc.toByteArray() );
        response.flush();
      }
    }

    void setGetFeature( QgsServerResponse &response, QgsWfsParameters::Format format, QgsFeature *feat, int featIdx,
                        const createFeatureParams &params, const QgsProject *project )
    {
      if ( !feat->isValid() )
        return;

      if ( format == QgsWfsParameters::Format::GeoJSON )
      {
        QString fcString;
        if ( featIdx == 0 )
          fcString += QLatin1String( "  " );
        else
          fcString += QLatin1String( " ," );
        mJsonExporter.setSourceCrs( params.crs );
        mJsonExporter.setIncludeGeometry( false );
        mJsonExporter.setIncludeAttributes( !params.attributeIndexes.isEmpty() );
        mJsonExporter.setAttributes( params.attributeIndexes );
        fcString += createFeatureGeoJSON( feat, params );
        fcString += QLatin1String( "\n" );

        response.write( fcString.toUtf8() );
      }
      else
      {
        QDomDocument gmlDoc;
        QDomElement featureElement;
        if ( format == QgsWfsParameters::Format::GML3 )
        {
          featureElement = createFeatureGML3( feat, gmlDoc, params, project );
          gmlDoc.appendChild( featureElement );
        }
        else
        {
          featureElement = createFeatureGML2( feat, gmlDoc, params, project );
          gmlDoc.appendChild( featureElement );
        }
        response.write( gmlDoc.toByteArray() );
      }

      // Stream partial content
      response.flush();
    }

    void endGetFeature( QgsServerResponse &response, QgsWfsParameters::Format format )
    {
      QString fcString;
      if ( format == QgsWfsParameters::Format::GeoJSON )
      {
        fcString += QLatin1String( " ]\n" );
        fcString += QLatin1String( "}" );
      }
      else
      {
        fcString = QStringLiteral( "</wfs:FeatureCollection>\n" );
      }
      response.write( fcString.toUtf8() );
    }


    QString createFeatureGeoJSON( QgsFeature *feat, const createFeatureParams &params )
    {
      QString id = QStringLiteral( "%1.%2" ).arg( params.typeName, FID_TO_STRING( feat->id() ) );
      //QgsJsonExporter force transform geometry to ESPG:4326
      //and the RFC 7946 GeoJSON specification recommends limiting coordinate precision to 6
      //Q_UNUSED( prec );

      //copy feature so we can modify its geometry as required
      QgsFeature f( *feat );
      QgsGeometry geom = feat->geometry();
      if ( !geom.isNull() && params.withGeom && params.geometryName != QLatin1String( "NONE" ) )
      {
        mJsonExporter.setIncludeGeometry( true );
        if ( params.geometryName == QLatin1String( "EXTENT" ) )
        {
          QgsRectangle box = geom.boundingBox();
          f.setGeometry( QgsGeometry::fromRect( box ) );
        }
        else if ( params.geometryName == QLatin1String( "CENTROID" ) )
        {
          f.setGeometry( geom.centroid() );
        }
      }

      return mJsonExporter.exportFeature( f, QVariantMap(), id );
    }


    QDomElement createFeatureGML2( QgsFeature *feat, QDomDocument &doc, const createFeatureParams &params, const QgsProject *project )
    {
      //gml:FeatureMember
      QDomElement featureElement = doc.createElement( QStringLiteral( "gml:featureMember" )/*wfs:FeatureMember*/ );

      //qgs:%TYPENAME%
      QDomElement typeNameElement = doc.createElement( "qgs:" + params.typeName /*qgs:%TYPENAME%*/ );
      typeNameElement.setAttribute( QStringLiteral( "fid" ), params.typeName + "." + QString::number( feat->id() ) );
      featureElement.appendChild( typeNameElement );

      //add geometry column (as gml)
      QgsGeometry geom = feat->geometry();
      if ( !geom.isNull() && params.withGeom && params.geometryName != QLatin1String( "NONE" ) )
      {
        int prec = params.precision;
        QgsCoordinateReferenceSystem crs = params.crs;
        QgsCoordinateTransform mTransform( crs, params.outputCrs, project );
        try
        {
          QgsGeometry transformed = geom;
          if ( transformed.transform( mTransform ) == 0 )
          {
            geom = transformed;
            crs = params.outputCrs;
            if ( crs.isGeographic() && !params.crs.isGeographic() )
              prec = std::min( params.precision + 3, 6 );
          }
        }
        catch ( QgsCsException &cse )
        {
          Q_UNUSED( cse );
        }

        QDomElement geomElem = doc.createElement( QStringLiteral( "qgs:geometry" ) );
        QDomElement gmlElem;
        if ( params.geometryName == QLatin1String( "EXTENT" ) )
        {
          QgsGeometry bbox = QgsGeometry::fromRect( geom.boundingBox() );
          gmlElem = QgsOgcUtils::geometryToGML( bbox, doc, prec );
        }
        else if ( params.geometryName == QLatin1String( "CENTROID" ) )
        {
          QgsGeometry centroid = geom.centroid();
          gmlElem = QgsOgcUtils::geometryToGML( centroid, doc, prec );
        }
        else
        {
          const QgsAbstractGeometry *abstractGeom = geom.constGet();
          if ( abstractGeom )
          {
            gmlElem = abstractGeom->asGml2( doc, prec, "http://www.opengis.net/gml" );
          }
        }

        if ( !gmlElem.isNull() )
        {
          QgsRectangle box = geom.boundingBox();
          QDomElement bbElem = doc.createElement( QStringLiteral( "gml:boundedBy" ) );
          QDomElement boxElem = QgsOgcUtils::rectangleToGMLBox( &box, doc, prec );

          if ( crs.isValid() )
          {
            boxElem.setAttribute( QStringLiteral( "srsName" ), crs.authid() );
            gmlElem.setAttribute( QStringLiteral( "srsName" ), crs.authid() );
          }

          bbElem.appendChild( boxElem );
          typeNameElement.appendChild( bbElem );

          geomElem.appendChild( gmlElem );
          typeNameElement.appendChild( geomElem );
        }
      }

      //read all attribute values from the feature
      QgsAttributes featureAttributes = feat->attributes();
      QgsFields fields = feat->fields();
      for ( int i = 0; i < params.attributeIndexes.count(); ++i )
      {
        int idx = params.attributeIndexes[i];
        if ( idx >= fields.count() )
        {
          continue;
        }
        const QgsField field = fields.at( idx );
        const QgsEditorWidgetSetup setup = field.editorWidgetSetup();
        QString attributeName = field.name();

        QDomElement fieldElem = doc.createElement( "qgs:" + attributeName.replace( ' ', '_' ).replace( cleanTagNameRegExp, QString() ) );
        QDomText fieldText = doc.createTextNode( encodeValueToText( featureAttributes[idx], setup ) );
        fieldElem.appendChild( fieldText );
        typeNameElement.appendChild( fieldElem );
      }

      return featureElement;
    }

    QDomElement createFeatureGML3( QgsFeature *feat, QDomDocument &doc, const createFeatureParams &params, const QgsProject *project )
    {
      //gml:FeatureMember
      QDomElement featureElement = doc.createElement( QStringLiteral( "gml:featureMember" )/*wfs:FeatureMember*/ );

      //qgs:%TYPENAME%
      QDomElement typeNameElement = doc.createElement( "qgs:" + params.typeName /*qgs:%TYPENAME%*/ );
      typeNameElement.setAttribute( QStringLiteral( "gml:id" ), params.typeName + "." + QString::number( feat->id() ) );
      featureElement.appendChild( typeNameElement );

      //add geometry column (as gml)
      QgsGeometry geom = feat->geometry();
      if ( !geom.isNull() && params.withGeom && params.geometryName != QLatin1String( "NONE" ) )
      {
        int prec = params.precision;
        QgsCoordinateReferenceSystem crs = params.crs;
        QgsCoordinateTransform mTransform( crs, params.outputCrs, project );
        try
        {
          QgsGeometry transformed = geom;
          if ( transformed.transform( mTransform ) == 0 )
          {
            geom = transformed;
            crs = params.outputCrs;
            if ( crs.isGeographic() && !params.crs.isGeographic() )
              prec = std::min( params.precision + 3, 6 );
          }
        }
        catch ( QgsCsException &cse )
        {
          Q_UNUSED( cse );
        }

        QDomElement geomElem = doc.createElement( QStringLiteral( "qgs:geometry" ) );
        QDomElement gmlElem;
        if ( params.geometryName == QLatin1String( "EXTENT" ) )
        {
          QgsGeometry bbox = QgsGeometry::fromRect( geom.boundingBox() );
          gmlElem = QgsOgcUtils::geometryToGML( bbox, doc, QStringLiteral( "GML3" ), prec );
        }
        else if ( params.geometryName == QLatin1String( "CENTROID" ) )
        {
          QgsGeometry centroid = geom.centroid();
          gmlElem = QgsOgcUtils::geometryToGML( centroid, doc, QStringLiteral( "GML3" ), prec );
        }
        else
        {
          const QgsAbstractGeometry *abstractGeom = geom.constGet();
          if ( abstractGeom )
          {
            gmlElem = abstractGeom->asGml3( doc, prec, "http://www.opengis.net/gml" );
          }
        }

        if ( !gmlElem.isNull() )
        {
          QgsRectangle box = geom.boundingBox();
          QDomElement bbElem = doc.createElement( QStringLiteral( "gml:boundedBy" ) );
          QDomElement boxElem = QgsOgcUtils::rectangleToGMLEnvelope( &box, doc, prec );

          if ( crs.isValid() )
          {
            boxElem.setAttribute( QStringLiteral( "srsName" ), crs.authid() );
            gmlElem.setAttribute( QStringLiteral( "srsName" ), crs.authid() );
          }

          bbElem.appendChild( boxElem );
          typeNameElement.appendChild( bbElem );

          geomElem.appendChild( gmlElem );
          typeNameElement.appendChild( geomElem );
        }
      }

      //read all attribute values from the feature
      QgsAttributes featureAttributes = feat->attributes();
      QgsFields fields = feat->fields();
      for ( int i = 0; i < params.attributeIndexes.count(); ++i )
      {
        int idx = params.attributeIndexes[i];
        if ( idx >= fields.count() )
        {
          continue;
        }
        const QgsField field = fields.at( idx );
        const QgsEditorWidgetSetup setup = field.editorWidgetSetup();
        QString attributeName = field.name();

        QDomElement fieldElem = doc.createElement( "qgs:" + attributeName.replace( ' ', '_' ).replace( cleanTagNameRegExp, QString() ) );
        QDomText fieldText = doc.createTextNode( encodeValueToText( featureAttributes[idx], setup ) );
        fieldElem.appendChild( fieldText );
        typeNameElement.appendChild( fieldElem );
      }

      return featureElement;
    }

    QString encodeValueToText( const QVariant &value, const QgsEditorWidgetSetup &setup )
    {
      if ( value.isNull() )
        return QString();

      if ( setup.type() ==  QStringLiteral( "DateTime" ) )
      {
        QgsDateTimeFieldFormatter fieldFormatter;
        const QVariantMap config = setup.config();
        const QString fieldFormat = config.value( QStringLiteral( "field_format" ), fieldFormatter.defaultFormat( value.type() ) ).toString();
        QDateTime date = value.toDateTime();

        if ( date.isValid() )
        {
          return date.toString( fieldFormat );
        }
      }
      else if ( setup.type() ==  QStringLiteral( "Range" ) )
      {
        const QVariantMap config = setup.config();
        if ( config.contains( QStringLiteral( "Precision" ) ) )
        {
          // if precision is defined, use it
          bool ok;
          int precision( config[ QStringLiteral( "Precision" ) ].toInt( &ok ) );
          if ( ok )
            return QString::number( value.toDouble(), 'f', precision );
        }
      }

      switch ( value.type() )
      {
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::LongLong:
        case QVariant::ULongLong:
        case QVariant::Double:
          return value.toString();

        case QVariant::Bool:
          return value.toBool() ? QStringLiteral( "true" ) : QStringLiteral( "false" );

        case QVariant::StringList:
        case QVariant::List:
        case QVariant::Map:
        {
          QString v = QgsJsonUtils::encodeValue( value );

          //do we need CDATA
          if ( v.indexOf( '<' ) != -1 || v.indexOf( '&' ) != -1 )
            v.prepend( QStringLiteral( "<![CDATA[" ) ).append( QStringLiteral( "]]>" ) );

          return v;
        }

        default:
        case QVariant::String:
        {
          QString v = value.toString();

          //do we need CDATA
          if ( v.indexOf( '<' ) != -1 || v.indexOf( '&' ) != -1 )
            v.prepend( QStringLiteral( "<![CDATA[" ) ).append( QStringLiteral( "]]>" ) );

          return v;
        }
      }
    }


  } // namespace

} // namespace QgsWfs



