/*
 *  AnalysisThread.h
 *  VORTRAC
 *
 *  Created by Michael Bell on 6/17/05.
 *  Copyright 2005 University Corporation for Atmospheric Research.
 *  All rights reserved.
 *
 */

#ifndef ANALYSISTHREAD_H
#define ANALYSISTHREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <QList>
#include "Radar/RadarData.h"
#include "IO/Message.h"
#include "Config/Configuration.h"
#include "DataObjects/VortexList.h"
#include "DataObjects/GriddedFactory.h"
#include "SimplexThread.h"

class AnalysisThread : public QThread
{
  Q_OBJECT
    
 public:
     AnalysisThread(QObject *parent = 0);
     ~AnalysisThread();
     void analyze(RadarData *dataVolume, Configuration *configPtr);
     void setConfig(Configuration *configPtr);
     void setVortexList(VortexList *archivePtr);

 public slots:
     void catchLog(const Message& message);
	 void foundCenter();
 
 protected:
     void run();
 
 signals:
     void doneProcessing();
     void log(const Message& message);
     void newVCP(const int);
 
 private:
     QMutex mutex;
     QWaitCondition waitForData;
	 QWaitCondition waitForCenter;
     bool abort;
     Configuration *configData;
     RadarData *radarVolume;
	 GriddedFactory gridFactory;
	 SimplexThread simplexThread;
     VortexList *vortexList;
     void archiveAnalysis();
     float vortexLat, vortexLon;

};

#endif
