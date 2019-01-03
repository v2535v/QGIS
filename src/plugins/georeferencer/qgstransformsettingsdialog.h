/***************************************************************************
     qgstransformsettingsdialog.h
     --------------------------------------
    Date                 : 14-Feb-2010
    Copyright            : (C) 2010 by Jack R, Maxim Dubinin (GIS-Lab)
    Email                : sim@gis-lab.info
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSTRANSFORMSETTINGSDIALOG_H
#define QGSTRANSFORMSETTINGSDIALOG_H

#include <QDialog>

#include "qgsgeorefplugingui.h"

#include "ui_qgstransformsettingsdialogbase.h"

class QgsTransformSettingsDialog : public QDialog, private Ui::QgsTransformSettingsDialog
{
    Q_OBJECT

  public:
    QgsTransformSettingsDialog( const QString &raster, const QString &output,
                                int countGCPpoints, QWidget *parent = nullptr );

    ~QgsTransformSettingsDialog() override;
    void getTransformSettings( QgsGeorefTransform::TransformParametrisation &tp,
                               QgsImageWarper::ResamplingMethod &rm, QString &comprMethod,
                               QString &raster, QgsCoordinateReferenceSystem &proj, QString &pdfMapFile, QString &pdfReportFile, QString &gcpPoints, bool &zt, bool &loadInQgis,
                               double &resX, double &resY );
    static void resetSettings();

  protected:
    void changeEvent( QEvent *e ) override;
    void accept() override;

  private slots:
    void cmbTransformType_currentIndexChanged( const QString &text );
    void mWorldFileCheckBox_stateChanged( int state );
    QIcon getThemeIcon( const QString &name );

  private:
    bool checkGCPpoints( int count, int &minGCPpoints );
    QString generateModifiedRasterFileName( const QString &raster );

    QString mSourceRasterFile;

    int mCountGCPpoints;

    QStringList mListCompression;
};

#endif // QGSTRANSFORMSETTINGSDIALOG_H
