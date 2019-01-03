/***************************************************************************
     testqgsattributetable.cpp
     -------------------------
    Date                 : 2016-02-14
    Copyright            : (C) 2016 by Nyall Dawson
    Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgstest.h"
#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsfeatureiterator.h"
#include "qgsvectorlayer.h"
#include "qgsfeature.h"
#include "qgsgeometry.h"
#include "qgsvectordataprovider.h"
#include "qgsattributetabledialog.h"
#include "qgsproject.h"
#include "qgsmapcanvas.h"
#include "qgsunittypes.h"
#include "qgssettings.h"
#include "qgsvectorfilewriter.h"
#include "qgsfeaturelistmodel.h"

#include "qgstest.h"

/**
 * \ingroup UnitTests
 * This is a unit test for the attribute table dialog
 */
class TestQgsAttributeTable : public QObject
{
    Q_OBJECT
  public:
    TestQgsAttributeTable();

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() {} // will be called before each testfunction is executed.
    void cleanup() {} // will be called after every testfunction.
    void testRegression15974();
    void testFieldCalculation();
    void testFieldCalculationArea();
    void testNoGeom();
    void testSelected();
    void testSortByDisplayExpression();
    void testOrderColumn();

  private:
    QgisApp *mQgisApp = nullptr;
};

TestQgsAttributeTable::TestQgsAttributeTable() = default;

//runs before all tests
void TestQgsAttributeTable::initTestCase()
{
  qDebug() << "TestQgisAppClipboard::initTestCase()";
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();
  mQgisApp = new QgisApp();

  // setup the test QSettings environment
  QCoreApplication::setOrganizationName( QStringLiteral( "QGIS" ) );
  QCoreApplication::setOrganizationDomain( QStringLiteral( "qgis.org" ) );
  QCoreApplication::setApplicationName( QStringLiteral( "QGIS-TEST" ) );
}

//runs after all tests
void TestQgsAttributeTable::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsAttributeTable::testFieldCalculation()
{
  //test field calculation

  //create a temporary layer
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "LineString?crs=epsg:3111&field=pk:int&field=col1:double" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );
  QgsFeature f1( tempLayer->dataProvider()->fields(), 1 );
  f1.setAttribute( QStringLiteral( "pk" ), 1 );
  f1.setAttribute( QStringLiteral( "col1" ), 0.0 );
  QgsPolylineXY line3111;
  line3111 << QgsPointXY( 2484588, 2425722 ) << QgsPointXY( 2482767, 2398853 );
  QgsGeometry line3111G = QgsGeometry::fromPolylineXY( line3111 ) ;
  f1.setGeometry( line3111G );
  tempLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 );

  // set project CRS and ellipsoid
  QgsCoordinateReferenceSystem srs( 3111, QgsCoordinateReferenceSystem::EpsgCrsId );
  QgsProject::instance()->setCrs( srs );
  QgsProject::instance()->setEllipsoid( QStringLiteral( "WGS84" ) );
  QgsProject::instance()->setDistanceUnits( QgsUnitTypes::DistanceMeters );

  // run length calculation
  std::unique_ptr< QgsAttributeTableDialog > dlg( new QgsAttributeTableDialog( tempLayer.get() ) );
  tempLayer->startEditing();
  dlg->runFieldCalculation( tempLayer.get(), QStringLiteral( "col1" ), QStringLiteral( "$length" ) );
  tempLayer->commitChanges();
  // check result
  QgsFeatureIterator fit = tempLayer->dataProvider()->getFeatures();
  QgsFeature f;
  QVERIFY( fit.nextFeature( f ) );
  double expected = 26932.156;
  QGSCOMPARENEAR( f.attribute( "col1" ).toDouble(), expected, 0.001 );

  // change project length unit, check calculation respects unit
  QgsProject::instance()->setDistanceUnits( QgsUnitTypes::DistanceFeet );
  std::unique_ptr< QgsAttributeTableDialog > dlg2( new QgsAttributeTableDialog( tempLayer.get() ) );
  tempLayer->startEditing();
  dlg2->runFieldCalculation( tempLayer.get(), QStringLiteral( "col1" ), QStringLiteral( "$length" ) );
  tempLayer->commitChanges();
  // check result
  fit = tempLayer->dataProvider()->getFeatures();
  QVERIFY( fit.nextFeature( f ) );
  expected = 88360.0918635;
  QGSCOMPARENEAR( f.attribute( "col1" ).toDouble(), expected, 0.001 );
}

