/***************************************************************************
                         qgshillshaderenderer.cpp
                         ---------------------------------
    begin                : May 2016
    copyright            : (C) 2016 by Nathan Woodrow
    email                : woodrow dot nathan at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QColor>

#include "qgshillshaderenderer.h"
#include "qgsrastertransparency.h"
#include "qgsrasterinterface.h"
#include "qgsrasterblock.h"
#include "qgsrectangle.h"
#include "qgsmessagelog.h"
#include <memory>

#ifdef HAVE_OPENCL
#ifdef QGISDEBUG
#include <chrono>
#include "qgssettings.h"
#endif
#include "qgsopenclutils.h"
#endif

QgsHillshadeRenderer::QgsHillshadeRenderer( QgsRasterInterface *input, int band, double lightAzimuth, double lightAngle ):
  QgsRasterRenderer( input, QStringLiteral( "hillshade" ) )
  , mBand( band )
  , mZFactor( 1 )
  , mLightAngle( lightAngle )
  , mLightAzimuth( lightAzimuth )
  , mMultiDirectional( false )
{

}

QgsHillshadeRenderer *QgsHillshadeRenderer::clone() const
{
  QgsHillshadeRenderer *r = new QgsHillshadeRenderer( nullptr, mBand, mLightAzimuth, mLightAngle );
  r->copyCommonProperties( this );

  r->setZFactor( mZFactor );
  r->setMultiDirectional( mMultiDirectional );
  return r;
}

QgsRasterRenderer *QgsHillshadeRenderer::create( const QDomElement &elem, QgsRasterInterface *input )
{
  if ( elem.isNull() )
  {
    return nullptr;
  }

  int band = elem.attribute( QStringLiteral( "band" ), QStringLiteral( "0" ) ).toInt();
  double azimuth = elem.attribute( QStringLiteral( "azimuth" ), QStringLiteral( "315" ) ).toDouble();
  double angle = elem.attribute( QStringLiteral( "angle" ), QStringLiteral( "45" ) ).toDouble();
  double zFactor = elem.attribute( QStringLiteral( "zfactor" ), QStringLiteral( "1" ) ).toDouble();
  bool multiDirectional = elem.attribute( QStringLiteral( "multidirection" ), QStringLiteral( "0" ) ).toInt();
  QgsHillshadeRenderer *r = new QgsHillshadeRenderer( input, band, azimuth, angle );
  r->readXml( elem );

  r->setZFactor( zFactor );
  r->setMultiDirectional( multiDirectional );
  return r;
}

void QgsHillshadeRenderer::writeXml( QDomDocument &doc, QDomElement &parentElem ) const
{
  if ( parentElem.isNull() )
  {
    return;
  }

  QDomElement rasterRendererElem = doc.createElement( QStringLiteral( "rasterrenderer" ) );
  _writeXml( doc, rasterRendererElem );

  rasterRendererElem.setAttribute( QStringLiteral( "band" ), mBand );
  rasterRendererElem.setAttribute( QStringLiteral( "azimuth" ), QString::number( mLightAzimuth ) );
  rasterRendererElem.setAttribute( QStringLiteral( "angle" ), QString::number( mLightAngle ) );
  rasterRendererElem.setAttribute( QStringLiteral( "zfactor" ), QString::number( mZFactor ) );
  rasterRendererElem.setAttribute( QStringLiteral( "multidirection" ), QString::number( mMultiDirectional ) );
  parentElem.appendChild( rasterRendererElem );
}

QgsRasterBlock *QgsHillshadeRenderer::block( int bandNo, const QgsRectangle &extent, int width, int height, QgsRasterBlockFeedback *feedback )
{
  Q_UNUSED( bandNo );
  std::unique_ptr< QgsRasterBlock > outputBlock( new QgsRasterBlock() );
  if ( !mInput )
  {
    QgsDebugMsg( QStringLiteral( "No input raster!" ) );
    return outputBlock.release();
  }

  std::shared_ptr< QgsRasterBlock > inputBlock( mInput->block( mBand, extent, width, height, feedback ) );

  if ( !inputBlock || inputBlock->isEmpty() )
  {
    QgsDebugMsg( QStringLiteral( "No raster data!" ) );
    return outputBlock.release();
  }

  std::shared_ptr< QgsRasterBlock > alphaBlock;

  if ( mAlphaBand > 0 && mBand != mAlphaBand )
  {
    alphaBlock.reset( mInput->block( mAlphaBand, extent, width, height, feedback ) );
    if ( !alphaBlock || alphaBlock->isEmpty() )
    {
      // TODO: better to render without alpha
      return outputBlock.release();
    }
  }
  else if ( mAlphaBand > 0 )
  {
    alphaBlock = inputBlock;
  }

  if ( !outputBlock->reset( Qgis::ARGB32_Premultiplied, width, height ) )
  {
    return outputBlock.release();
  }

  // Starting the computation

  // Common pre-calculated values
  float cellXSize = static_cast<float>( extent.width() ) / width;
  float cellYSize = static_cast<float>( extent.height() ) / height;
  float zenithRad = static_cast<float>( std::max( 0.0, 90 - mLightAngle ) * M_PI / 180.0 );
  float azimuthRad = static_cast<float>( -1 * mLightAzimuth * M_PI / 180.0 );
  float cosZenithRad = std::cos( zenithRad );
  float sinZenithRad = std::sin( zenithRad );

  // For fast formula from GDAL DEM
  float cos_alt_mul_z = cosZenithRad * static_cast<float>( mZFactor );
  float cos_az_mul_cos_alt_mul_z = std::cos( azimuthRad ) * cos_alt_mul_z;
  float sin_az_mul_cos_alt_mul_z = std::sin( azimuthRad ) * cos_alt_mul_z;
  float cos_az_mul_cos_alt_mul_z_mul_254 = 254.0f * cos_az_mul_cos_alt_mul_z;
  float sin_az_mul_cos_alt_mul_z_mul_254 = 254.0f * sin_az_mul_cos_alt_mul_z;
  float square_z = static_cast<float>( mZFactor * mZFactor );
  float sin_altRadians_mul_254 = 254.0f * sinZenithRad;

  // For multi directional
  float sin_altRadians_mul_127 = 127.0f * sinZenithRad;
  // 127.0 * std::cos(225.0 *  M_PI / 180.0) = -32.87001872802012
  float cos225_az_mul_cos_alt_mul_z_mul_127 = -32.87001872802012f * cos_alt_mul_z;
  float cos_alt_mul_z_mul_127 = 127.0f * cos_alt_mul_z;

  QRgb defaultNodataColor = NODATA_COLOR;


#ifdef HAVE_OPENCL

  // Use OpenCL? For now OpenCL is enabled in the default configuration only
  bool useOpenCL( QgsOpenClUtils::enabled()
                  && QgsOpenClUtils::available()
                  && ( ! mRasterTransparency || mRasterTransparency->isEmpty() )
                  && mAlphaBand <= 0 );
  // Check for sources
  QString source;
  if ( useOpenCL )
  {
    source = QgsOpenClUtils::sourceFromBaseName( QStringLiteral( "hillshade_renderer" ) );
    if ( source.isEmpty() )
    {
      useOpenCL = false;
      QgsMessageLog::logMessage( QObject::tr( "Error loading OpenCL program source from path" ).arg( QgsOpenClUtils::sourcePath() ), QgsOpenClUtils::LOGMESSAGE_TAG, Qgis::Critical );
    }
  }

#ifdef QGISDEBUG
  std::chrono::time_point<std::chrono::system_clock> startTime( std::chrono::system_clock::now() );
#endif

  if ( useOpenCL )
  {

    try
    {
      std::size_t inputDataTypeSize = inputBlock->dataTypeSize();
      std::size_t outputDataTypeSize = outputBlock->dataTypeSize();
      // Buffer scanline, 1px height, 2px wider
      // Data type for input is Float32 (4 bytes)
      std::size_t scanLineWidth( inputBlock->width() + 2 );
      std::size_t inputSize( inputDataTypeSize * inputBlock->width() );

      // CL buffers are also 2px wider for nodata initial and final columns
      std::size_t bufferWidth( width + 2 );
      std::size_t bufferSize( inputDataTypeSize * bufferWidth );

      // Buffer scanlines, 1px height, 2px wider
      // Data type for input is Float32 (4 bytes)
      // keep only three scanlines in memory at a time, make room for initial and final nodata
      std::unique_ptr<QgsRasterBlock> scanLine = qgis::make_unique<QgsRasterBlock>( inputBlock->dataType(), scanLineWidth, 1 );
      // Note: output block is not 2px wider and it is an image
      // Prepare context and queue
      cl::Context ctx = QgsOpenClUtils::context();
      cl::CommandQueue queue = QgsOpenClUtils::commandQueue();

      // Cast to float (because double just crashes on some GPUs)
      std::vector<float> rasterParams;
      rasterParams.push_back( inputBlock->noDataValue() );
      rasterParams.push_back( outputBlock->noDataValue() );
      rasterParams.push_back( mZFactor );
      rasterParams.push_back( cellXSize );
      rasterParams.push_back( cellYSize );
      rasterParams.push_back( static_cast<float>( mOpacity ) ); // 5

      // For fast formula from GDAL DEM
      rasterParams.push_back( cos_az_mul_cos_alt_mul_z_mul_254 ); // 6
      rasterParams.push_back( sin_az_mul_cos_alt_mul_z_mul_254 ); // 7
      rasterParams.push_back( square_z ); // 8
      rasterParams.push_back( sin_altRadians_mul_254 ); // 9

      // For multidirectional fast formula
      rasterParams.push_back( sin_altRadians_mul_127 ); // 10
      rasterParams.push_back( cos225_az_mul_cos_alt_mul_z_mul_127 ); // 11
      rasterParams.push_back( cos_alt_mul_z_mul_127 ); // 12

      // Default color for nodata (BGR components)
      rasterParams.push_back( static_cast<float>( qBlue( defaultNodataColor ) ) ); // 13
      rasterParams.push_back( static_cast<float>( qGreen( defaultNodataColor ) ) ); // 14
      rasterParams.push_back( static_cast<float>( qRed( defaultNodataColor ) ) ); // 15
      rasterParams.push_back( static_cast<float>( qAlpha( defaultNodataColor ) ) / 255.0f ); // 16

      // Whether use multidirectional
      rasterParams.push_back( static_cast<float>( mMultiDirectional ) ); // 17


      cl::Buffer rasterParamsBuffer( queue, rasterParams.begin(), rasterParams.end(), true, false, nullptr );
      cl::Buffer scanLine1Buffer( ctx, CL_MEM_READ_ONLY, bufferSize, nullptr, nullptr );
      cl::Buffer scanLine2Buffer( ctx, CL_MEM_READ_ONLY, bufferSize, nullptr, nullptr );
      cl::Buffer scanLine3Buffer( ctx, CL_MEM_READ_ONLY, bufferSize, nullptr, nullptr );
      cl::Buffer *scanLineBuffer[3] = {&scanLine1Buffer, &scanLine2Buffer, &scanLine3Buffer};
      // Note that result buffer is an image
      cl::Buffer resultLineBuffer( ctx, CL_MEM_WRITE_ONLY, outputDataTypeSize * width, nullptr, nullptr );

      static cl::Program program;
      static std::once_flag programBuilt;
      std::call_once( programBuilt, [ = ]()
      {
        // Create a program from the kernel source
        program = QgsOpenClUtils::buildProgram( source, QgsOpenClUtils::ExceptionBehavior::Throw );
      } );

      // Disable program cache when developing and testing cl program
      // program = QgsOpenClUtils::buildProgram( ctx, source, QgsOpenClUtils::ExceptionBehavior::Throw );

      // Create the OpenCL kernel
      auto kernel =  cl::KernelFunctor <
                     cl::Buffer &,
                     cl::Buffer &,
                     cl::Buffer &,
                     cl::Buffer &,
                     cl::Buffer &
                     > ( program, "processNineCellWindow" );


      // Rotating buffer index
      std::vector<int> rowIndex = {0, 1, 2};

      for ( int i = 0; i < height; i++ )
      {
        if ( feedback && feedback->isCanceled() )
        {
          break;
        }

        if ( feedback )
        {
          feedback->setProgress( 100.0 * static_cast< double >( i ) / height );
        }

        if ( i == 0 )
        {
          // Fill scanline 1 with (input) nodata for the values above the first row and feed scanline2 with the first row
          scanLine->resetNoDataValue();
          queue.enqueueWriteBuffer( scanLine1Buffer, CL_TRUE, 0, bufferSize, scanLine->bits( ) );
          // Read first row
          memcpy( scanLine->bits( 0, 1 ), inputBlock->bits( i, 0 ), inputSize );
          queue.enqueueWriteBuffer( scanLine2Buffer, CL_TRUE, 0, bufferSize, scanLine->bits( ) ); // row 0
          // Second row
          memcpy( scanLine->bits( 0, 1 ), inputBlock->bits( i + 1, 0 ), inputSize );
          queue.enqueueWriteBuffer( scanLine3Buffer, CL_TRUE, 0, bufferSize, scanLine->bits( ) ); //
        }
        else
        {
          // Normally fetch only scanLine3 and move forward one row
          // Read scanline 3, fill the last row with nodata values if it's the last iteration
          if ( i == inputBlock->height() - 1 )
          {
            scanLine->resetNoDataValue();
            queue.enqueueWriteBuffer( *scanLineBuffer[rowIndex[2]], CL_TRUE, 0, bufferSize, scanLine->bits( ) );
          }
          else // Overwrite from input, skip first and last
          {
            queue.enqueueWriteBuffer( *scanLineBuffer[rowIndex[2]], CL_TRUE, inputDataTypeSize * 1 /* offset 1 */, inputSize, inputBlock->bits( i + 1, 0 ) );
          }
        }

        kernel( cl::EnqueueArgs(
                  queue,
                  cl::NDRange( width )
                ),
                *scanLineBuffer[rowIndex[0]],
                *scanLineBuffer[rowIndex[1]],
                *scanLineBuffer[rowIndex[2]],
                resultLineBuffer,
                rasterParamsBuffer
              );

        queue.enqueueReadBuffer( resultLineBuffer, CL_TRUE, 0, outputDataTypeSize * outputBlock->width( ), outputBlock->bits( i, 0 ) );
        std::rotate( rowIndex.begin(), rowIndex.begin() + 1, rowIndex.end() );
      }
    }
    catch ( cl::Error &e )
    {
      QgsMessageLog::logMessage( QObject::tr( "Error running OpenCL program: %1 - %2" ).arg( e.what( ) ).arg( QgsOpenClUtils::errorText( e.err( ) ) ),
                                 QgsOpenClUtils::LOGMESSAGE_TAG, Qgis::Critical );
      QgsOpenClUtils::setEnabled( false );
      QgsMessageLog::logMessage( QObject::tr( "OpenCL has been disabled, you can re-enable it in the options dialog." ),
                                 QgsOpenClUtils::LOGMESSAGE_TAG, Qgis::Critical );
    }

  } // End of OpenCL processing path
  else  // Use the CPU and the original algorithm
  {

#endif

    for ( qgssize i = 0; i < static_cast<qgssize>( height ); i++ )
    {

      for ( qgssize j = 0; j < static_cast<qgssize>( width ); j++ )
      {

        if ( inputBlock->isNoData( i,  j ) )
        {
          outputBlock->setColor( static_cast<int>( i ), static_cast<int>( j ), defaultNodataColor );
          continue;
        }

        qgssize iUp, iDown, jLeft, jRight;
        if ( i == 0 )
        {
          iUp = i;
          iDown = i + 1;
        }
        else if ( i < static_cast<qgssize>( height ) - 1 )
        {
          iUp = i - 1;
          iDown = i + 1;
        }
        else
        {
          iUp = i - 1;
          iDown = i;
        }

        if ( j == 0 )
        {
          jLeft = j;
          jRight = j + 1;
        }
        else if ( j <  static_cast<qgssize>( width ) - 1 )
        {
          jLeft = j - 1;
          jRight = j + 1;
        }
        else
        {
          jLeft = j - 1;
          jRight = j;
        }

        double x11;
        double x21;
        double x31;
        double x12;
        double x22; // Working cell
        double x32;
        double x13;
        double x23;
        double x33;

        // This is center cell. It is not nodata. Use this in place of nodata neighbors
        x22 = inputBlock->value( i, j );

        x11 = inputBlock->isNoData( iUp, jLeft )  ? x22 : inputBlock->value( iUp, jLeft );
        x21 = inputBlock->isNoData( i, jLeft )     ? x22 : inputBlock->value( i, jLeft );
        x31 = inputBlock->isNoData( iDown, jLeft ) ? x22 : inputBlock->value( iDown, jLeft );

        x12 = inputBlock->isNoData( iUp, j )       ? x22 : inputBlock->value( iUp, j );
        // x22
        x32 = inputBlock->isNoData( iDown, j )     ? x22 : inputBlock->value( iDown, j );

        x13 = inputBlock->isNoData( iUp, jRight )   ? x22 : inputBlock->value( iUp, jRight );
        x23 = inputBlock->isNoData( i, jRight )     ? x22 : inputBlock->value( i, jRight );
        x33 = inputBlock->isNoData( iDown, jRight ) ? x22 : inputBlock->value( iDown, jRight );

        double derX = calcFirstDerX( x11, x21, x31, x12, x22, x32, x13, x23, x33, cellXSize );
        double derY = calcFirstDerY( x11, x21, x31, x12, x22, x32, x13, x23, x33, cellYSize );

        // Fast formula

        double grayValue;
        if ( !mMultiDirectional )
        {
          // Standard single direction hillshade
          grayValue = qBound( 0.0, ( sin_altRadians_mul_254 -
                                     ( derY * cos_az_mul_cos_alt_mul_z_mul_254 -
                                       derX * sin_az_mul_cos_alt_mul_z_mul_254 ) ) /
                              std::sqrt( 1 + square_z * ( derX * derX + derY * derY ) )
                              , 255.0 );
        }
        else
        {
          // Weighted multi direction as in http://pubs.usgs.gov/of/1992/of92-422/of92-422.pdf
          // Fast formula from GDAL DEM
          const float xx = derX * derX;
          const float yy = derY * derY;
          const float xx_plus_yy = xx + yy;
          // Flat?
          if ( xx_plus_yy == 0.0 )
          {
            grayValue = qBound( 0.0f, static_cast<float>( 1.0 + sin_altRadians_mul_254 ), 255.0f );
          }
          else
          {
            // ... then the shade value from different azimuth
            float val225_mul_127 = sin_altRadians_mul_127 +
                                   ( derX - derY ) * cos225_az_mul_cos_alt_mul_z_mul_127;
            val225_mul_127 = ( val225_mul_127 <= 0.0 ) ? 0.0 : val225_mul_127;
            float val270_mul_127 = sin_altRadians_mul_127 -
                                   derX * cos_alt_mul_z_mul_127;
            val270_mul_127 = ( val270_mul_127 <= 0.0 ) ? 0.0 : val270_mul_127;
            float val315_mul_127 = sin_altRadians_mul_127 +
                                   ( derX + derY ) * cos225_az_mul_cos_alt_mul_z_mul_127;
            val315_mul_127 = ( val315_mul_127 <= 0.0 ) ? 0.0 : val315_mul_127;
            float val360_mul_127 = sin_altRadians_mul_127 -
                                   derY * cos_alt_mul_z_mul_127;
            val360_mul_127 = ( val360_mul_127 <= 0.0 ) ? 0.0 : val360_mul_127;

            // ... then the weighted shading
            const float weight_225 = 0.5 * xx_plus_yy - derX * derY;
            const float weight_270 = xx;
            const float weight_315 = xx_plus_yy - weight_225;
            const float weight_360 = yy;
            const float cang_mul_127 = (
                                         ( weight_225 * val225_mul_127 +
                                           weight_270 * val270_mul_127 +
                                           weight_315 * val315_mul_127 +
                                           weight_360 * val360_mul_127 ) / xx_plus_yy ) /
                                       ( 1 + square_z * xx_plus_yy );

            grayValue = qBound( 0.0f, 1.0f + cang_mul_127, 255.0f );
          }
        }

        double currentAlpha = mOpacity;
        if ( mRasterTransparency )
        {
          currentAlpha = mRasterTransparency->alphaValue( x22, mOpacity * 255 ) / 255.0;
        }
        if ( mAlphaBand > 0 )
        {
          currentAlpha *= alphaBlock->value( i ) / 255.0;
        }

        if ( qgsDoubleNear( currentAlpha, 1.0 ) )
        {
          outputBlock->setColor( i, j, qRgba( grayValue, grayValue, grayValue, 255 ) );
        }
        else
        {
          outputBlock->setColor( i, j, qRgba( currentAlpha * grayValue, currentAlpha * grayValue, currentAlpha * grayValue, currentAlpha * 255 ) );
        }
      }
    }

#ifdef HAVE_OPENCL
  } // End of switch in case OpenCL is not available or enabled

