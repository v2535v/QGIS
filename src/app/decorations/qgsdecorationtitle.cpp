/***************************************************************************
  qgsdecorationtitle.cpp
  --------------------------------------
  Date                 : November 2018
  Copyright            : (C) 2018 by Mathieu Pellerin
  Email                : nirvn dot asia at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsdecorationtitle.h"
#include "qgsdecorationtitledialog.h"

#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsexpression.h"
#include "qgsexpressioncontext.h"
#include "qgslogger.h"
#include "qgsmapcanvas.h"
#include "qgsproject.h"
#include "qgsreadwritecontext.h"
#include "qgssymbollayerutils.h"

#include <QPainter>
#include <QMenu>
#include <QDate>
#include <QDomDocument>
#include <QMatrix>
#include <QFile>

//non qt includes
#include <cmath>


QgsDecorationTitle::QgsDecorationTitle( QObject *parent )
  : QgsDecorationItem( parent )
{
  mPlacement = TopCenter;
  mMarginUnit = QgsUnitTypes::RenderMillimeters;

  setName( "Title Label" );
  // initialize default values in the gui
  projectRead();
}

void QgsDecorationTitle::projectRead()
{
  QgsDecorationItem::projectRead();

  mLabelText = QgsProject::instance()->readEntry( mNameConfig, QStringLiteral( "/Label" ), QString() );
  mBackgroundColor = QgsSymbolLayerUtils::decodeColor( QgsProject::instance()->readEntry( mNameConfig, QStringLiteral( "/BackgroundColor" ), QStringLiteral( "0,0,0,99" ) ) );

  mMarginHorizontal = QgsProject::instance()->readNumEntry( mNameConfig, QStringLiteral( "/MarginH" ), 0 );
  mMarginVertical = QgsProject::instance()->readNumEntry( mNameConfig, QStringLiteral( "/MarginV" ), 0 );

  QDomDocument doc;
  QDomElement elem;
  QString textXml = QgsProject::instance()->readEntry( mNameConfig, QStringLiteral( "/Font" ) );
  if ( !textXml.isEmpty() )
  {
    doc.setContent( textXml );
    elem = doc.documentElement();
    QgsReadWriteContext rwContext;
    rwContext.setPathResolver( QgsProject::instance()->pathResolver() );
    mTextFormat.readXml( elem, rwContext );
  }
}

void QgsDecorationTitle::saveToProject()
{
  QgsDecorationItem::saveToProject();

  QgsProject::instance()->writeEntry( mNameConfig, QStringLiteral( "/Label" ), mLabelText );
  QgsProject::instance()->writeEntry( mNameConfig, QStringLiteral( "/BackgroundColor" ), QgsSymbolLayerUtils::encodeColor( mBackgroundColor ) );

  QgsProject::instance()->writeEntry( mNameConfig, QStringLiteral( "/MarginH" ), mMarginHorizontal );
  QgsProject::instance()->writeEntry( mNameConfig, QStringLiteral( "/MarginV" ), mMarginVertical );

  QDomDocument textDoc;
  QgsReadWriteContext rwContext;
  rwContext.setPathResolver( QgsProject::instance()->pathResolver() );
  QDomElement textElem = mTextFormat.writeXml( textDoc, rwContext );
  textDoc.appendChild( textElem );
  QgsProject::instance()->writeEntry( mNameConfig, QStringLiteral( "/Font" ), textDoc.toString() );
}

// Slot called when the buffer menu item is activated
void QgsDecorationTitle::run()
{
  QgsDecorationTitleDialog dlg( *this, QgisApp::instance() );
  dlg.exec();
}


void QgsDecorationTitle::render( const QgsMapSettings &mapSettings, QgsRenderContext &context )
{
  Q_UNUSED( mapSettings );
  if ( !enabled() )
    return;

  context.painter()->save();
  context.painter()->setRenderHint( QPainter::Antialiasing, true );

  QString displayString = QgsExpression::replaceExpressionText( mLabelText, &context.expressionContext() );
  QStringList displayStringList = displayString.split( '\n' );

  QFontMetricsF fm( mTextFormat.scaledFont( context ) );
  QFontMetricsF textMetrics = QgsTextRenderer::fontMetrics( context, mTextFormat );
  double textDescent = textMetrics.descent();
  double textWidth = QgsTextRenderer::textWidth( context, mTextFormat, displayStringList, &fm );
  double textHeight = QgsTextRenderer::textHeight( context, mTextFormat, displayStringList, QgsTextRenderer::Point, &fm );

  QPaintDevice *device = context.painter()->device();
  int deviceHeight = device->height() / device->devicePixelRatioF();
  int deviceWidth = device->width() / device->devicePixelRatioF();

  float xOffset( 0 ), yOffset( 0 );

  // Set  margin according to selected units
  switch ( mMarginUnit )
  {
    case QgsUnitTypes::RenderMillimeters:
    {
      int pixelsInchX = context.painter()->device()->logicalDpiX();
      int pixelsInchY = context.painter()->device()->logicalDpiY();
      xOffset = pixelsInchX * INCHES_TO_MM * mMarginHorizontal;
      yOffset = pixelsInchY * INCHES_TO_MM * mMarginVertical;
      break;
    }
    case QgsUnitTypes::RenderPixels:
    {
      xOffset = mMarginHorizontal;
      yOffset = mMarginVertical;
      break;
    }
    case QgsUnitTypes::RenderPercentage:
    {
      xOffset = ( ( deviceWidth - textWidth ) / 100. ) * mMarginHorizontal;
      yOffset = ( ( deviceHeight - textHeight ) / 100. ) * mMarginVertical;
      break;
    }
    case QgsUnitTypes::RenderMapUnits:
    case QgsUnitTypes::RenderPoints:
    case QgsUnitTypes::RenderInches:
    case QgsUnitTypes::RenderUnknownUnit:
    case QgsUnitTypes::RenderMetersInMapUnits:
      break;
  }

  QPolygonF backgroundBar;

  // Determine placement of label from form combo box
  QgsTextRenderer::HAlignment horizontalAlignment = QgsTextRenderer::AlignLeft;
  switch ( mPlacement )
  {
    case BottomLeft: // Bottom Left, xOffset is set above
      backgroundBar << QPointF( 0, deviceHeight )
                    << QPointF( deviceWidth, deviceHeight )
                    << QPointF( deviceWidth, deviceHeight - ( yOffset * 2 + textHeight ) )
                    << QPointF( 0, deviceHeight - ( yOffset * 2 + textHeight ) );
      yOffset = deviceHeight - yOffset - textDescent;
      break;
    case TopLeft: // Top left, xOffset is set above
      backgroundBar << QPointF( 0, 0 )
                    << QPointF( deviceWidth, 0 )
                    << QPointF( deviceWidth, yOffset * 2 + textHeight )
                    << QPointF( 0, yOffset * 2 + textHeight );
      yOffset = yOffset + textHeight - textDescent;
      break;
    case TopRight: // Top Right
      backgroundBar << QPointF( 0, 0 )
                    << QPointF( deviceWidth, 0 )
                    << QPointF( deviceWidth, yOffset * 2 + textHeight )
                    << QPointF( 0, yOffset * 2 + textHeight );
      yOffset = yOffset + textHeight - textDescent;
      xOffset = deviceWidth - xOffset;
      horizontalAlignment = QgsTextRenderer::AlignRight;
      break;
    case BottomRight: // Bottom Right
      backgroundBar << QPointF( 0, deviceHeight )
                    << QPointF( deviceWidth, deviceHeight )
                    << QPointF( deviceWidth, deviceHeight - ( yOffset * 2 + textHeight ) )
                    << QPointF( 0, deviceHeight - ( yOffset * 2 + textHeight ) );
      yOffset = deviceHeight - yOffset - textDescent;
      xOffset = deviceWidth - xOffset;
      horizontalAlignment = QgsTextRenderer::AlignRight;
      break;
    case TopCenter: // Top Center
      backgroundBar << QPointF( 0, 0 )
                    << QPointF( deviceWidth, 0 )
                    << QPointF( deviceWidth, yOffset * 2 + textHeight )
                    << QPointF( 0, yOffset * 2 + textHeight );
      yOffset = yOffset + textHeight - textDescent;
      xOffset = deviceWidth / 2;
      horizontalAlignment = QgsTextRenderer::AlignCenter;
      break;
    case BottomCenter: // Bottom Center
      backgroundBar << QPointF( 0, deviceHeight )
                    << QPointF( deviceWidth, deviceHeight )
                    << QPointF( deviceWidth, deviceHeight - ( yOffset * 2 + textHeight ) )
                    << QPointF( 0, deviceHeight - ( yOffset * 2 + textHeight ) );
      yOffset = deviceHeight - yOffset - textDescent;
      xOffset = deviceWidth / 2;
      horizontalAlignment = QgsTextRenderer::AlignCenter;
      break;
    default:
      QgsDebugMsg( QStringLiteral( "Unsupported placement index of %1" ).arg( static_cast<int>( mPlacement ) ) );
  }

  // Draw background bar
  context.painter()->setPen( Qt::NoPen );
  context.painter()->setBrush( QBrush( mBackgroundColor, Qt::SolidPattern ) );
  context.painter()->drawPolygon( backgroundBar );

  // Paint label to canvas
  QgsTextRenderer::drawText( QPointF( xOffset, yOffset ), 0.0, horizontalAlignment, displayStringList, context, mTextFormat );

  context.painter()->restore();
}

