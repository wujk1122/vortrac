/*
 *
 * CappiDisplay.h
 *
 * Created by Michael Bell on 8/26/06
 *  Copyright 2005 University Corporation for Atmospheric Research.
 *  All rights reserved.
 *
 *
 */

#ifndef CAPPIDISPLAY_H
#define CAPPIDISPLAY_H

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QWidget>
#include <QBrush>
#include <QMutex>
#include "DataObjects/GriddedData.h"

class CappiDisplay : public QWidget
{
    Q_OBJECT

public:
    CappiDisplay(QWidget *parent = 0);

    bool openImage(const QString &fileName);
    bool saveImage(const QString &fileName, const char *fileFormat);

    float getMaxRec() { return maxRec; }
    float getMaxApp() { return maxApp; }

public slots:
    void clearImage();
    void constructImage(const GriddedData& cappi);
    void setGBVTDResults(float x, float y,float rmwEstimate, float sMin, float sMax, float vMax,
                         float userlat, float userlon,float lat, float lon);

signals:
    void hasImage(bool imageAvailable);

protected:
    void mousePressEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    void resizeImage(QImage *image, const QSize &newSize);
    QString cappiLabel;
    QImage image;
    QMutex imageHolder;
    int iDim, jDim;
    QImage legendImage;
    QPoint lastPoint;
    int PaintEngineMode;
    QColor backColor;
    float maxApp;
    float maxRec;
    float maxVel;
    float minVel;
    float velIncr;
    bool hasGBVTDInfo;
    float xPercent, yPercent;
    float radiusMaximumWind;
    float simplexMin, simplexMax;
    float vortexMax;
    float minVelXpercent, minVelYpercent;
    float maxVelXpercent, maxVelYpercent;


};

#endif