void TestQgsAttributeTable::testFieldCalculationArea()
{
  //test $area field calculation

  //create a temporary layer
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "Polygon?crs=epsg:3111&field=pk:int&field=col1:double" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );
  QgsFeature f1( tempLayer->dataProvider()->fields(), 1 );
  f1.setAttribute( QStringLiteral( "pk" ), 1 );
  f1.setAttribute( QStringLiteral( "col1" ), 0.0 );

  QgsPolylineXY polygonRing3111;
  polygonRing3111 << QgsPointXY( 2484588, 2425722 ) << QgsPointXY( 2482767, 2398853 ) << QgsPointXY( 2520109, 2397715 ) << QgsPointXY( 2520792, 2425494 ) << QgsPointXY( 2484588, 2425722 );
  QgsPolygonXY polygon3111;
  polygon3111 << polygonRing3111;
  QgsGeometry polygon3111G = QgsGeometry::fromPolygonXY( polygon3111 );
  f1.setGeometry( polygon3111G );
  tempLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 );

  // set project CRS and ellipsoid
  QgsCoordinateReferenceSystem srs( 3111, QgsCoordinateReferenceSystem::EpsgCrsId );
  QgsProject::instance()->setCrs( srs );
  QgsProject::instance()->setEllipsoid( QStringLiteral( "WGS84" ) );
  QgsProject::instance()->setAreaUnits( QgsUnitTypes::AreaSquareMeters );

  // run area calculation
  std::unique_ptr< QgsAttributeTableDialog > dlg( new QgsAttributeTableDialog( tempLayer.get() ) );
  tempLayer->startEditing();
  dlg->runFieldCalculation( tempLayer.get(), QStringLiteral( "col1" ), QStringLiteral( "$area" ) );
  tempLayer->commitChanges();
  // check result
  QgsFeatureIterator fit = tempLayer->dataProvider()->getFeatures();
  QgsFeature f;
  QVERIFY( fit.nextFeature( f ) );
  double expected = 1005721496.78008;
  QGSCOMPARENEAR( f.attribute( "col1" ).toDouble(), expected, 1.0 );

  // change project area unit, check calculation respects unit
  QgsProject::instance()->setAreaUnits( QgsUnitTypes::AreaSquareMiles );
  std::unique_ptr< QgsAttributeTableDialog > dlg2( new QgsAttributeTableDialog( tempLayer.get() ) );
  tempLayer->startEditing();
  dlg2->runFieldCalculation( tempLayer.get(), QStringLiteral( "col1" ), QStringLiteral( "$area" ) );
  tempLayer->commitChanges();
  // check result
  fit = tempLayer->dataProvider()->getFeatures();
  QVERIFY( fit.nextFeature( f ) );
  expected = 388.311240;
  QGSCOMPARENEAR( f.attribute( "col1" ).toDouble(), expected, 0.001 );
}

void TestQgsAttributeTable::testNoGeom()
{
  QgsSettings s;

  //test that by default the attribute table DOESN'T fetch geometries (because performance)
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "LineString?crs=epsg:3111&field=pk:int&field=col1:double" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );

  std::unique_ptr< QgsAttributeTableDialog > dlg( new QgsAttributeTableDialog( tempLayer.get(), QgsAttributeTableFilterModel::ShowAll ) );

  QVERIFY( !dlg->mMainView->masterModel()->layerCache()->cacheGeometry() );
  QVERIFY( dlg->mMainView->masterModel()->request().flags() & QgsFeatureRequest::NoGeometry );

  // but if we are requesting only visible features, then geometry must be fetched...

  dlg.reset( new QgsAttributeTableDialog( tempLayer.get(), QgsAttributeTableFilterModel::ShowVisible ) );
  QVERIFY( dlg->mMainView->masterModel()->layerCache()->cacheGeometry() );
  QVERIFY( !( dlg->mMainView->masterModel()->request().flags() & QgsFeatureRequest::NoGeometry ) );

  // try changing existing dialog to no geometry mode
  dlg->filterShowAll();
  QVERIFY( !dlg->mMainView->masterModel()->layerCache()->cacheGeometry() );
  QVERIFY( dlg->mMainView->masterModel()->request().flags() & QgsFeatureRequest::NoGeometry );

  // and back to a geometry mode
  dlg->filterVisible();
  QVERIFY( dlg->mMainView->masterModel()->layerCache()->cacheGeometry() );
  QVERIFY( !( dlg->mMainView->masterModel()->request().flags() & QgsFeatureRequest::NoGeometry ) );

}

