/***************************************************************************
                         qgsalgorithmbuffer.cpp
                         ---------------------
    begin                : April 2017
    copyright            : (C) 2017 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsalgorithmbuffer.h"
#include "qgswkbtypes.h"
#include "qgsvectorlayer.h"

///@cond PRIVATE

QString QgsBufferAlgorithm::name() const
{
  return QStringLiteral( "buffer" );
}

QString QgsBufferAlgorithm::displayName() const
{
  return QObject::tr( "Buffer" );
}

QStringList QgsBufferAlgorithm::tags() const
{
  return QObject::tr( "buffer,grow,fixed,variable,distance" ).split( ',' );
}

QString QgsBufferAlgorithm::group() const
{
  return QObject::tr( "Vector geometry" );
}

QString QgsBufferAlgorithm::groupId() const
{
  return QStringLiteral( "vectorgeometry" );
}

void QgsBufferAlgorithm::initAlgorithm( const QVariantMap & )
{
  addParameter( new QgsProcessingParameterFeatureSource( QStringLiteral( "INPUT" ), QObject::tr( "Input layer" ) ) );

  auto bufferParam = qgis::make_unique < QgsProcessingParameterDistance >( QStringLiteral( "DISTANCE" ), QObject::tr( "Distance" ), 10, QStringLiteral( "INPUT" ) );
  bufferParam->setIsDynamic( true );
  bufferParam->setDynamicPropertyDefinition( QgsPropertyDefinition( QStringLiteral( "Distance" ), QObject::tr( "Buffer distance" ), QgsPropertyDefinition::Double ) );
  bufferParam->setDynamicLayerParameterName( QStringLiteral( "INPUT" ) );
  addParameter( bufferParam.release() );
  addParameter( new QgsProcessingParameterNumber( QStringLiteral( "SEGMENTS" ), QObject::tr( "Segments" ), QgsProcessingParameterNumber::Integer, 5, false, 1 ) );

  addParameter( new QgsProcessingParameterEnum( QStringLiteral( "END_CAP_STYLE" ), QObject::tr( "End cap style" ), QStringList() << QObject::tr( "Round" ) << QObject::tr( "Flat" ) << QObject::tr( "Square" ), false, 0 ) );
  addParameter( new QgsProcessingParameterEnum( QStringLiteral( "JOIN_STYLE" ), QObject::tr( "Join style" ), QStringList() << QObject::tr( "Round" ) << QObject::tr( "Miter" ) << QObject::tr( "Bevel" ), false, 0 ) );
  addParameter( new QgsProcessingParameterNumber( QStringLiteral( "MITER_LIMIT" ), QObject::tr( "Miter limit" ), QgsProcessingParameterNumber::Double, 2, false, 1 ) );

  addParameter( new QgsProcessingParameterBoolean( QStringLiteral( "DISSOLVE" ), QObject::tr( "Dissolve result" ), false ) );
  addParameter( new QgsProcessingParameterFeatureSink( QStringLiteral( "OUTPUT" ), QObject::tr( "Buffered" ), QgsProcessing::TypeVectorPolygon ) );
}

QString QgsBufferAlgorithm::shortHelpString() const
{
  return QObject::tr( "This algorithm computes a buffer area for all the features in an input layer, using a fixed or dynamic distance.\n\n"
                      "The segments parameter controls the number of line segments to use to approximate a quarter circle when creating rounded offsets.\n\n"
                      "The end cap style parameter controls how line endings are handled in the buffer.\n\n"
                      "The join style parameter specifies whether round, miter or beveled joins should be used when offsetting corners in a line.\n\n"
                      "The miter limit parameter is only applicable for miter join styles, and controls the maximum distance from the offset curve to use when creating a mitered join." );
}

QgsBufferAlgorithm *QgsBufferAlgorithm::createInstance() const
{
  return new QgsBufferAlgorithm();
}

QVariantMap QgsBufferAlgorithm::processAlgorithm( const QVariantMap &parameters, QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  std::unique_ptr< QgsProcessingFeatureSource > source( parameterAsSource( parameters, QStringLiteral( "INPUT" ), context ) );
  if ( !source )
    throw QgsProcessingException( invalidSourceError( parameters, QStringLiteral( "INPUT" ) ) );

  QString dest;
  std::unique_ptr< QgsFeatureSink > sink( parameterAsSink( parameters, QStringLiteral( "OUTPUT" ), context, dest, source->fields(), QgsWkbTypes::Polygon, source->sourceCrs() ) );
  if ( !sink )
    throw QgsProcessingException( invalidSinkError( parameters, QStringLiteral( "OUTPUT" ) ) );

  // fixed parameters
  bool dissolve = parameterAsBool( parameters, QStringLiteral( "DISSOLVE" ), context );
  int segments = parameterAsInt( parameters, QStringLiteral( "SEGMENTS" ), context );
  QgsGeometry::EndCapStyle endCapStyle = static_cast< QgsGeometry::EndCapStyle >( 1 + parameterAsInt( parameters, QStringLiteral( "END_CAP_STYLE" ), context ) );
  QgsGeometry::JoinStyle joinStyle = static_cast< QgsGeometry::JoinStyle>( 1 + parameterAsInt( parameters, QStringLiteral( "JOIN_STYLE" ), context ) );
  double miterLimit = parameterAsDouble( parameters, QStringLiteral( "MITER_LIMIT" ), context );
  double bufferDistance = parameterAsDouble( parameters, QStringLiteral( "DISTANCE" ), context );
  bool dynamicBuffer = QgsProcessingParameters::isDynamic( parameters, QStringLiteral( "DISTANCE" ) );
  QgsExpressionContext expressionContext = createExpressionContext( parameters, context, source.get() );
  QgsProperty bufferProperty;
  if ( dynamicBuffer )
  {
    bufferProperty = parameters.value( QStringLiteral( "DISTANCE" ) ).value< QgsProperty >();
  }

  long count = source->featureCount();

  QgsFeature f;
  // buffer doesn't care about invalid features, and buffering can be used to repair geometries
  QgsFeatureIterator it = source->getFeatures( QgsFeatureRequest(), QgsProcessingFeatureSource::FlagSkipGeometryValidityChecks );

  double step = count > 0 ? 100.0 / count : 1;
  int current = 0;

  QVector< QgsGeometry > bufferedGeometriesForDissolve;
  QgsAttributes dissolveAttrs;

  while ( it.nextFeature( f ) )
  {
    if ( feedback->isCanceled() )
    {
      break;
    }
    if ( dissolveAttrs.isEmpty() )
      dissolveAttrs = f.attributes();

    QgsFeature out = f;
    if ( out.hasGeometry() )
    {
      double distance =  bufferDistance;
      if ( dynamicBuffer )
      {
        expressionContext.setFeature( f );
        distance = bufferProperty.valueAsDouble( expressionContext, bufferDistance );
      }

      QgsGeometry outputGeometry = f.geometry().buffer( distance, segments, endCapStyle, joinStyle, miterLimit );
      if ( outputGeometry.isNull() )
      {
        QgsMessageLog::logMessage( QObject::tr( "Error calculating buffer for feature %1" ).arg( f.id() ), QObject::tr( "Processing" ), Qgis::Warning );
      }
      if ( dissolve )
        bufferedGeometriesForDissolve << outputGeometry;
      else
        out.setGeometry( outputGeometry );
    }

    if ( !dissolve )
      sink->addFeature( out, QgsFeatureSink::FastInsert );

    feedback->setProgress( current * step );
    current++;
  }

  if ( dissolve )
  {
    QgsGeometry finalGeometry = QgsGeometry::unaryUnion( bufferedGeometriesForDissolve );
    QgsFeature f;
    f.setGeometry( finalGeometry );
    f.setAttributes( dissolveAttrs );
    sink->addFeature( f, QgsFeatureSink::FastInsert );
  }

  QVariantMap outputs;
  outputs.insert( QStringLiteral( "OUTPUT" ), dest );
  return outputs;
}

QgsProcessingAlgorithm::Flags QgsBufferAlgorithm::flags() const
{
  Flags f = QgsProcessingAlgorithm::flags();
  f |= QgsProcessingAlgorithm::FlagSupportsInPlaceEdits;
  return f;
}

bool QgsBufferAlgorithm::supportInPlaceEdit( const QgsMapLayer *layer ) const
{
  const QgsVectorLayer *vlayer = qobject_cast< const QgsVectorLayer * >( layer );
  if ( !vlayer )
    return false;
  //Only Polygons
  return vlayer->wkbType() == QgsWkbTypes::Type::Polygon || vlayer->wkbType() == QgsWkbTypes::Type::MultiPolygon;
}

///@endcond
