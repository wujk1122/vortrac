/*
 *  VortexThread.h
 *  vortrac
 *
 *  Created by Michael Bell on 2/6/06.
 *  Copyright 2006 University Corporation for Atmospheric Research.
 *  All rights reserved.
 *
 */

#ifndef VORTEXTHREAD_H
#define VORTEXTHREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>

#include "IO/Message.h"
#include "Config/Configuration.h"
#include "DataObjects/GriddedData.h"
#include "VTD/GBVTD.h"
#include "DataObjects/Coefficient.h"
#include "DataObjects/VortexList.h"
#include "DataObjects/VortexData.h"
#include "Pressure/PressureList.h"
#include "Radar/RadarData.h"
//#include "HVVP/Hvvp.h"

class VortexThread : public QThread
{
  Q_OBJECT
    
 public:
  VortexThread(QObject *parent = 0);
  ~VortexThread();
  void getWinds(Configuration *wholeConfig, GriddedData *dataPtr, RadarData *radarPtr, VortexData *vortexPtr, PressureList *pressurePtr);
  void writeAsi(float*** propGrid,float& kdim);
  void writeANAsi(float** DataField,float& IT,int& kdim, int& xdim, int& ydim);
  //  void setHVVPangle(float HVVPThetaM);
  //  void setvmcos(float vmcos);
  // bool writeAsi(const QString& fileName);

 public slots:
     void catchLog(const Message& message);
   
 protected:
     void run();
 
 signals:
     void windsFound();
     void pressureFound();
     void log(const Message& message);
 
 private:
     float ThetaM;
     QMutex mutex;
     QWaitCondition waitForData;
     bool abort;
     GriddedData *gridData;
     RadarData *radarVolume;
     VortexData *vortexData;
     PressureList *pressureList;
     Configuration *configData;
     //Hvvp* hvvp;
     
     float* dataGaps;
     GBVTD* vtd;
     Coefficient* vtdCoeffs; 

     QString vortexPath;
     QString geometry;
     QString refField;
     QString velField;
     QString closure;
     float gradientHeight;
     float firstLevel;
     float lastLevel;
     float firstRing;
     float lastRing;
     float ringWidth;
     int maxWave;
     float maxObRadius;
     float maxObTimeDiff;
     float hvvpResult;
     float hvvpUncertainty;
     
     int numEstimates;
     PressureData presObs[101];

     float vtdStdDev;
     float convergingCenters;
     float rhoBar[16];
     void archiveWinds(float& radius,int& height,float& maxCoeffs);
     void archiveWinds(VortexData& data, float& radius,
		       int& height,float& maxCoeffs);
     // void getPressureDeficit(const float& height);
     void getPressureDeficit(VortexData* data, float* pDeficit, 
			     const float& height, float& kdim);
     void getPressureDeficit(VortexData* data, float* pDeficit, const float& height);
     //void calcCentralPressure();
     void getRhoDeficit(VortexData* data, float** rhoDeficit, float& kdim);
     void getDeficitFields(VortexData* data, float& kdim, float*** propGrid,float centralPressure);
     //void calcCentralPressure();
     float calcCentralPressure(VortexData* vortex, float* pD, float centralPressure, float height);
     float createRankine(float radius, float height, float H);
     void getTDeficit(VortexData* data,float** T1,float** P1,float** Rho1,float** theta1,float** thetadiff1,float** Tdeficit1,float** rhoDeficit, float& kdim, float centralPressure, float*** propGrid1);
     void calcPressureUncertainty(float setLimit, QString nameAddition);
     void storePressureUncertaintyData(QString& fileLocation);
     void readInConfig();
     bool calcHVVP(bool printOutput);

     float IT;
     float VMx, VMy, thetaT, Vmsin, Vmcos, VMCosCheck, height;
     float** vertex; 
     float* STD;
     float* vertexSum;
     float**  MeanWindField;
     int xdim, ydim, zdim;
     float maxCoeffs;
     int storageIndex;

     float simplexTest(int& low, float factor, int h);
     inline void getVertexSum();
     float IterateMeanWind(int h);
     void DownSimplex(float Vmguess, float thetaMguess, int h);

};

#endif