void TestQgsAttributeTable::testSelected()
{
  // test attribute table opening in show selected mode
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "LineString?crs=epsg:3111&field=pk:int&field=col1:double" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );

  QgsFeature f1( tempLayer->dataProvider()->fields(), 1 );
  QgsFeature f2( tempLayer->dataProvider()->fields(), 2 );
  QgsFeature f3( tempLayer->dataProvider()->fields(), 3 );
  QVERIFY( tempLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 << f2 << f3 ) );

  std::unique_ptr< QgsAttributeTableDialog > dlg( new QgsAttributeTableDialog( tempLayer.get(), QgsAttributeTableFilterModel::ShowSelected ) );

  QVERIFY( !dlg->mMainView->masterModel()->layerCache()->cacheGeometry() );
  //should be nothing - because no selection!
  QCOMPARE( dlg->mMainView->masterModel()->request().filterType(), QgsFeatureRequest::FilterFids );
  QVERIFY( dlg->mMainView->masterModel()->request().filterFids().isEmpty() );

  // make a selection
  tempLayer->selectByIds( QgsFeatureIds() << 1 << 3 );
  QCOMPARE( dlg->mMainView->masterModel()->request().filterType(), QgsFeatureRequest::FilterFids );
  QCOMPARE( dlg->mMainView->masterModel()->request().filterFids(), QgsFeatureIds() << 1 << 3 );

  // another test - start with selection when dialog created
  dlg.reset( new QgsAttributeTableDialog( tempLayer.get(), QgsAttributeTableFilterModel::ShowSelected ) );
  QVERIFY( !dlg->mMainView->masterModel()->layerCache()->cacheGeometry() );
  QCOMPARE( dlg->mMainView->masterModel()->request().filterType(), QgsFeatureRequest::FilterFids );
  QCOMPARE( dlg->mMainView->masterModel()->request().filterFids(), QgsFeatureIds() << 1 << 3 );
  // remove selection
  tempLayer->removeSelection();
  QCOMPARE( dlg->mMainView->masterModel()->request().filterType(), QgsFeatureRequest::FilterFids );
  QVERIFY( dlg->mMainView->masterModel()->request().filterFids().isEmpty() );
}

void TestQgsAttributeTable::testSortByDisplayExpression()
{
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "LineString?crs=epsg:3111&field=pk:int&field=col1:double" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );

  QgsFeature f1( tempLayer->dataProvider()->fields(), 1 );
  f1.setAttribute( 0, 1 );
  f1.setAttribute( 1, 3.2 );
  QgsFeature f2( tempLayer->dataProvider()->fields(), 2 );
  f2.setAttribute( 0, 2 );
  f2.setAttribute( 1, 1.8 );
  QgsFeature f3( tempLayer->dataProvider()->fields(), 3 );
  f3.setAttribute( 0, 3 );
  f3.setAttribute( 1, 5.0 );
  QVERIFY( tempLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 << f2 << f3 ) );

  std::unique_ptr< QgsAttributeTableDialog > dlg( new QgsAttributeTableDialog( tempLayer.get() ) );

  dlg->mMainView->mFeatureList->setDisplayExpression( "pk" );
  QgsFeatureListModel *listModel = dlg->mMainView->mFeatureListModel;
  QCOMPARE( listModel->rowCount(), 3 );

  QCOMPARE( listModel->index( 0, 0 ).data( Qt::DisplayRole ), QVariant( 1 ) );
  QCOMPARE( listModel->index( 1, 0 ).data( Qt::DisplayRole ), QVariant( 2 ) );
  QCOMPARE( listModel->index( 2, 0 ).data( Qt::DisplayRole ), QVariant( 3 ) );

  dlg->mMainView->mFeatureList->setDisplayExpression( "col1" );
  QCOMPARE( listModel->index( 0, 0 ).data( Qt::DisplayRole ), QVariant( 1.8 ) );
  QCOMPARE( listModel->index( 1, 0 ).data( Qt::DisplayRole ), QVariant( 3.2 ) );
  QCOMPARE( listModel->index( 2, 0 ).data( Qt::DisplayRole ), QVariant( 5.0 ) );
}

