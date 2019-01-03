# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsTextRenderer.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Nyall Dawson'
__date__ = '2016-09'
__copyright__ = 'Copyright 2016, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis  # NOQA
import os

from qgis.core import (QgsTextBufferSettings,
                       QgsTextBackgroundSettings,
                       QgsTextShadowSettings,
                       QgsTextFormat,
                       QgsUnitTypes,
                       QgsMapUnitScale,
                       QgsVectorLayer,
                       QgsTextRenderer,
                       QgsMapSettings,
                       QgsReadWriteContext,
                       QgsRenderContext,
                       QgsRectangle,
                       QgsRenderChecker,
                       QgsBlurEffect)
from qgis.PyQt.QtGui import (QColor, QPainter, QFont, QImage, QBrush, QPen, QFontMetricsF)
from qgis.PyQt.QtCore import (Qt, QSizeF, QPointF, QRectF, QDir, QSize)
from qgis.PyQt.QtXml import QDomDocument
from PyQt5.QtSvg import QSvgGenerator
from qgis.testing import unittest, start_app
from utilities import getTestFont, svgSymbolsPath

start_app()


def createEmptyLayer():
    layer = QgsVectorLayer("Point", "addfeat", "memory")
    assert layer.isValid()
    return layer


class PyQgsTextRenderer(unittest.TestCase):

    def setUp(self):
        self.report = "<h1>Python QgsTextRenderer Tests</h1>\n"

    def tearDown(self):
        report_file_path = "%s/qgistest.html" % QDir.tempPath()
        with open(report_file_path, 'a') as report_file:
            report_file.write(self.report)

    def createBufferSettings(self):
        s = QgsTextBufferSettings()
        s.setEnabled(True)
        s.setSize(5)
        s.setSizeUnit(QgsUnitTypes.RenderPoints)
        s.setSizeMapUnitScale(QgsMapUnitScale(1, 2))
        s.setColor(QColor(255, 0, 0))
        s.setFillBufferInterior(True)
        s.setOpacity(0.5)
        s.setJoinStyle(Qt.RoundJoin)
        s.setBlendMode(QPainter.CompositionMode_DestinationAtop)
        return s

    def checkBufferSettings(self, s):
        """ test QgsTextBufferSettings """
        self.assertTrue(s.enabled())
        self.assertEqual(s.size(), 5)
        self.assertEqual(s.sizeUnit(), QgsUnitTypes.RenderPoints)
        self.assertEqual(s.sizeMapUnitScale(), QgsMapUnitScale(1, 2))
        self.assertEqual(s.color(), QColor(255, 0, 0))
        self.assertTrue(s.fillBufferInterior())
        self.assertEqual(s.opacity(), 0.5)
        self.assertEqual(s.joinStyle(), Qt.RoundJoin)
        self.assertEqual(s.blendMode(), QPainter.CompositionMode_DestinationAtop)

    def testBufferGettersSetters(self):
        s = self.createBufferSettings()
        self.checkBufferSettings(s)

        # some other checks
        s.setEnabled(False)
        self.assertFalse(s.enabled())
        s.setEnabled(True)
        self.assertTrue(s.enabled())
        s.setFillBufferInterior(False)
        self.assertFalse(s.fillBufferInterior())
        s.setFillBufferInterior(True)
        self.assertTrue(s.fillBufferInterior())

    def testBufferReadWriteXml(self):
        """test saving and restoring state of a buffer to xml"""
        doc = QDomDocument("testdoc")
        s = self.createBufferSettings()
        elem = s.writeXml(doc)
        parent = doc.createElement("settings")
        parent.appendChild(elem)
        t = QgsTextBufferSettings()
        t.readXml(parent)
        self.checkBufferSettings(t)

    def testBufferCopy(self):
        s = self.createBufferSettings()
        s2 = s
        self.checkBufferSettings(s2)
        s3 = QgsTextBufferSettings(s)
        self.checkBufferSettings(s3)

    def createBackgroundSettings(self):
        s = QgsTextBackgroundSettings()
        s.setEnabled(True)
        s.setType(QgsTextBackgroundSettings.ShapeEllipse)
        s.setSvgFile('svg.svg')
        s.setSizeType(QgsTextBackgroundSettings.SizePercent)
        s.setSize(QSizeF(1, 2))
        s.setSizeUnit(QgsUnitTypes.RenderPoints)
        s.setSizeMapUnitScale(QgsMapUnitScale(1, 2))
        s.setRotationType(QgsTextBackgroundSettings.RotationFixed)
        s.setRotation(45)
        s.setOffset(QPointF(3, 4))
        s.setOffsetUnit(QgsUnitTypes.RenderMapUnits)
        s.setOffsetMapUnitScale(QgsMapUnitScale(5, 6))
        s.setRadii(QSizeF(11, 12))
        s.setRadiiUnit(QgsUnitTypes.RenderPercentage)
        s.setRadiiMapUnitScale(QgsMapUnitScale(15, 16))
        s.setFillColor(QColor(255, 0, 0))
        s.setStrokeColor(QColor(0, 255, 0))
        s.setOpacity(0.5)
        s.setJoinStyle(Qt.RoundJoin)
        s.setBlendMode(QPainter.CompositionMode_DestinationAtop)
        s.setStrokeWidth(7)
        s.setStrokeWidthUnit(QgsUnitTypes.RenderPoints)
        s.setStrokeWidthMapUnitScale(QgsMapUnitScale(QgsMapUnitScale(25, 26)))
        return s

    def checkBackgroundSettings(self, s):
        """ test QgsTextBackgroundSettings """
        self.assertTrue(s.enabled())
        self.assertEqual(s.type(), QgsTextBackgroundSettings.ShapeEllipse)
        self.assertEqual(s.svgFile(), 'svg.svg')
        self.assertEqual(s.sizeType(), QgsTextBackgroundSettings.SizePercent)
        self.assertEqual(s.size(), QSizeF(1, 2))
        self.assertEqual(s.sizeUnit(), QgsUnitTypes.RenderPoints)
        self.assertEqual(s.sizeMapUnitScale(), QgsMapUnitScale(1, 2))
        self.assertEqual(s.rotationType(), QgsTextBackgroundSettings.RotationFixed)
        self.assertEqual(s.rotation(), 45)
        self.assertEqual(s.offset(), QPointF(3, 4))
        self.assertEqual(s.offsetUnit(), QgsUnitTypes.RenderMapUnits)
        self.assertEqual(s.offsetMapUnitScale(), QgsMapUnitScale(5, 6))
        self.assertEqual(s.radii(), QSizeF(11, 12))
        self.assertEqual(s.radiiUnit(), QgsUnitTypes.RenderPercentage)
        self.assertEqual(s.radiiMapUnitScale(), QgsMapUnitScale(15, 16))
        self.assertEqual(s.fillColor(), QColor(255, 0, 0))
        self.assertEqual(s.strokeColor(), QColor(0, 255, 0))
        self.assertEqual(s.opacity(), 0.5)
        self.assertEqual(s.joinStyle(), Qt.RoundJoin)
        self.assertEqual(s.blendMode(), QPainter.CompositionMode_DestinationAtop)
        self.assertEqual(s.strokeWidth(), 7)
        self.assertEqual(s.strokeWidthUnit(), QgsUnitTypes.RenderPoints)
        self.assertEqual(s.strokeWidthMapUnitScale(), QgsMapUnitScale(25, 26))

    def testBackgroundGettersSetters(self):
        s = self.createBackgroundSettings()
        self.checkBackgroundSettings(s)

        # some other checks
        s.setEnabled(False)
        self.assertFalse(s.enabled())
        s.setEnabled(True)
        self.assertTrue(s.enabled())

    def testBackgroundCopy(self):
        s = self.createBackgroundSettings()
        s2 = s
        self.checkBackgroundSettings(s2)
        s3 = QgsTextBackgroundSettings(s)
        self.checkBackgroundSettings(s3)

    def testBackgroundReadWriteXml(self):
        """test saving and restoring state of a background to xml"""
        doc = QDomDocument("testdoc")
        s = self.createBackgroundSettings()
        elem = s.writeXml(doc, QgsReadWriteContext())
        parent = doc.createElement("settings")
        parent.appendChild(elem)
        t = QgsTextBackgroundSettings()
        t.readXml(parent, QgsReadWriteContext())
        self.checkBackgroundSettings(t)

    def createShadowSettings(self):
        s = QgsTextShadowSettings()
        s.setEnabled(True)
        s.setShadowPlacement(QgsTextShadowSettings.ShadowBuffer)
        s.setOffsetAngle(45)
        s.setOffsetDistance(75)
        s.setOffsetUnit(QgsUnitTypes.RenderMapUnits)
        s.setOffsetMapUnitScale(QgsMapUnitScale(5, 6))
        s.setOffsetGlobal(True)
        s.setBlurRadius(11)
        s.setBlurRadiusUnit(QgsUnitTypes.RenderPercentage)
        s.setBlurRadiusMapUnitScale(QgsMapUnitScale(15, 16))
        s.setBlurAlphaOnly(True)
        s.setColor(QColor(255, 0, 0))
        s.setOpacity(0.5)
        s.setScale(123)
        s.setBlendMode(QPainter.CompositionMode_DestinationAtop)
        return s

    def checkShadowSettings(self, s):
        """ test QgsTextShadowSettings """
        self.assertTrue(s.enabled())
        self.assertEqual(s.shadowPlacement(), QgsTextShadowSettings.ShadowBuffer)
        self.assertEqual(s.offsetAngle(), 45)
        self.assertEqual(s.offsetDistance(), 75)
        self.assertEqual(s.offsetUnit(), QgsUnitTypes.RenderMapUnits)
        self.assertEqual(s.offsetMapUnitScale(), QgsMapUnitScale(5, 6))
        self.assertTrue(s.offsetGlobal())
        self.assertEqual(s.blurRadius(), 11)
        self.assertEqual(s.blurRadiusUnit(), QgsUnitTypes.RenderPercentage)
        self.assertEqual(s.blurRadiusMapUnitScale(), QgsMapUnitScale(15, 16))
        self.assertTrue(s.blurAlphaOnly())
        self.assertEqual(s.color(), QColor(255, 0, 0))
        self.assertEqual(s.opacity(), 0.5)
        self.assertEqual(s.scale(), 123)
        self.assertEqual(s.blendMode(), QPainter.CompositionMode_DestinationAtop)

    def testShadowGettersSetters(self):
        s = self.createShadowSettings()
        self.checkShadowSettings(s)

        # some other checks
        s.setEnabled(False)
        self.assertFalse(s.enabled())
        s.setEnabled(True)
        self.assertTrue(s.enabled())
        s.setOffsetGlobal(False)
        self.assertFalse(s.offsetGlobal())
        s.setOffsetGlobal(True)
        self.assertTrue(s.offsetGlobal())
        s.setBlurAlphaOnly(False)
        self.assertFalse(s.blurAlphaOnly())
        s.setBlurAlphaOnly(True)
        self.assertTrue(s.blurAlphaOnly())

    def testShadowCopy(self):
        s = self.createShadowSettings()
        s2 = s
        self.checkShadowSettings(s2)
        s3 = QgsTextShadowSettings(s)
        self.checkShadowSettings(s3)

    def testShadowReadWriteXml(self):
        """test saving and restoring state of a shadow to xml"""
        doc = QDomDocument("testdoc")
        s = self.createShadowSettings()
        elem = s.writeXml(doc)
        parent = doc.createElement("settings")
        parent.appendChild(elem)
        t = QgsTextShadowSettings()
        t.readXml(parent)
        self.checkShadowSettings(t)

    def createFormatSettings(self):
        s = QgsTextFormat()
        s.buffer().setEnabled(True)
        s.buffer().setSize(25)
        s.background().setEnabled(True)
        s.background().setSvgFile('test.svg')
        s.shadow().setEnabled(True)
        s.shadow().setOffsetAngle(223)
        s.setFont(getTestFont())
        s.setNamedStyle('Italic')
        s.setSize(5)
        s.setSizeUnit(QgsUnitTypes.RenderPoints)
        s.setSizeMapUnitScale(QgsMapUnitScale(1, 2))
        s.setColor(QColor(255, 0, 0))
        s.setOpacity(0.5)
        s.setBlendMode(QPainter.CompositionMode_DestinationAtop)
        s.setLineHeight(5)
        return s

    def checkTextFormat(self, s):
        """ test QgsTextFormat """
        self.assertTrue(s.buffer().enabled())
        self.assertEqual(s.buffer().size(), 25)
        self.assertTrue(s.background().enabled())
        self.assertEqual(s.background().svgFile(), 'test.svg')
        self.assertTrue(s.shadow().enabled())
        self.assertEqual(s.shadow().offsetAngle(), 223)
        self.assertEqual(s.font().family(), 'QGIS Vera Sans')
        self.assertEqual(s.namedStyle(), 'Italic')
        self.assertEqual(s.size(), 5)
        self.assertEqual(s.sizeUnit(), QgsUnitTypes.RenderPoints)
        self.assertEqual(s.sizeMapUnitScale(), QgsMapUnitScale(1, 2))
        self.assertEqual(s.color(), QColor(255, 0, 0))
        self.assertEqual(s.opacity(), 0.5)
        self.assertEqual(s.blendMode(), QPainter.CompositionMode_DestinationAtop)
        self.assertEqual(s.lineHeight(), 5)

    def testFormatGettersSetters(self):
        s = self.createFormatSettings()
        self.checkTextFormat(s)

    def testFormatCopy(self):
        s = self.createFormatSettings()
        s2 = s
        self.checkTextFormat(s2)
        s3 = QgsTextFormat(s)
        self.checkTextFormat(s3)

    def testFormatReadWriteXml(self):
        """test saving and restoring state of a shadow to xml"""
        doc = QDomDocument("testdoc")
        s = self.createFormatSettings()
        elem = s.writeXml(doc, QgsReadWriteContext())
        parent = doc.createElement("settings")
        parent.appendChild(elem)
        t = QgsTextFormat()
        t.readXml(parent, QgsReadWriteContext())
        self.checkTextFormat(t)

    def testFormatToFromMimeData(self):
        """Test converting format to and from mime data"""
        s = self.createFormatSettings()
        md = s.toMimeData()
        from_mime, ok = QgsTextFormat.fromMimeData(None)
        self.assertFalse(ok)
        from_mime, ok = QgsTextFormat.fromMimeData(md)
        self.assertTrue(ok)
        self.checkTextFormat(from_mime)

    def containsAdvancedEffects(self):
        t = QgsTextFormat()
        self.assertFalse(t.containsAdvancedEffects())
        t.setBlendMode(QPainter.CompositionMode_DestinationAtop)
        self.assertTrue(t.containsAdvancedEffects())

        t = QgsTextFormat()
        t.buffer().setBlendMode(QPainter.CompositionMode_DestinationAtop)
        self.assertFalse(t.containsAdvancedEffects())
        t.buffer().setEnabled(True)
        self.assertTrue(t.containsAdvancedEffects())
        t.buffer().setBlendMode(QPainter.CompositionMode_SourceOver)
        self.assertFalse(t.containsAdvancedEffects())

        t = QgsTextFormat()
        t.background().setBlendMode(QPainter.CompositionMode_DestinationAtop)
        self.assertFalse(t.containsAdvancedEffects())
        t.background().setEnabled(True)
        self.assertTrue(t.containsAdvancedEffects())
        t.background().setBlendMode(QPainter.CompositionMode_SourceOver)
        self.assertFalse(t.containsAdvancedEffects())

        t = QgsTextFormat()
        t.shadow().setBlendMode(QPainter.CompositionMode_DestinationAtop)
        self.assertFalse(t.containsAdvancedEffects())
        t.shadow().setEnabled(True)
        self.assertTrue(t.containsAdvancedEffects())
        t.shadow().setBlendMode(QPainter.CompositionMode_SourceOver)
        self.assertFalse(t.containsAdvancedEffects())

    def testFontFoundFromLayer(self):
        layer = createEmptyLayer()
        layer.setCustomProperty('labeling/fontFamily', 'asdasd')
        f = QgsTextFormat()
        f.readFromLayer(layer)
        self.assertFalse(f.fontFound())

        font = getTestFont()
        layer.setCustomProperty('labeling/fontFamily', font.family())
        f.readFromLayer(layer)
        self.assertTrue(f.fontFound())

    def testFontFoundFromXml(self):
        doc = QDomDocument("testdoc")
        f = QgsTextFormat()
        elem = f.writeXml(doc, QgsReadWriteContext())
        elem.setAttribute('fontFamily', 'asdfasdfsadf')
        parent = doc.createElement("parent")
        parent.appendChild(elem)

        f.readXml(parent, QgsReadWriteContext())
        self.assertFalse(f.fontFound())

        font = getTestFont()
        elem.setAttribute('fontFamily', font.family())
        f.readXml(parent, QgsReadWriteContext())
        self.assertTrue(f.fontFound())

    def testFromQFont(self):
        qfont = getTestFont()
        qfont.setPointSizeF(16.5)
        qfont.setLetterSpacing(QFont.AbsoluteSpacing, 3)

        format = QgsTextFormat.fromQFont(qfont)
        self.assertEqual(format.font().family(), qfont.family())
        self.assertEqual(format.font().letterSpacing(), 3.0)
        self.assertEqual(format.size(), 16.5)
        self.assertEqual(format.sizeUnit(), QgsUnitTypes.RenderPoints)

        qfont.setPixelSize(12)
        format = QgsTextFormat.fromQFont(qfont)
        self.assertEqual(format.size(), 12.0)
        self.assertEqual(format.sizeUnit(), QgsUnitTypes.RenderPixels)

    def testToQFont(self):
        s = QgsTextFormat()
        f = getTestFont()
        f.setLetterSpacing(QFont.AbsoluteSpacing, 3)
        s.setFont(f)
        s.setNamedStyle('Italic')
        s.setSize(5.5)
        s.setSizeUnit(QgsUnitTypes.RenderPoints)

        qfont = s.toQFont()
        self.assertEqual(qfont.family(), f.family())
        self.assertEqual(qfont.pointSizeF(), 5.5)
        self.assertEqual(qfont.letterSpacing(), 3.0)

        s.setSize(5)
        s.setSizeUnit(QgsUnitTypes.RenderPixels)
        qfont = s.toQFont()
        self.assertEqual(qfont.pixelSize(), 5)

        s.setSize(5)
        s.setSizeUnit(QgsUnitTypes.RenderMillimeters)
        qfont = s.toQFont()
        self.assertAlmostEqual(qfont.pointSizeF(), 14.17, 2)

        s.setSizeUnit(QgsUnitTypes.RenderInches)
        qfont = s.toQFont()
        self.assertAlmostEqual(qfont.pointSizeF(), 360.0, 2)

    def testFontMetrics(self):
        """
        Test calculating font metrics from scaled text formats
        """
        s = QgsTextFormat()
        f = getTestFont()
        s.setFont(f)
        s.setSize(12)
        s.setSizeUnit(QgsUnitTypes.RenderPoints)

        string = 'xxxxxxxxxxxxxxxxxxxxxx'

        image = QImage(400, 400, QImage.Format_RGB32)
        painter = QPainter(image)
        context = QgsRenderContext.fromQPainter(painter)
        context.setScaleFactor(1)
        metrics = QgsTextRenderer.fontMetrics(context, s)
        context.setScaleFactor(2)
        metrics2 = QgsTextRenderer.fontMetrics(context, s)
        painter.end()

        self.assertAlmostEqual(metrics.width(string), 51.9, 1)
        self.assertAlmostEqual(metrics2.width(string), 104.15, 1)

    def imageCheck(self, name, reference_image, image):
        self.report += "<h2>Render {}</h2>\n".format(name)
        temp_dir = QDir.tempPath() + '/'
        file_name = temp_dir + name + ".png"
        image.save(file_name, "PNG")
        checker = QgsRenderChecker()
        checker.setControlPathPrefix("text_renderer")
        checker.setControlName(reference_image)
        checker.setRenderedImage(file_name)
        checker.setColorTolerance(2)
        result = checker.compareImages(name, 20)
        self.report += checker.report()
        print(checker.report())
        return result

    def checkRender(self, format, name, part=None, angle=0, alignment=QgsTextRenderer.AlignLeft,
                    text=['test'],
                    rect=QRectF(100, 100, 50, 250)):

        image = QImage(400, 400, QImage.Format_RGB32)

        painter = QPainter()
        ms = QgsMapSettings()
        ms.setExtent(QgsRectangle(0, 0, 50, 50))
        ms.setOutputSize(image.size())
        context = QgsRenderContext.fromMapSettings(ms)
        context.setPainter(painter)
        context.setScaleFactor(96 / 25.4)  # 96 DPI

        painter.begin(image)
        painter.setRenderHint(QPainter.Antialiasing)
        image.fill(QColor(152, 219, 249))

        painter.setBrush(QBrush(QColor(182, 239, 255)))
        painter.setPen(Qt.NoPen)
        # to highlight rect on image
        #painter.drawRect(rect)

        if part is not None:
            QgsTextRenderer.drawPart(rect,
                                     angle,
                                     alignment,
                                     text,
                                     context,
                                     format,
                                     part)
        else:
            QgsTextRenderer.drawText(rect,
                                     angle,
                                     alignment,
                                     text,
                                     context,
                                     format)

        painter.setFont(format.scaledFont(context))
        painter.setPen(QPen(QColor(255, 0, 255, 200)))
        # For comparison with QPainter's methods:
        # if alignment == QgsTextRenderer.AlignCenter:
        #     align = Qt.AlignHCenter
        # elif alignment == QgsTextRenderer.AlignRight:
        #     align = Qt.AlignRight
        # else:
        #     align = Qt.AlignLeft
        # painter.drawText(rect, align, '\n'.join(text))

        painter.end()
        return self.imageCheck(name, name, image)

    def checkRenderPoint(self, format, name, part=None, angle=0, alignment=QgsTextRenderer.AlignLeft,
                         text=['test'],
                         point=QPointF(100, 200)):
        image = QImage(400, 400, QImage.Format_RGB32)

        painter = QPainter()
        ms = QgsMapSettings()
        ms.setExtent(QgsRectangle(0, 0, 50, 50))
        ms.setOutputSize(image.size())
        context = QgsRenderContext.fromMapSettings(ms)
        context.setPainter(painter)
        context.setScaleFactor(96 / 25.4)  # 96 DPI

        painter.begin(image)
        painter.setRenderHint(QPainter.Antialiasing)
        image.fill(QColor(152, 219, 249))

        painter.setBrush(QBrush(QColor(182, 239, 255)))
        painter.setPen(Qt.NoPen)
        # to highlight point on image
        #painter.drawRect(QRectF(point.x() - 5, point.y() - 5, 10, 10))

        if part is not None:
            QgsTextRenderer.drawPart(point,
                                     angle,
                                     alignment,
                                     text,
                                     context,
                                     format,
                                     part)
        else:
            QgsTextRenderer.drawText(point,
                                     angle,
                                     alignment,
                                     text,
                                     context,
                                     format)

        painter.setFont(format.scaledFont(context))
        painter.setPen(QPen(QColor(255, 0, 255, 200)))
        # For comparison with QPainter's methods:
        #painter.drawText(point, '\n'.join(text))

        painter.end()
        return self.imageCheck(name, name, image)

    def testDrawBackgroundDisabled(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(False)
        assert self.checkRender(format, 'background_disabled', QgsTextRenderer.Background)

    def testDrawBackgroundRectangleFixedSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_rect_mapunits', QgsTextRenderer.Background)

    def testDrawBackgroundRectangleMultilineFixedSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_rect_multiline_mapunits', QgsTextRenderer.Background,
                                text=['test', 'multi', 'line'])

    def testDrawBackgroundPointMultilineFixedSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRenderPoint(format, 'background_point_multiline_mapunits', QgsTextRenderer.Background,
                                     text=['test', 'multi', 'line'])

    def testDrawBackgroundRectangleMultilineBufferMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(4, 2))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_rect_multiline_buffer_mapunits', QgsTextRenderer.Background,
                                text=['test', 'multi', 'line'])

    def testDrawBackgroundPointMultilineBufferMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(4, 2))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRenderPoint(format, 'background_point_multiline_buffer_mapunits', QgsTextRenderer.Background,
                                     text=['test', 'multi', 'line'])

    def testDrawBackgroundPointFixedSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRenderPoint(format, 'background_point_mapunits', QgsTextRenderer.Background,
                                     text=['Testy'])

    def testDrawBackgroundRectangleCenterAlignFixedSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_rect_center_mapunits', QgsTextRenderer.Background,
                                alignment=QgsTextRenderer.AlignCenter)

    def testDrawBackgroundPointCenterAlignFixedSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRenderPoint(format, 'background_point_center_mapunits', QgsTextRenderer.Background,
                                     alignment=QgsTextRenderer.AlignCenter)

    def testDrawBackgroundRectangleRightAlignFixedSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_rect_right_mapunits', QgsTextRenderer.Background,
                                alignment=QgsTextRenderer.AlignRight)

    def testDrawBackgroundPointRightAlignFixedSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRenderPoint(format, 'background_point_right_mapunits', QgsTextRenderer.Background,
                                     alignment=QgsTextRenderer.AlignRight)

    def testDrawBackgroundRectangleFixedSizeMM(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'background_rect_mm', QgsTextRenderer.Background)

    def testDrawBackgroundRectangleFixedSizePixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(60, 80))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'background_rect_pixels', QgsTextRenderer.Background)

    def testDrawBackgroundRectBufferPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 50))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'background_rect_buffer_pixels', QgsTextRenderer.Background,
                                rect=QRectF(100, 100, 100, 100))

    def testDrawBackgroundRectRightAlignBufferPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 50))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'background_rect_right_buffer_pixels', QgsTextRenderer.Background,
                                alignment=QgsTextRenderer.AlignRight,
                                rect=QRectF(100, 100, 100, 100))

    def testDrawBackgroundRectCenterAlignBufferPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 50))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'background_rect_center_buffer_pixels', QgsTextRenderer.Background,
                                alignment=QgsTextRenderer.AlignCenter,
                                rect=QRectF(100, 100, 100, 100))

    def testDrawBackgroundPointBufferPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 50))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRenderPoint(format, 'background_point_buffer_pixels', QgsTextRenderer.Background,
                                     point=QPointF(100, 100))

    def testDrawBackgroundPointRightAlignBufferPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 50))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRenderPoint(format, 'background_point_right_buffer_pixels', QgsTextRenderer.Background,
                                     alignment=QgsTextRenderer.AlignRight,
                                     point=QPointF(100, 100))

    def testDrawBackgroundPointCenterAlignBufferPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 50))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRenderPoint(format, 'background_point_center_buffer_pixels', QgsTextRenderer.Background,
                                     alignment=QgsTextRenderer.AlignCenter,
                                     point=QPointF(100, 100))

    def testDrawBackgroundRectBufferMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(4, 6))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_rect_buffer_mapunits', QgsTextRenderer.Background,
                                rect=QRectF(100, 100, 100, 100))

    def testDrawBackgroundRectBufferMM(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(10, 16))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'background_rect_buffer_mm', QgsTextRenderer.Background,
                                rect=QRectF(100, 100, 100, 100))

    def testDrawBackgroundEllipse(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeEllipse)
        format.background().setSize(QSizeF(60, 80))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'background_ellipse_pixels', QgsTextRenderer.Background)

    def testDrawBackgroundSvgFixedPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeSVG)
        format.background().setSize(QSizeF(60, 80))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'background_svg_fixed_pixels', QgsTextRenderer.Background)

    def testDrawBackgroundSvgFixedMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeSVG)
        format.background().setSize(QSizeF(20, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_svg_fixed_mapunits', QgsTextRenderer.Background)

    def testDrawBackgroundSvgFixedMM(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeSVG)
        format.background().setSize(QSizeF(30, 30))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'background_svg_fixed_mm', QgsTextRenderer.Background)

    def testDrawBackgroundRotationSynced(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.background().setRotation(45)  # should be ignored
        format.background().setRotationType(QgsTextBackgroundSettings.RotationSync)
        assert self.checkRender(format, 'background_rotation_sync', QgsTextRenderer.Background, angle=20)

    def testDrawBackgroundSvgBufferPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeSVG)
        format.background().setSize(QSizeF(30, 30))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'background_svg_buffer_pixels', QgsTextRenderer.Background,
                                rect=QRectF(100, 100, 100, 100))

    def testDrawBackgroundSvgBufferMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeSVG)
        format.background().setSize(QSizeF(4, 4))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_svg_buffer_mapunits', QgsTextRenderer.Background,
                                rect=QRectF(100, 100, 100, 100))

    def testDrawBackgroundSvgBufferMM(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        svg = os.path.join(
            svgSymbolsPath(), 'backgrounds', 'background_square.svg')
        format.background().setSvgFile(svg)
        format.background().setType(QgsTextBackgroundSettings.ShapeSVG)
        format.background().setSize(QSizeF(10, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeBuffer)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'background_svg_buffer_mm', QgsTextRenderer.Background,
                                rect=QRectF(100, 100, 100, 100))

    def testDrawBackgroundRotationFixed(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.background().setRotation(45)
        format.background().setRotationType(QgsTextBackgroundSettings.RotationFixed)
        assert self.checkRender(format, 'background_rotation_fixed', QgsTextRenderer.Background, angle=20)

    def testDrawRotationOffset(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.background().setRotation(45)
        format.background().setRotationType(QgsTextBackgroundSettings.RotationOffset)
        assert self.checkRender(format, 'background_rotation_offset', QgsTextRenderer.Background, angle=20)

    def testDrawBackgroundOffsetMM(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.background().setOffset(QPointF(30, 20))
        format.background().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'background_offset_mm', QgsTextRenderer.Background)

    def testDrawBackgroundOffsetMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.background().setOffset(QPointF(10, 5))
        format.background().setOffsetUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_offset_mapunits', QgsTextRenderer.Background)

    def testDrawBackgroundRadiiMM(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.background().setRadii(QSizeF(6, 4))
        format.background().setRadiiUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'background_radii_mm', QgsTextRenderer.Background)

    def testDrawBackgroundRadiiMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.background().setRadii(QSizeF(3, 2))
        format.background().setRadiiUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'background_radii_mapunits', QgsTextRenderer.Background)

    def testDrawBackgroundOpacity(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setOpacity(0.6)
        assert self.checkRender(format, 'background_opacity', QgsTextRenderer.Background)

    def testDrawBackgroundFillColor(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setFillColor(QColor(50, 100, 50))
        assert self.checkRender(format, 'background_fillcolor', QgsTextRenderer.Background)

    def testDrawBackgroundStroke(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setStrokeColor(QColor(50, 100, 50))
        format.background().setStrokeWidth(3)
        format.background().setStrokeWidthUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'background_outline', QgsTextRenderer.Background)

    def testDrawBackgroundEffect(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(30, 20))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setPaintEffect(QgsBlurEffect.create({'blur_level': '10', 'enabled': '1'}))
        assert self.checkRender(format, 'background_effect', QgsTextRenderer.Background, text=['test'])

    def testDrawText(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRender(format, 'text_bold', QgsTextRenderer.Text, text=['test'])

    def testDrawTextPoint(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRenderPoint(format, 'text_point_bold', QgsTextRenderer.Text, text=['test'])

    def testDrawTextNamedStyle(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        # need to call getTestFont to make sure font style is installed and ready to go
        temp_font = getTestFont('Bold Oblique')  # NOQA
        format.setFont(getTestFont())
        format.setNamedStyle('Bold Oblique')
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRender(format, 'text_named_style', QgsTextRenderer.Text, text=['test'])

    def testDrawTextColor(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(0, 255, 0))
        assert self.checkRender(format, 'text_color', QgsTextRenderer.Text, text=['test'])

    def testDrawTextOpacity(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setOpacity(0.7)
        assert self.checkRender(format, 'text_opacity', QgsTextRenderer.Text, text=['test'])

    def testDrawTextBlendMode(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(100, 100, 100))
        format.setBlendMode(QPainter.CompositionMode_Difference)
        assert self.checkRender(format, 'text_blend_mode', QgsTextRenderer.Text, text=['test'])

    def testDrawTextAngle(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRender(format, 'text_angled', QgsTextRenderer.Text, angle=90 / 180 * 3.141, text=['test'])

    def testDrawTextMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(5)
        format.setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'text_mapunits', QgsTextRenderer.Text, text=['test'])

    def testDrawTextPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(50)
        format.setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'text_pixels', QgsTextRenderer.Text, text=['test'])

    def testDrawMultiLineText(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRender(format, 'text_multiline', QgsTextRenderer.Text, text=['test', 'multi', 'line'])

    def testDrawMultiLineTextPoint(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRenderPoint(format, 'text_point_multiline', QgsTextRenderer.Text, text=['test', 'multi', 'line'])

    def testDrawLineHeightText(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setLineHeight(1.5)
        assert self.checkRender(format, 'text_line_height', QgsTextRenderer.Text, text=['test', 'multi', 'line'])

    def testDrawBufferSizeMM(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(True)
        format.buffer().setSize(2)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'text_buffer_mm', QgsTextRenderer.Buffer, text=['test'])

    def testDrawBufferDisabled(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(False)
        assert self.checkRender(format, 'text_disabled_buffer', QgsTextRenderer.Buffer, text=['test'])

    def testDrawBufferSizeMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(True)
        format.buffer().setSize(2)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'text_buffer_mapunits', QgsTextRenderer.Buffer, text=['test'])

    def testDrawBufferSizePixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(True)
        format.buffer().setSize(10)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'text_buffer_pixels', QgsTextRenderer.Buffer, text=['test'])

    def testDrawBufferColor(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(True)
        format.buffer().setSize(2)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.buffer().setColor(QColor(0, 255, 0))
        assert self.checkRender(format, 'text_buffer_color', QgsTextRenderer.Buffer, text=['test'])

    def testDrawBufferOpacity(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(True)
        format.buffer().setSize(2)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.buffer().setOpacity(0.5)
        assert self.checkRender(format, 'text_buffer_opacity', QgsTextRenderer.Buffer, text=['test'])

    def testDrawBufferFillInterior(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(True)
        format.buffer().setSize(2)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.buffer().setFillBufferInterior(True)
        assert self.checkRender(format, 'text_buffer_interior', QgsTextRenderer.Buffer, text=['test'])

    def testDrawBufferEffect(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(True)
        format.buffer().setSize(2)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        format.buffer().setPaintEffect(QgsBlurEffect.create({'blur_level': '10', 'enabled': '1'}))
        assert self.checkRender(format, 'text_buffer_effect', QgsTextRenderer.Buffer, text=['test'])

    def testDrawShadow(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'shadow_enabled', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowOffsetAngle(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetAngle(0)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'shadow_offset_angle', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowOffsetMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(10)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'shadow_offset_mapunits', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowOffsetPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(10)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'shadow_offset_pixels', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowBlurRadiusMM(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setOpacity(1.0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setBlurRadius(1)
        format.shadow().setBlurRadiusUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'shadow_radius_mm', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowBlurRadiusMapUnits(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setOpacity(1.0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setBlurRadius(3)
        format.shadow().setBlurRadiusUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'shadow_radius_mapunits', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowBlurRadiusPixels(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setOpacity(1.0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setBlurRadius(3)
        format.shadow().setBlurRadiusUnit(QgsUnitTypes.RenderPixels)
        assert self.checkRender(format, 'shadow_radius_pixels', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowOpacity(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setOpacity(0.5)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'shadow_opacity', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowColor(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setColor(QColor(255, 255, 0))
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'shadow_color', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowScale(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setScale(50)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'shadow_scale_50', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowScaleUp(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.shadow().setScale(150)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'shadow_scale_150', QgsTextRenderer.Text, text=['test'])

    def testDrawShadowBackgroundPlacement(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowShape)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'shadow_placement_background', QgsTextRenderer.Background, text=['test'])

    def testDrawShadowBufferPlacement(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.setColor(QColor(255, 255, 255))
        format.shadow().setEnabled(True)
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowBuffer)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.buffer().setEnabled(True)
        format.buffer().setSize(4)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'shadow_placement_buffer', QgsTextRenderer.Buffer, text=['test'])

    def testDrawTextWithBuffer(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.buffer().setEnabled(True)
        format.buffer().setSize(4)
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'text_with_buffer', text=['test'], rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithBackground(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'text_with_background', text=['test'], rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithBufferAndBackground(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        format.buffer().setEnabled(True)
        format.buffer().setSize(4)
        format.buffer().setColor(QColor(100, 255, 100))
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'text_with_buffer_and_background', text=['test'], rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithShadowAndBuffer(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.shadow().setEnabled(True)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setColor(QColor(255, 100, 100))
        format.buffer().setEnabled(True)
        format.buffer().setSize(4)
        format.buffer().setColor(QColor(100, 255, 100))
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'text_with_shadow_and_buffer', text=['test'], rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithShadowBelowTextAndBuffer(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.shadow().setEnabled(True)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setColor(QColor(255, 100, 100))
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.buffer().setEnabled(True)
        format.buffer().setSize(4)
        format.buffer().setColor(QColor(100, 255, 100))
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'text_with_shadow_below_text_and_buffer', text=['test'], rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithBackgroundAndShadow(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.shadow().setEnabled(True)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setColor(QColor(255, 100, 100))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'text_with_shadow_and_background', text=['test'], rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithShadowBelowTextAndBackground(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.shadow().setEnabled(True)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setColor(QColor(255, 100, 100))
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        assert self.checkRender(format, 'text_with_shadow_below_text_and_background', text=['test'], rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithBackgroundBufferAndShadow(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.shadow().setEnabled(True)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setColor(QColor(255, 100, 100))
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        format.buffer().setEnabled(True)
        format.buffer().setSize(4)
        format.buffer().setColor(QColor(100, 255, 100))
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'text_with_shadow_buffer_and_background', text=['test'], rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithBackgroundBufferAndShadowBelowText(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.shadow().setEnabled(True)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setColor(QColor(255, 100, 100))
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowText)
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        format.buffer().setEnabled(True)
        format.buffer().setSize(4)
        format.buffer().setColor(QColor(100, 255, 100))
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'text_with_shadow_below_text_buffer_and_background', text=['test'],
                                rect=QRectF(100, 100, 200, 100))

    def testDrawTextWithBackgroundBufferAndShadowBelowBuffer(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(60)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        format.shadow().setEnabled(True)
        format.shadow().setOpacity(1.0)
        format.shadow().setBlurRadius(0)
        format.shadow().setOffsetDistance(5)
        format.shadow().setOffsetUnit(QgsUnitTypes.RenderMillimeters)
        format.shadow().setColor(QColor(255, 100, 100))
        format.shadow().setShadowPlacement(QgsTextShadowSettings.ShadowBuffer)
        format.background().setEnabled(True)
        format.background().setType(QgsTextBackgroundSettings.ShapeRectangle)
        format.background().setSize(QSizeF(20, 10))
        format.background().setSizeType(QgsTextBackgroundSettings.SizeFixed)
        format.background().setSizeUnit(QgsUnitTypes.RenderMapUnits)
        format.buffer().setEnabled(True)
        format.buffer().setSize(4)
        format.buffer().setColor(QColor(100, 255, 100))
        format.buffer().setSizeUnit(QgsUnitTypes.RenderMillimeters)
        assert self.checkRender(format, 'text_with_shadow_below_buffer_and_background', text=['test'],
                                rect=QRectF(100, 100, 200, 100))

    def testDrawTextRectMultilineRightAlign(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRender(format, 'text_rect_multiline_right_aligned', text=['test', 'right', 'aligned'],
                                alignment=QgsTextRenderer.AlignRight, rect=QRectF(100, 100, 200, 100))

    def testDrawTextRectRightAlign(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRender(format, 'text_rect_right_aligned', text=['test'],
                                alignment=QgsTextRenderer.AlignRight, rect=QRectF(100, 100, 200, 100))

    def testDrawTextRectMultilineCenterAlign(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRender(format, 'text_rect_multiline_center_aligned', text=['test', 'c', 'aligned'],
                                alignment=QgsTextRenderer.AlignCenter, rect=QRectF(100, 100, 200, 100))

    def testDrawTextRectCenterAlign(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRender(format, 'text_rect_center_aligned', text=['test'],
                                alignment=QgsTextRenderer.AlignCenter, rect=QRectF(100, 100, 200, 100))

    def testDrawTextPointMultilineRightAlign(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRenderPoint(format, 'text_point_right_multiline_aligned', text=['test', 'right', 'aligned'],
                                     alignment=QgsTextRenderer.AlignRight, point=QPointF(300, 200))

    def testDrawTextPointMultilineCenterAlign(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRenderPoint(format, 'text_point_center_multiline_aligned', text=['test', 'center', 'aligned'],
                                     alignment=QgsTextRenderer.AlignCenter, point=QPointF(200, 200))

    def testDrawTextPointRightAlign(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRenderPoint(format, 'text_point_right_aligned', text=['test'],
                                     alignment=QgsTextRenderer.AlignRight, point=QPointF(300, 200))

    def testDrawTextPointCenterAlign(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)
        assert self.checkRenderPoint(format, 'text_point_center_aligned', text=['test'],
                                     alignment=QgsTextRenderer.AlignCenter, point=QPointF(200, 200))

    def testTextRenderFormat(self):
        format = QgsTextFormat()
        format.setFont(getTestFont('bold'))
        format.setSize(30)
        format.setSizeUnit(QgsUnitTypes.RenderPoints)

        filename = '{}/test_render_text.svg'.format(QDir.tempPath())
        svg = QSvgGenerator()
        svg.setFileName(filename)
        svg.setSize(QSize(400, 400))
        svg.setResolution(600)

        ms = QgsMapSettings()
        ms.setExtent(QgsRectangle(0, 0, 50, 50))
        ms.setOutputSize(QSize(400, 400))
        context = QgsRenderContext.fromMapSettings(ms)

        # test with ALWAYS TEXT mode
        context.setTextRenderFormat(QgsRenderContext.TextFormatAlwaysText)
        painter = QPainter()
        context.setPainter(painter)

        context.setScaleFactor(96 / 25.4)  # 96 DPI

        painter.begin(svg)

        painter.setBrush(QBrush(QColor(182, 239, 255)))
        painter.setPen(Qt.NoPen)

        QgsTextRenderer.drawText(QPointF(0, 30),
                                 0,
                                 QgsTextRenderer.AlignLeft,
                                 ['my test text'],
                                 context,
                                 format)

        painter.end()

        # expect svg to contain a text object with the label
        with open(filename, 'r') as f:
            lines = ''.join(f.readlines())
        self.assertIn('<text', lines)
        self.assertIn('>my test text<', lines)

        os.unlink(filename)

        # test with ALWAYS CURVES mode
        context = QgsRenderContext.fromMapSettings(ms)
        context.setTextRenderFormat(QgsRenderContext.TextFormatAlwaysOutlines)
        painter = QPainter()
        context.setPainter(painter)

        context.setScaleFactor(96 / 25.4)  # 96 DPI

        svg = QSvgGenerator()
        svg.setFileName(filename)
        svg.setSize(QSize(400, 400))
        svg.setResolution(600)
        painter.begin(svg)

        painter.setBrush(QBrush(QColor(182, 239, 255)))
        painter.setPen(Qt.NoPen)

        QgsTextRenderer.drawText(QPointF(0, 30),
                                 0,
                                 QgsTextRenderer.AlignLeft,
                                 ['my test text'],
                                 context,
                                 format)

        painter.end()

        # expect svg to contain a text object with the label
        with open(filename, 'r') as f:
            lines = ''.join(f.readlines())
        self.assertNotIn('<text', lines)
        self.assertNotIn('>my test text<', lines)


if __name__ == '__main__':
    unittest.main()
