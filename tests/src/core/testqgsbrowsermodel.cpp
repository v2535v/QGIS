/***************************************************************************
     testqgsbrowsermodel.cpp
     --------------------------------------
    Date                 : October 2018
    Copyright            : (C) 2018 Nyall Dawson
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

#include <QObject>
#include <QString>
#include <QStringList>

//qgis includes...
#include "qgsdataitem.h"
#include "qgsvectorlayer.h"
#include "qgsapplication.h"
#include "qgslogger.h"
#include "qgssettings.h"
#include "qgsbrowsermodel.h"

class TestQgsBrowserModel : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init() {} // will be called before each testfunction is executed.
    void cleanup() {} // will be called after every testfunction.

    void testModel();
    void driveItems();

};

void TestQgsBrowserModel::initTestCase()
{
  //
  // Runs once before any tests are run
  //
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();
  QgsApplication::showSettings();

  // Set up the QgsSettings environment
  QCoreApplication::setOrganizationName( QStringLiteral( "QGIS" ) );
  QCoreApplication::setOrganizationDomain( QStringLiteral( "qgis.org" ) );
  QCoreApplication::setApplicationName( QStringLiteral( "QGIS-TEST" ) );
}

void TestQgsBrowserModel::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsBrowserModel::testModel()
{
  QgsBrowserModel model;

  // empty
  QCOMPARE( model.rowCount(), 0 );
  QCOMPARE( model.columnCount(), 1 );
  QVERIFY( !model.data( QModelIndex() ).isValid() );
  QVERIFY( !model.flags( QModelIndex() ) );
  QVERIFY( !model.hasChildren() );
  QVERIFY( !model.dataItem( QModelIndex() ) );

  // add a root child
  QgsDataCollectionItem *rootItem1 = new QgsDataCollectionItem( nullptr, QStringLiteral( "Test" ), QStringLiteral( "root1" ) );
  QVERIFY( !model.findItem( rootItem1 ).isValid() );
  model.setupItemConnections( rootItem1 );
  model.mRootItems.append( rootItem1 );

  QCOMPARE( model.rowCount(), 1 );
  QCOMPARE( model.columnCount(), 1 );
  QVERIFY( !model.data( QModelIndex() ).isValid() );
  QVERIFY( !model.flags( QModelIndex() ) );
  QVERIFY( model.hasChildren() );
  QModelIndex root1Index = model.index( 0, 0 );
  QVERIFY( root1Index.isValid() );
  QCOMPARE( model.rowCount( root1Index ), 0 );
  QCOMPARE( model.columnCount( root1Index ), 1 );
  // initially, we say the item has children, until it's populated and we know for sure
  QVERIFY( model.hasChildren( root1Index ) );
  rootItem1->setState( QgsDataItem::Populated );
  QVERIFY( !model.hasChildren( root1Index ) );
  QCOMPARE( model.data( root1Index ).toString(), QStringLiteral( "Test" ) );
  QCOMPARE( model.data( root1Index, QgsBrowserModel::PathRole ).toString(), QStringLiteral( "root1" ) );
  QCOMPARE( model.dataItem( root1Index ), rootItem1 );
  QCOMPARE( model.findItem( rootItem1 ), root1Index );

  // second root item
  QgsDataCollectionItem *rootItem2 = new QgsDataCollectionItem( nullptr, QStringLiteral( "Test2" ), QStringLiteral( "root2" ) );
  model.setupItemConnections( rootItem2 );
  model.mRootItems.append( rootItem2 );

  QCOMPARE( model.rowCount(), 2 );
  QVERIFY( model.hasChildren() );
  QModelIndex root2Index = model.index( 1, 0 );
  QVERIFY( root2Index.isValid() );
  QCOMPARE( model.rowCount( root2Index ), 0 );
  QCOMPARE( model.columnCount( root2Index ), 1 );
  QCOMPARE( model.data( root2Index ).toString(), QStringLiteral( "Test2" ) );
  QCOMPARE( model.data( root2Index, QgsBrowserModel::PathRole ).toString(), QStringLiteral( "root2" ) );
  QCOMPARE( model.dataItem( root2Index ), rootItem2 );
  QCOMPARE( model.findItem( rootItem2 ), root2Index );

  // child item
  QgsDataCollectionItem *childItem1 = new QgsDataCollectionItem( nullptr, QStringLiteral( "Child1" ), QStringLiteral( "child1" ) );
  model.setupItemConnections( childItem1 );
  rootItem1->addChild( childItem1 );

  QCOMPARE( model.rowCount(), 2 );
  QCOMPARE( model.columnCount(), 1 );
  QCOMPARE( model.rowCount( root1Index ), 1 );
  QCOMPARE( model.columnCount( root1Index ), 1 );
  QVERIFY( model.hasChildren( root1Index ) );
  QModelIndex child1Index = model.index( 0, 0, root1Index );
  QCOMPARE( model.data( child1Index ).toString(), QStringLiteral( "Child1" ) );
  QCOMPARE( model.data( child1Index, QgsBrowserModel::PathRole ).toString(), QStringLiteral( "child1" ) );
  QCOMPARE( model.dataItem( child1Index ), childItem1 );
  QCOMPARE( model.findItem( childItem1 ), child1Index );
  QCOMPARE( model.findItem( childItem1, rootItem1 ), child1Index );
  // search for child in wrong parent
  QVERIFY( !model.findItem( childItem1, rootItem2 ).isValid() );


  // more children
  QgsDataCollectionItem *childItem2 = new QgsDataCollectionItem( nullptr, QStringLiteral( "Child2" ), QStringLiteral( "child2" ) );
  rootItem1->addChildItem( childItem2, true );

  QgsDataCollectionItem *childItem3 = new QgsDataCollectionItem( nullptr, QStringLiteral( "Child3" ), QStringLiteral( "child3" ) );
  childItem2->addChildItem( childItem3, true );
  QCOMPARE( childItem2->rowCount(), 1 );

  QgsDataCollectionItem *childItem4 = new QgsDataCollectionItem( nullptr, QStringLiteral( "Child4" ), QStringLiteral( "child4" ) );
  rootItem2->addChildItem( childItem4, true );

  QCOMPARE( model.rowCount(), 2 );
  root1Index = model.index( 0, 0 );
  root2Index = model.index( 1, 0 );
  QCOMPARE( model.rowCount( root1Index ), 2 );
  child1Index = model.index( 0, 0, root1Index );
  QCOMPARE( model.data( child1Index ).toString(), QStringLiteral( "Child1" ) );
  QModelIndex child2Index = model.index( 1, 0, root1Index );
  QCOMPARE( model.data( child2Index ).toString(), QStringLiteral( "Child2" ) );
  QCOMPARE( model.rowCount( child1Index ), 0 );
  QCOMPARE( model.dataItem( child2Index ), childItem2 );
  QCOMPARE( childItem2->rowCount(), 1 );
  QCOMPARE( model.rowCount( child2Index ), 1 );
  QCOMPARE( model.data( model.index( 0, 0, child2Index ) ).toString(), QStringLiteral( "Child3" ) );
  QCOMPARE( model.rowCount( root2Index ), 1 );
  QCOMPARE( model.data( model.index( 0, 0, root2Index ) ).toString(), QStringLiteral( "Child4" ) );
}

void TestQgsBrowserModel::driveItems()
{
  // an unapologetically linux-directed test ;)
  QgsBrowserModel model;
  QVERIFY( model.driveItems().empty() );

  model.initialize();
  QVERIFY( !model.driveItems().empty() );
  QVERIFY( model.driveItems().contains( QStringLiteral( "/" ) ) );
  QgsDirectoryItem *rootItem = model.driveItems().value( QStringLiteral( "/" ) );
  QVERIFY( rootItem );
  QCOMPARE( rootItem->path(), QStringLiteral( "/" ) );
}

QGSTEST_MAIN( TestQgsBrowserModel )
#include "testqgsbrowsermodel.moc"