void TestQgsAttributeTable::testRegression15974()
{
  // Test duplicated rows in attribute table + two crashes.
  QString path = QDir::tempPath() + "/testshp15974.shp";
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "polygon?crs=epsg:4326&field=id:integer" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );
  QgsVectorFileWriter::writeAsVectorFormat( tempLayer.get(), path, QStringLiteral( "system" ), QgsCoordinateReferenceSystem( 4326 ), QStringLiteral( "ESRI Shapefile" ) );
  std::unique_ptr< QgsVectorLayer> shpLayer( new QgsVectorLayer( path, QStringLiteral( "test" ),  QStringLiteral( "ogr" ) ) );
  QgsFeature f1( shpLayer->dataProvider()->fields(), 1 );
  QgsGeometry geom;
  geom = QgsGeometry().fromWkt( QStringLiteral( "polygon((0 0, 0 1, 1 1, 1 0, 0 0))" ) );
  QVERIFY( geom.isGeosValid() );
  f1.setGeometry( geom );
  QgsFeature f2( shpLayer->dataProvider()->fields(), 2 );
  f2.setGeometry( geom );
  QgsFeature f3( shpLayer->dataProvider()->fields(), 3 );
  f3.setGeometry( geom );
  QVERIFY( shpLayer->startEditing() );
  QVERIFY( shpLayer->addFeatures( QgsFeatureList() << f1 << f2 << f3 ) );
  std::unique_ptr< QgsAttributeTableDialog > dlg( new QgsAttributeTableDialog( shpLayer.get() ) );
  QCOMPARE( shpLayer->featureCount(), 3L );
  mQgisApp->saveEdits( shpLayer.get() );
  QCOMPARE( shpLayer->featureCount(), 3L );
  QCOMPARE( dlg->mMainView->masterModel()->rowCount(), 3 );
  QCOMPARE( dlg->mMainView->mLayerCache->cachedFeatureIds().count(), 3 );
  QCOMPARE( dlg->mMainView->featureCount(), 3 );
  // All the following instructions made the test pass, before the connections to invalidate()
  // were introduced in QgsDualView::initModels
  // dlg->mMainView->mFilterModel->setSourceModel( dlg->mMainView->masterModel() );
  // dlg->mMainView->mFilterModel->invalidate();
  QCOMPARE( dlg->mMainView->filteredFeatureCount(), 3 );
}

void TestQgsAttributeTable::testOrderColumn()
{
  std::unique_ptr< QgsVectorLayer> tempLayer( new QgsVectorLayer( QStringLiteral( "LineString?crs=epsg:3111&field=pk:int&field=col1:int&field=col2:int" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );
  QVERIFY( tempLayer->isValid() );

  QgsFeature f1( tempLayer->dataProvider()->fields(), 1 );
  f1.setAttribute( 0, 1 );
  f1.setAttribute( 1, 13 );
  f1.setAttribute( 2, 7 );
  QVERIFY( tempLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 ) );

  std::unique_ptr< QgsAttributeTableDialog > dlg( new QgsAttributeTableDialog( tempLayer.get() ) );

  // Issue https://issues.qgis.org/issues/20673
  // When we reorder column (last column becomes first column), and we select an entire row
  // the currentIndex is no longer the first column, and consequently it breaks edition

  QgsAttributeTableConfig config = QgsAttributeTableConfig();
  config.update( tempLayer->dataProvider()->fields() );
  QVector<QgsAttributeTableConfig::ColumnConfig> columns = config.columns();

  // move last column in first position
  columns.move( 2, 0 );
  config.setColumns( columns );

  dlg->mMainView->setAttributeTableConfig( config );

  QgsAttributeTableFilterModel *filterModel = static_cast<QgsAttributeTableFilterModel *>( dlg->mMainView->mTableView->model() );
  filterModel->sort( 0, Qt::AscendingOrder );

  QModelIndex index = filterModel->mapToSource( filterModel->sourceModel()->index( 0, 0 ) );
  QCOMPARE( index.row(), 0 );
  QCOMPARE( index.column(), 2 );

  index = filterModel->mapFromSource( filterModel->sourceModel()->index( 0, 0 ) );
  QCOMPARE( index.row(), 0 );
  QCOMPARE( index.column(), 1 );

  qDebug() << filterModel->mapFromSource( filterModel->sourceModel()->index( 0, 0 ) );

  // column 0 is indeed column 2 since we move it
  QCOMPARE( filterModel->sortColumn(), 2 );
}

QGSTEST_MAIN( TestQgsAttributeTable )
#include "testqgsattributetable.moc"