#ifdef QGISDEBUG
  if ( QgsSettings().value( QStringLiteral( "Map/logCanvasRefreshEvent" ), false ).toBool() )
  {
    QgsMessageLog::logMessage( QStringLiteral( "%1 processing time for hillshade (%2 x %3 ): %4 ms" )
                               .arg( useOpenCL ? QStringLiteral( "OpenCL" ) : QStringLiteral( "CPU" ) )
                               .arg( width )
                               .arg( height )
                               .arg( std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now() - startTime ).count() ),
                               tr( "Rendering" ) );
  }
#endif
#endif

  return outputBlock.release();
}

QList<int> QgsHillshadeRenderer::usesBands() const
{
  QList<int> bandList;
  if ( mBand != -1 )
  {
    bandList << mBand;
  }
  return bandList;

}

void QgsHillshadeRenderer::setBand( int bandNo )
{
  if ( bandNo > mInput->bandCount() || bandNo <= 0 )
  {
    return;
  }
  mBand = bandNo;
}

double QgsHillshadeRenderer::calcFirstDerX( double x11, double x21, double x31, double x12, double x22, double x32, double x13, double x23, double x33, double cellsize )
{
  Q_UNUSED( x12 );
  Q_UNUSED( x22 );
  Q_UNUSED( x32 );
  return ( ( x13 + x23 + x23 + x33 ) - ( x11 + x21 + x21 + x31 ) ) / ( 8 * cellsize );
}

double QgsHillshadeRenderer::calcFirstDerY( double x11, double x21, double x31, double x12, double x22, double x32, double x13, double x23, double x33, double cellsize )
{
  Q_UNUSED( x21 );
  Q_UNUSED( x22 );
  Q_UNUSED( x23 );
  return ( ( x31 + x32 + x32 + x33 ) - ( x11 + x12 + x12 + x13 ) ) / ( 8 * -cellsize );
}




