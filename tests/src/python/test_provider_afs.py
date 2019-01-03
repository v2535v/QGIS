# -*- coding: utf-8 -*-
"""QGIS Unit tests for the AFS provider.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Nyall Dawson'
__date__ = '2018-02-16'
__copyright__ = 'Copyright 2018, Nyall Dawson'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import hashlib
import os
import re
import tempfile
import shutil

from qgis.PyQt.QtCore import QCoreApplication, Qt, QObject, QDate

from qgis.core import (NULL,
                       QgsVectorLayer,
                       QgsLayerMetadata,
                       QgsBox3d,
                       QgsCoordinateReferenceSystem,
                       QgsApplication,
                       QgsSettings,
                       QgsRectangle,
                       QgsCategorizedSymbolRenderer,
                       QgsProviderRegistry
                       )
from qgis.testing import (start_app,
                          unittest
                          )
from providertestbase import ProviderTestCase


def sanitize(endpoint, x):
    if x.startswith('/query'):
        x = x[len('/query'):]
        endpoint = endpoint + '_query'

    if len(endpoint + x) > 150:
        ret = endpoint + hashlib.md5(x.encode()).hexdigest()
        # print('Before: ' + endpoint + x)
        # print('After:  ' + ret)
        return ret
    return endpoint + x.replace('?', '_').replace('&', '_').replace('<', '_').replace('>', '_').replace('"', '_').replace("'", '_').replace(' ', '_').replace(':', '_').replace('/', '_').replace('\n', '_')


class MessageLogger(QObject):

    def __init__(self, tag=None):
        QObject.__init__(self)
        self.log = []
        self.tag = tag

    def __enter__(self):
        QgsApplication.messageLog().messageReceived.connect(self.logMessage)
        return self

    def __exit__(self, type, value, traceback):
        QgsApplication.messageLog().messageReceived.disconnect(self.logMessage)

    def logMessage(self, msg, tag, level):
        if tag == self.tag or not self.tag:
            self.log.append(msg.encode('UTF-8'))

    def messages(self):
        return self.log


class TestPyQgsAFSProvider(unittest.TestCase, ProviderTestCase):

    @classmethod
    def setUpClass(cls):
        """Run before all tests"""

        QCoreApplication.setOrganizationName("QGIS_Test")
        QCoreApplication.setOrganizationDomain("TestPyQgsAFSProvider.com")
        QCoreApplication.setApplicationName("TestPyQgsAFSProvider")
        QgsSettings().clear()
        start_app()

        # On Windows we must make sure that any backslash in the path is
        # replaced by a forward slash so that QUrl can process it
        cls.basetestpath = tempfile.mkdtemp().replace('\\', '/')
        endpoint = cls.basetestpath + '/fake_qgis_http_endpoint'
        with open(sanitize(endpoint, '?f=json'), 'wb') as f:
            f.write("""
{"currentVersion":10.22,"id":1,"name":"QGIS Test","type":"Feature Layer","description":
"QGIS Provider Test Layer.\n","geometryType":"esriGeometryPoint","copyrightText":"","parentLayer":{"id":0,"name":"QGIS Tests"},"subLayers":[],
"minScale":72225,"maxScale":0,
"defaultVisibility":true,
"extent":{"xmin":-71.123,"ymin":66.33,"xmax":-65.32,"ymax":78.3,
"spatialReference":{"wkid":4326,"latestWkid":4326}},
"hasAttachments":false,"htmlPopupType":"esriServerHTMLPopupTypeAsHTMLText",
"displayField":"LABEL","typeIdField":null,
"fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null},
{"name":"pk","type":"esriFieldTypeInteger","alias":"pk","domain":null},
{"name":"cnt","type":"esriFieldTypeInteger","alias":"cnt","domain":null},
{"name":"name","type":"esriFieldTypeString","alias":"name","length":100,"domain":null},
{"name":"name2","type":"esriFieldTypeString","alias":"name2","length":100,"domain":null},
{"name":"num_char","type":"esriFieldTypeString","alias":"num_char","length":100,"domain":null},
{"name":"Shape","type":"esriFieldTypeGeometry","alias":"Shape","domain":null}],
"relationships":[],"canModifyLayer":false,"canScaleSymbols":false,"hasLabels":false,
"capabilities":"Map,Query,Data","maxRecordCount":1000,"supportsStatistics":true,
"supportsAdvancedQueries":true,"supportedQueryFormats":"JSON, AMF",
"ownershipBasedAccessControlForFeatures":{"allowOthersToQuery":true},"useStandardizedQueries":true}""".encode('UTF-8'))

        with open(sanitize(endpoint, '/query?f=json_where=OBJECTID=OBJECTID_returnIdsOnly=true'), 'wb') as f:
            f.write("""
{
 "objectIdFieldName": "OBJECTID",
 "objectIds": [
  5,
  3,
  1,
  2,
  4
 ]
}
""".encode('UTF-8'))

        # Create test layer
        cls.vl = QgsVectorLayer("url='http://" + endpoint + "' crs='epsg:4326'", 'test', 'arcgisfeatureserver')
        assert cls.vl.isValid()
        cls.source = cls.vl.dataProvider()

        with open(sanitize(endpoint,
                           '/query?f=json&objectIds=5,3,1,2,4&inSR=4326&outSR=4326&returnGeometry=true&outFields=OBJECTID,pk,cnt,name,name2,num_char&returnM=false&returnZ=false'),
                  'wb') as f:
            f.write("""
        {
         "displayFieldName": "name",
         "fieldAliases": {
          "name": "name"
         },
         "geometryType": "esriGeometryPoint",
         "spatialReference": {
          "wkid": 4326,
          "latestWkid": 4326
         },
         "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null},
        {"name":"pk","type":"esriFieldTypeInteger","alias":"pk","domain":null},
        {"name":"cnt","type":"esriFieldTypeInteger","alias":"cnt","domain":null},
        {"name":"name","type":"esriFieldTypeString","alias":"name","length":100,"domain":null},
        {"name":"name2","type":"esriFieldTypeString","alias":"name2","length":100,"domain":null},
        {"name":"num_char","type":"esriFieldTypeString","alias":"num_char","length":100,"domain":null},
        {"name":"Shape","type":"esriFieldTypeGeometry","alias":"Shape","domain":null}],
         "features": [
          {
           "attributes": {
            "OBJECTID": 5,
            "pk": 5,
            "cnt": -200,
            "name": null,
            "name2":"NuLl",
            "num_char":"5"    
           },
           "geometry": {
            "x": -71.123,
            "y": 78.23
           }
          },
          {
           "attributes": {
            "OBJECTID": 3,
            "pk": 3,
            "cnt": 300,
            "name": "Pear",
            "name2":"PEaR",
            "num_char":"3"   
           },
           "geometry": null
          },
          {
           "attributes": {
            "OBJECTID": 1,
            "pk": 1,
            "cnt": 100,
            "name": "Orange",
            "name2":"oranGe",
            "num_char":"1"    
           },
           "geometry": {
            "x": -70.332,
            "y": 66.33
           }
          },
          {
           "attributes": {
            "OBJECTID": 2,
            "pk": 2,
            "cnt": 200,
            "name": "Apple",
            "name2":"Apple",
            "num_char":"2"    
           },
           "geometry": {
            "x": -68.2,
            "y": 70.8
           }
          },
          {
           "attributes": {
            "OBJECTID": 4,
            "pk": 4,
            "cnt": 400,
            "name": "Honey",
            "name2":"Honey",
            "num_char":"4"    
           },
           "geometry": {
            "x": -65.32,
            "y": 78.3
           }
          }
         ]
        }""".encode('UTF-8'))

        with open(sanitize(endpoint, '/query?f=json&objectIds=5,3,1,2,4&inSR=4326&outSR=4326&returnGeometry=true&outFields=OBJECTID,pk,cnt,name,name2,num_char&returnM=false&returnZ=false&geometry=-71.123000,66.330000,-65.320000,78.300000&geometryType=esriGeometryEnvelope&spatialRel=esriSpatialRelEnvelopeIntersects'), 'wb') as f:
            f.write("""
{
 "displayFieldName": "name",
 "fieldAliases": {
  "name": "name"
 },
 "geometryType": "esriGeometryPoint",
 "spatialReference": {
  "wkid": 4326,
  "latestWkid": 4326
 },
 "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null},
{"name":"pk","type":"esriFieldTypeInteger","alias":"pk","domain":null},
{"name":"cnt","type":"esriFieldTypeInteger","alias":"cnt","domain":null},
{"name":"name","type":"esriFieldTypeString","alias":"name","length":100,"domain":null},
{"name":"name2","type":"esriFieldTypeString","alias":"name2","length":100,"domain":null},
{"name":"num_char","type":"esriFieldTypeString","alias":"num_char","length":100,"domain":null},
{"name":"Shape","type":"esriFieldTypeGeometry","alias":"Shape","domain":null}],
 "features": [
  {
   "attributes": {
    "OBJECTID": 5,
    "pk": 5,
    "cnt": -200,
    "name": null,
    "name2":"NuLl",
    "num_char":"5"    
   },
   "geometry": {
    "x": -71.123,
    "y": 78.23
   }
  },
  {
   "attributes": {
    "OBJECTID": 3,
    "pk": 3,
    "cnt": 300,
    "name": "Pear",
    "name2":"PEaR",
    "num_char":"3"   
   },
   "geometry": null
  },
  {
   "attributes": {
    "OBJECTID": 1,
    "pk": 1,
    "cnt": 100,
    "name": "Orange",
    "name2":"oranGe",
    "num_char":"1"    
   },
   "geometry": {
    "x": -70.332,
    "y": 66.33
   }
  },
  {
   "attributes": {
    "OBJECTID": 2,
    "pk": 2,
    "cnt": 200,
    "name": "Apple",
    "name2":"Apple",
    "num_char":"2"    
   },
   "geometry": {
    "x": -68.2,
    "y": 70.8
   }
  },
  {
   "attributes": {
    "OBJECTID": 4,
    "pk": 4,
    "cnt": 400,
    "name": "Honey",
    "name2":"Honey",
    "num_char":"4"    
   },
   "geometry": {
    "x": -65.32,
    "y": 78.3
   }
  }
 ]
}""".encode('UTF-8'))

        with open(sanitize(endpoint,
                           '/query?f=json&objectIds=2,4&inSR=4326&outSR=4326&returnGeometry=true&outFields=OBJECTID,pk,cnt,name,name2,num_char&returnM=false&returnZ=false'),
                  'wb') as f:
            f.write("""
        {
         "displayFieldName": "name",
         "fieldAliases": {
          "name": "name"
         },
         "geometryType": "esriGeometryPoint",
         "spatialReference": {
          "wkid": 4326,
          "latestWkid": 4326
         },
         "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null},
        {"name":"pk","type":"esriFieldTypeInteger","alias":"pk","domain":null},
        {"name":"cnt","type":"esriFieldTypeInteger","alias":"cnt","domain":null},
        {"name":"name","type":"esriFieldTypeString","alias":"name","length":100,"domain":null},
        {"name":"name2","type":"esriFieldTypeString","alias":"name2","length":100,"domain":null},
        {"name":"num_char","type":"esriFieldTypeString","alias":"num_char","length":100,"domain":null},
        {"name":"Shape","type":"esriFieldTypeGeometry","alias":"Shape","domain":null}],
         "features": [
          {
           "attributes": {
            "OBJECTID": 2,
            "pk": 2,
            "cnt": 200,
            "name": "Apple",
            "name2":"Apple",
            "num_char":"2"
           },
           "geometry": {
            "x": -68.2,
            "y": 70.8
           }
          },
          {
           "attributes": {
            "OBJECTID": 4,
            "pk": 4,
            "cnt": 400,
            "name": "Honey",
            "name2":"Honey",
            "num_char":"4"
           },
           "geometry": {
            "x": -65.32,
            "y": 78.3
           }
          }
         ]
        }""".encode('UTF-8'))

        with open(sanitize(endpoint, '/query?f=json&where=OBJECTID=OBJECTID&returnIdsOnly=true&geometry=-70.000000,67.000000,-60.000000,80.000000&geometryType=esriGeometryEnvelope&spatialRel=esriSpatialRelEnvelopeIntersects'), 'wb') as f:
            f.write("""
        {
         "objectIdFieldName": "OBJECTID",
         "objectIds": [
          2,
          4
         ]
        }
        """.encode('UTF-8'))

        with open(sanitize(endpoint, '/query?f=json&where=OBJECTID=OBJECTID&returnIdsOnly=true&geometry=-73.000000,70.000000,-63.000000,80.000000&geometryType=esriGeometryEnvelope&spatialRel=esriSpatialRelEnvelopeIntersects'), 'wb') as f:
            f.write("""
        {
         "objectIdFieldName": "OBJECTID",
         "objectIds": [
          2,
          4
         ]
        }
        """.encode('UTF-8'))

        with open(sanitize(endpoint, '/query?f=json&where=OBJECTID=OBJECTID&returnIdsOnly=true&geometry=-68.721119,68.177676,-64.678700,79.123755&geometryType=esriGeometryEnvelope&spatialRel=esriSpatialRelEnvelopeIntersects'), 'wb') as f:
            f.write("""
        {
         "objectIdFieldName": "OBJECTID",
         "objectIds": [
          2,
          4
         ]
        }
        """.encode('UTF-8'))

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""
        QgsSettings().clear()
        #shutil.rmtree(cls.basetestpath, True)
        cls.vl = None  # so as to properly close the provider and remove any temporary file

    def testGetFeaturesSubsetAttributes2(self):
        """ Override and skip this test for AFS provider, as it's actually more efficient for the AFS provider to return
        its features as direct copies (due to implicit sharing of QgsFeature), and the nature of the caching
        used by the AFS provider.
        """
        pass

    def testGetFeaturesNoGeometry(self):
        """ Override and skip this test for AFS provider, as it's actually more efficient for the AFS provider to return
        its features as direct copies (due to implicit sharing of QgsFeature), and the nature of the caching
        used by the AFS provider.
        """
        pass

    def testDecodeUri(self):
        """
        Test decoding an AFS uri
        """
        uri = self.vl.source()
        parts = QgsProviderRegistry.instance().decodeUri(self.vl.dataProvider().name(), uri)
        self.assertEqual(parts, {'url': 'http://' + self.basetestpath + '/fake_qgis_http_endpoint'})

    def testObjectIdDifferentName(self):
        """ Test that object id fields not named OBJECTID work correctly """

        endpoint = self.basetestpath + '/oid_fake_qgis_http_endpoint'
        with open(sanitize(endpoint, '?f=json'), 'wb') as f:
            f.write("""
        {"currentVersion":10.22,"id":1,"name":"QGIS Test","type":"Feature Layer","description":
        "QGIS Provider Test Layer.\n","geometryType":"esriGeometryPoint","copyrightText":"","parentLayer":{"id":0,"name":"QGIS Tests"},"subLayers":[],
        "minScale":72225,"maxScale":0,
        "defaultVisibility":true,
        "extent":{"xmin":-71.123,"ymin":66.33,"xmax":-65.32,"ymax":78.3,
        "spatialReference":{"wkid":4326,"latestWkid":4326}},
        "hasAttachments":false,"htmlPopupType":"esriServerHTMLPopupTypeAsHTMLText",
        "displayField":"LABEL","typeIdField":null,
        "fields":[{"name":"OBJECTID1","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null},
        {"name":"pk","type":"esriFieldTypeInteger","alias":"pk","domain":null},
        {"name":"cnt","type":"esriFieldTypeInteger","alias":"cnt","domain":null}],
        "relationships":[],"canModifyLayer":false,"canScaleSymbols":false,"hasLabels":false,
        "capabilities":"Map,Query,Data","maxRecordCount":1000,"supportsStatistics":true,
        "supportsAdvancedQueries":true,"supportedQueryFormats":"JSON, AMF",
        "ownershipBasedAccessControlForFeatures":{"allowOthersToQuery":true},"useStandardizedQueries":true}""".encode(
                'UTF-8'))

        with open(sanitize(endpoint, '/query?f=json_where=OBJECTID1=OBJECTID1_returnIdsOnly=true'), 'wb') as f:
            f.write("""
        {
         "objectIdFieldName": "OBJECTID1",
         "objectIds": [
          5,
          3,
          1,
          2,
          4
         ]
        }
        """.encode('UTF-8'))

        with open(sanitize(endpoint, '/query?f=json&objectIds=5,3,1,2,4&inSR=4326&outSR=4326&returnGeometry=true&outFields=OBJECTID1,pk,cnt&returnM=false&returnZ=false'), 'wb') as f:
            f.write("""
        {
         "displayFieldName": "LABEL",
         "geometryType": "esriGeometryPoint",
         "spatialReference": {
          "wkid": 4326,
          "latestWkid": 4326
         },
         "fields":[{"name":"OBJECTID1","type":"esriFieldTypeOID","alias":"OBJECTID1","domain":null},
          {"name":"pk","type":"esriFieldTypeInteger","alias":"pk","domain":null},
          {"name":"cnt","type":"esriFieldTypeInteger","alias":"cnt","domain":null},
          {"name":"Shape","type":"esriFieldTypeGeometry","alias":"Shape","domain":null}],
         "features": [
          {
           "attributes": {
            "OBJECTID1": 5,
            "pk": 5,
            "cnt": -200,
            "name": null
           },
           "geometry": {
            "x": -71.123,
            "y": 78.23
           }
          }
         ]
        }""".encode('UTF-8'))

        # Create test layer
        vl = QgsVectorLayer("url='http://" + endpoint + "' crs='epsg:4326'", 'test', 'arcgisfeatureserver')
        assert vl.isValid()

        f = vl.getFeature(0)
        assert f.isValid()

    def testDateTime(self):
        """ Test that datetime fields work correctly """

        endpoint = self.basetestpath + '/oid_fake_qgis_http_endpoint'
        with open(sanitize(endpoint, '?f=json'), 'wb') as f:
            f.write("""
        {"currentVersion":10.22,"id":1,"name":"QGIS Test","type":"Feature Layer","description":
        "QGIS Provider Test Layer.\n","geometryType":"esriGeometryPoint","copyrightText":"","parentLayer":{"id":0,"name":"QGIS Tests"},"subLayers":[],
        "minScale":72225,"maxScale":0,
        "defaultVisibility":true,
        "extent":{"xmin":-71.123,"ymin":66.33,"xmax":-65.32,"ymax":78.3,
        "spatialReference":{"wkid":4326,"latestWkid":4326}},
        "hasAttachments":false,"htmlPopupType":"esriServerHTMLPopupTypeAsHTMLText",
        "displayField":"LABEL","typeIdField":null,
        "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null},
        {"name":"pk","type":"esriFieldTypeInteger","alias":"pk","domain":null},
        {"name":"dt","type":"esriFieldTypeDate","alias":"dt","length":8,"domain":null}],
        "relationships":[],"canModifyLayer":false,"canScaleSymbols":false,"hasLabels":false,
        "capabilities":"Map,Query,Data","maxRecordCount":1000,"supportsStatistics":true,
        "supportsAdvancedQueries":true,"supportedQueryFormats":"JSON, AMF",
        "ownershipBasedAccessControlForFeatures":{"allowOthersToQuery":true},"useStandardizedQueries":true}""".encode(
                'UTF-8'))

        with open(sanitize(endpoint, '/query?f=json_where=OBJECTID=OBJECTID_returnIdsOnly=true'), 'wb') as f:
            f.write("""
        {
         "objectIdFieldName": "OBJECTID",
         "objectIds": [
          1,
          2
         ]
        }
        """.encode('UTF-8'))

        # Create test layer
        vl = QgsVectorLayer("url='http://" + endpoint + "' crs='epsg:4326'", 'test', 'arcgisfeatureserver')

        self.assertTrue(vl.isValid())
        with open(sanitize(endpoint,
                           '/query?f=json&objectIds=1,2&inSR=4326&outSR=4326&returnGeometry=true&outFields=OBJECTID,pk,dt&returnM=false&returnZ=false'), 'wb') as f:
            f.write("""
        {
         "displayFieldName": "name",
         "fieldAliases": {
          "name": "name"
         },
         "geometryType": "esriGeometryPoint",
         "spatialReference": {
          "wkid": 4326,
          "latestWkid": 4326
         },
         "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null},
        {"name":"pk","type":"esriFieldTypeInteger","alias":"pk","domain":null},
        {"name":"dt","type":"esriFieldTypeDate","alias":"dt","domain":null},
        {"name":"Shape","type":"esriFieldTypeGeometry","alias":"Shape","domain":null}],
         "features": [
          {
           "attributes": {
            "OBJECTID": 1,
            "pk": 1,
            "dt":1493769600000
           },
           "geometry": {
            "x": -70.332,
            "y": 66.33
           }
          },
          {
           "attributes": {
            "OBJECTID": 2,
            "pk": 2,
            "dt":null
           },
           "geometry": {
            "x": -68.2,
            "y": 70.8
           }
          }
         ]
        }""".encode('UTF-8'))

        features = [f for f in vl.getFeatures()]
        self.assertEqual(len(features), 2)
        self.assertEqual([f['dt'] for f in features], [QDate(2017, 5, 3), NULL])

    def testMetadata(self):
        """ Test that metadata is correctly acquired from provider """

        endpoint = self.basetestpath + '/metadata_fake_qgis_http_endpoint'
        with open(sanitize(endpoint, '?f=json'), 'wb') as f:
            f.write("""
        {"currentVersion":10.22,"id":1,"name":"QGIS Test","type":"Feature Layer","description":
        "QGIS Provider Test Layer","geometryType":"esriGeometryPoint","copyrightText":"not copyright","parentLayer":{"id":2,"name":"QGIS Tests"},"subLayers":[],
        "minScale":72225,"maxScale":0,
        "defaultVisibility":true,
        "extent":{"xmin":-71.123,"ymin":66.33,"xmax":-65.32,"ymax":78.3,
        "spatialReference":{"wkid":4326,"latestWkid":4326}},
        "hasAttachments":false,"htmlPopupType":"esriServerHTMLPopupTypeAsHTMLText",
        "displayField":"LABEL","typeIdField":null,
        "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null}],
        "relationships":[],"canModifyLayer":false,"canScaleSymbols":false,"hasLabels":false,
        "capabilities":"Map,Query,Data","maxRecordCount":1000,"supportsStatistics":true,
        "supportsAdvancedQueries":true,"supportedQueryFormats":"JSON, AMF",
        "ownershipBasedAccessControlForFeatures":{"allowOthersToQuery":true},"useStandardizedQueries":true}""".encode(
                'UTF-8'))

        with open(sanitize(endpoint, '/query?f=json_where=OBJECTID=OBJECTID_returnIdsOnly=true'), 'wb') as f:
            f.write("""
        {
         "objectIdFieldName": "OBJECTID",
         "objectIds": [
          1
         ]
        }
        """.encode('UTF-8'))

        # Create test layer
        vl = QgsVectorLayer("url='http://" + endpoint + "' crs='epsg:4326'", 'test', 'arcgisfeatureserver')
        self.assertTrue(vl.isValid())

        extent = QgsLayerMetadata.Extent()
        extent1 = QgsLayerMetadata.SpatialExtent()
        extent1.extentCrs = QgsCoordinateReferenceSystem.fromEpsgId(4326)
        extent1.bounds = QgsBox3d(QgsRectangle(-71.123, 66.33, -65.32, 78.3))
        extent.setSpatialExtents([extent1])
        self.assertEqual(vl.metadata().extent(), extent)

        self.assertEqual(vl.metadata().crs(), QgsCoordinateReferenceSystem.fromEpsgId(4326))
        self.assertEqual(vl.metadata().identifier(), 'http://' + sanitize(endpoint, ''))
        self.assertEqual(vl.metadata().parentIdentifier(), 'http://' + self.basetestpath + '/2')
        self.assertEqual(vl.metadata().type(), 'dataset')
        self.assertEqual(vl.metadata().abstract(), 'QGIS Provider Test Layer')
        self.assertEqual(vl.metadata().title(), 'QGIS Test')
        self.assertEqual(vl.metadata().rights(), ['not copyright'])
        l = QgsLayerMetadata.Link()
        l.name = 'Source'
        l.type = 'WWW:LINK'
        l.url = 'http://' + sanitize(endpoint, '')
        self.assertEqual(vl.metadata().links(), [l])

    def testRenderer(self):
        """ Test that renderer is correctly acquired from provider """

        endpoint = self.basetestpath + '/renderer_fake_qgis_http_endpoint'
        with open(sanitize(endpoint, '?f=json'), 'wb') as f:
            f.write("""
        {"currentVersion":10.22,"id":1,"name":"QGIS Test","type":"Feature Layer","description":
        "QGIS Provider Test Layer","geometryType":"esriGeometryPoint","copyrightText":"not copyright","parentLayer":{"id":2,"name":"QGIS Tests"},"subLayers":[],
        "minScale":72225,"maxScale":0,
        "defaultVisibility":true,
        "extent":{"xmin":-71.123,"ymin":66.33,"xmax":-65.32,"ymax":78.3,
        "spatialReference":{"wkid":4326,"latestWkid":4326}},
        "hasAttachments":false,"htmlPopupType":"esriServerHTMLPopupTypeAsHTMLText",
        "displayField":"LABEL","typeIdField":null,
        "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null}],
        "relationships":[],"canModifyLayer":false,"canScaleSymbols":false,"hasLabels":false,
        "capabilities":"Map,Query,Data","maxRecordCount":1000,"supportsStatistics":true,
        "supportsAdvancedQueries":true,"supportedQueryFormats":"JSON, AMF",
        "drawingInfo":{"renderer": {
    "type": "uniqueValue",
    "field1": "COUNTRY",
    "uniqueValueInfos": [
      {
        "value": "US",
        "symbol": {
          "color": [
            253,
            127,
            111,
            255
          ],
          "size": 12.75,
          "angle": 0,
          "xoffset": 0,
          "yoffset": 0,
          "type": "esriSMS",
          "style": "esriSMSCircle",
          "outline": {
            "color": [
              26,
              26,
              26,
              255
            ],
            "width": 0.75,
            "type": "esriSLS",
            "style": "esriSLSSolid"
          }
        },
        "label": "US"
      },
      {
        "value": "Canada",
        "symbol": {
          "color": [
            126,
            176,
            213,
            255
          ],
          "size": 12.75,
          "angle": 0,
          "xoffset": 0,
          "yoffset": 0,
          "type": "esriSMS",
          "style": "esriSMSCircle",
          "outline": {
            "color": [
              26,
              26,
              26,
              255
            ],
            "width": 0.75,
            "type": "esriSLS",
            "style": "esriSLSSolid"
          }
        },
        "label": "Canada"
      }]}},
        "ownershipBasedAccessControlForFeatures":{"allowOthersToQuery":true},"useStandardizedQueries":true}""".encode(
                'UTF-8'))

        with open(sanitize(endpoint, '/query?f=json_where=OBJECTID=OBJECTID_returnIdsOnly=true'), 'wb') as f:
            f.write("""
        {
         "objectIdFieldName": "OBJECTID",
         "objectIds": [
          1
         ]
        }
        """.encode('UTF-8'))

        # Create test layer
        vl = QgsVectorLayer("url='http://" + endpoint + "' crs='epsg:4326'", 'test', 'arcgisfeatureserver')
        self.assertTrue(vl.isValid())
        self.assertIsNotNone(vl.dataProvider().createRenderer())
        self.assertIsInstance(vl.renderer(), QgsCategorizedSymbolRenderer)
        self.assertEqual(len(vl.renderer().categories()), 2)
        self.assertEqual(vl.renderer().categories()[0].value(), 'US')
        self.assertEqual(vl.renderer().categories()[1].value(), 'Canada')

    def testBboxRestriction(self):
        """
        Test limiting provider to features within a preset bounding box
        """
        endpoint = self.basetestpath + '/fake_qgis_http_endpoint'
        vl = QgsVectorLayer("url='http://" + endpoint + "' crs='epsg:4326' bbox='-70.000000,67.000000,-60.000000,80.000000'", 'test', 'arcgisfeatureserver')
        self.assertTrue(vl.isValid())
        self.assertEqual(vl.featureCount(), 2)
        self.assertEqual([f['pk'] for f in vl.getFeatures()], [2, 4])

    def testBadMultiPoints(self):
        """
        Test invalid server response where a layer's type is multipoint but single point geometries
        are returned. Thanks Jack. Thack.
        """
        endpoint = self.basetestpath + '/multipoint_fake_qgis_http_endpoint'
        with open(sanitize(endpoint, '?f=json'), 'wb') as f:
            f.write("""
        {"currentVersion":10.22,"id":1,"name":"QGIS Test","type":"Feature Layer","description":
        "QGIS Provider Test Layer.\n","geometryType":"esriGeometryMultipoint","copyrightText":"","parentLayer":{"id":0,"name":"QGIS Tests"},"subLayers":[],
        "minScale":72225,"maxScale":0,
        "defaultVisibility":true,
        "extent":{"xmin":-71.123,"ymin":66.33,"xmax":-65.32,"ymax":78.3,
        "spatialReference":{"wkid":4326,"latestWkid":4326}},
        "hasAttachments":false,"htmlPopupType":"esriServerHTMLPopupTypeAsHTMLText",
        "displayField":"LABEL","typeIdField":null,
        "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null}],
        "relationships":[],"canModifyLayer":false,"canScaleSymbols":false,"hasLabels":false,
        "capabilities":"Map,Query,Data","maxRecordCount":1000,"supportsStatistics":true,
        "supportsAdvancedQueries":true,"supportedQueryFormats":"JSON, AMF",
        "ownershipBasedAccessControlForFeatures":{"allowOthersToQuery":true},"useStandardizedQueries":true}""".encode(
                'UTF-8'))

        with open(sanitize(endpoint, '/query?f=json_where=OBJECTID=OBJECTID_returnIdsOnly=true'), 'wb') as f:
            f.write("""
        {
         "objectIdFieldName": "OBJECTID",
         "objectIds": [
          1,
          2,
          3
         ]
        }
        """.encode('UTF-8'))

        # Create test layer
        vl = QgsVectorLayer("url='http://" + endpoint + "' crs='epsg:4326'", 'test', 'arcgisfeatureserver')

        self.assertTrue(vl.isValid())
        with open(sanitize(endpoint,
                           '/query?f=json&objectIds=1,2,3&inSR=4326&outSR=4326&returnGeometry=true&outFields=OBJECTID&returnM=false&returnZ=false'), 'wb') as f:
            f.write("""
        {
         "displayFieldName": "name",
         "fieldAliases": {
          "name": "name"
         },
         "geometryType": "esriGeometryMultipoint",
         "spatialReference": {
          "wkid": 4326,
          "latestWkid": 4326
         },
         "fields":[{"name":"OBJECTID","type":"esriFieldTypeOID","alias":"OBJECTID","domain":null},
        {"name":"Shape","type":"esriFieldTypeGeometry","alias":"Shape","domain":null}],
         "features": [
          {
           "attributes": {
            "OBJECTID": 1
           },
           "geometry": {
            "x": -70,
            "y": 66
           }
          },
          {
           "attributes": {
            "OBJECTID": 2
           },
           "geometry": null
          },
          {
           "attributes": {
            "OBJECTID": 3
           },
           "geometry":
           {"points" :[[-68,70],
           [-22,21]]
           }
          }
         ]
        }""".encode('UTF-8'))

        features = [f for f in vl.getFeatures()]
        self.assertEqual(len(features), 3)
        self.assertEqual([f.geometry().asWkt() for f in features], ['MultiPoint ((-70 66))', '', 'MultiPoint ((-68 70),(-22 21))'])


if __name__ == '__main__':
    unittest.main()
