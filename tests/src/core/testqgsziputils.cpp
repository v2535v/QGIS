/***************************************************************************
     testqgsziputils.cpp
     --------------------
    begin                : December 2018
    copyright            : (C) 2018 Viktor Sklencar
    email                : vsklencar at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgstest.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QDirIterator>

#include "qgsziputils.h"
#include "qgsapplication.h"

class TestQgsZipUtils: public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init();// will be called before each testfunction is executed.
    void cleanup();// will be called after every testfunction.

    void unzipWithSubdirs();
    void unzipWithSubdirs2();

  private:
    void genericTest( QString zipName, int expectedEntries, bool includeFolders, const QStringList &testFileNames );
};

void TestQgsZipUtils::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestQgsZipUtils::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsZipUtils::init()
{

}

void TestQgsZipUtils::cleanup()
{

}

void TestQgsZipUtils::unzipWithSubdirs()
{
  QStringList testFileNames;
  testFileNames << "/folder/folder2/landsat_b2.tif" << "/folder/points.geojson" << "/points.qml";
  genericTest( QString( "testzip" ), 11, true, testFileNames );
}

/**
 * Test unzips zip file with a following structure. Note that subfolder is not included in the structure as
 * usually expected. Zip file has been made with the python zipstream lib (https://github.com/allanlei/python-zipstream).
 *
 * output of zipinfo diff_structured.zip:
 * Archive:  diff_structured.zip
 * Zip file size: 452 bytes, number of entries: 3
 * ?rw-------  2.0 unx       16 bl defN 18-Dec-18 13:27 subfolder/second_level.txt
 * ?rw-------  2.0 unx        5 bl defN 18-Dec-18 13:27 subfolder/3.txt
 * ?rw-------  2.0 unx       15 bl defN 18-Dec-18 13:27 first_level.txt
 *
*/
void TestQgsZipUtils::unzipWithSubdirs2()
{
  genericTest( QString( "diff_structured" ), 3, false, QStringList() << "/subfolder/3.txt" );
}

/**
 * \brief TestQgsZipUtils::genericTest
 * \param zipName File to unzip
 * \param expectedEntries number of expected entries in given file
 * \param includeFolders Tag if a folder should be count as an entry
 * \param testFileNames List of file names to check if files were unzipped successfully
 */
void TestQgsZipUtils::genericTest( QString zipName, int expectedEntries, bool includeFolders, const QStringList &testFileNames )
{
  QFile zipFile( QString( TEST_DATA_DIR ) + QString( "/zip/%1.zip" ).arg( zipName ) );
  QVERIFY( zipFile.exists() );

  QFileInfo fileInfo( zipFile );
  QString unzipDirPath = QDir::tempPath() + zipName;
  QStringList files;

  // Create a root folder otherwise nothing is unzipped
  QDir dir( unzipDirPath );
  if ( !dir.exists( unzipDirPath ) )
  {
    dir.mkdir( unzipDirPath );
  }

  QgsZipUtils::unzip( fileInfo.absoluteFilePath(), unzipDirPath, files );
  // Test number of unzipped files
  QCOMPARE( files.count(), expectedEntries );

  if ( includeFolders )
  {
    dir.setFilter( QDir::Files |  QDir::NoDotAndDotDot | QDir::Dirs );
  }
  else
  {
    dir.setFilter( QDir::Files |  QDir::NoDotAndDotDot );
  }
  // Get list of entries from the root folder
  QDirIterator it( dir, QDirIterator::Subdirectories );
  QStringList filesFromResultDir;
  while ( it.hasNext() )
    filesFromResultDir << it.next();

  // Test if ziplib matches number of files in the root folder
  QCOMPARE( files.count(), filesFromResultDir.count() );

  // Test if specific files are included in the root folder
  for ( QString fileName : testFileNames )
  {
    QVERIFY( filesFromResultDir.contains( unzipDirPath + fileName ) );
  }

  // Delete unzipped data
  bool testDataRemoved = dir.removeRecursively();
  QVERIFY( testDataRemoved );
}

QGSTEST_MAIN( TestQgsZipUtils )
#include "testqgsziputils.moc"
