/*
 *  AnalysisThread.cpp
 *  VORTRAC
 *
 *  Created by Michael Bell on 6/17/05.
 *  Copyright 2005 University Corporation for Atmospheric Research.
 *  All rights reserved.
 *
 */

#include <QtGui>
#include "AnalysisThread.h"
#include "Radar/RadarQC.h"
#include "DataObjects/CappiGrid.h"
#include "DataObjects/VortexData.h"
#include "Message.h"
#include "SimplexThread.h"
#include "HVVP/Hvvp.h"
#include "math.h"

AnalysisThread::AnalysisThread(QObject *parent)
  : QThread(parent)
{
  this->setObjectName("Analysis");
  //Message::toScreen("AnalysisThread Constructor");
  abort = false;
  simplexThread = new SimplexThread;
  connect(simplexThread, SIGNAL(log(const Message&)), this, 
		  SLOT(catchLog(const Message&)), Qt::DirectConnection);
  connect(simplexThread, SIGNAL(centerFound()), 
		  this, SLOT(foundCenter()), Qt::DirectConnection);
  vortexThread = new VortexThread;
  connect(vortexThread, SIGNAL(log(const Message&)), this, 
		  SLOT(catchLog(const Message&)), Qt::DirectConnection);
  connect(vortexThread, SIGNAL(windsFound()), 
		  this, SLOT(foundWinds()), Qt::DirectConnection);

  numVolProcessed = 0;
  analyticRun = false;
  configData = NULL;
  radarVolume = NULL;
  vortexList = NULL;
  // CurrentList = NULL;
  simplexList = NULL;
  pressureList = NULL;
  dropSondeList = NULL;

  gridFactory.setAbort(&abort);
}

AnalysisThread::~AnalysisThread()
{
  //Message::toScreen("AnalysisThread Destructor IN");
  abort = true;

  if(this->isRunning())
    this->abortThread();

  delete simplexThread; // This goes first so it can break the cycle which will
						// allow us a change to lock if we are in simplexThread
						// Otherwise we could wait a while for a lock
  simplexThread = NULL;
  
  delete vortexThread;    
  vortexThread = NULL;  

  delete radarVolume;
  radarVolume = NULL;
  
  this->quit();
  
  //Message::toScreen("AnalysisThread Destructor OUT");
}

void AnalysisThread::setConfig(Configuration *configPtr)
{

  configData = configPtr;

}

void AnalysisThread::setVortexList(VortexList *archivePtr)
{

  vortexList = archivePtr;

}

void AnalysisThread::setSimplexList(SimplexList *archivePtr)
{
	
  simplexList = archivePtr;
	
}

void AnalysisThread::setPressureList(PressureList *archivePtr)
{
	
	pressureList = archivePtr;
	
}

void AnalysisThread::setDropSondeList(PressureList *archivePtr)
{

        dropSondeList = archivePtr;

}

void AnalysisThread::abortThread()
{
  //Message::toScreen("In AnalysisThread Abort");
  
  mutex.lock();
  abort = true;
  mutex.unlock();
  
  // Wait for the thread to finish running if it is still processing
  if(this->isRunning()) {
    //Message::toScreen("This is running - analysisThread");
    waitForData.wakeOne();
    // Message::toScreen("WaitForData.wakeAll() - passed");
    wait();
    //Message::toScreen("Got past wait - AnalysisThread");
    // Got rid of wait because it was getting stuck when aborted in simplex cycle
	/* Done in Destructor now
    if(radarVolume!=NULL) {
      RadarData *temp = radarVolume;
      radarVolume = NULL;
      delete temp;
    } */
  }
  //Message::toScreen("Leaving AnalysisThread Abort");
}

void AnalysisThread::analyze(RadarData *dataVolume, Configuration *configPtr)
{
	// Lock the thread
	QMutexLocker locker(&mutex);

	this->radarVolume = dataVolume;
	configData = configPtr;
	radarVolume->setAltitude(configData->getParam(configData->getConfig("radar"),"alt").toFloat()/1000.0);
	//Message::toScreen("AnalysisThread: Getting New RadarVolume: # Rays = "+radarVolume->getDateTimeString());
	// Sets altitude of radar to radarData (km) from ConfigData (meters)

	// Start or wake the thread
	if(!isRunning()) {
	  start();
	} else {
	  //Message::toScreen("found running copy of analysis... waiting....");
		waitForData.wakeOne();
	}
}

void AnalysisThread::run()
{
  //  Message::toScreen("Entering AnalysisThread - Run");
  abort = false;

       forever {
	 
	 //Message::toScreen("AnalysisThread: Starting From the Top");
	 
	 // Check to see if we should quit
	 if (abort) 
	   return; 
	 
	 // OK, Let's process some radar data
	 mutex.lock();

	 //Check for research mode
	 QDomElement research = configData->getConfig("research");
	 QString RunMode = configData->getParam(research,"run_mode");
	 bool CappiON = configData->getParam(research,"cappi").toInt();
	 bool SimplexON = configData->getParam(research,"simplex").toInt();
	 bool VtdON = configData->getParam(research,"vtd").toInt();
	 bool QCON = configData->getParam(research,"qc").toInt();
	 if (RunMode == "operations") {
	   CappiON = 1;
	   SimplexON = 1;
	   VtdON = 1;
	   QCON = 1;
	 }

	 //this is used in analytic runs, set to default here
	 bool ForceCenter = false;

	 

	 bool analysisGood = true;
	 emit log(Message(QString(),0,this->objectName(),Green));
	 QTime analysisTime;
	 analysisTime.start();    // Timer used for user info only
	 
	 bool beyondRadar;
	 
	 // Check the vortexList for a current center to use
	 // as the starting point for analysis
	 
	 if (!vortexList->isEmpty()) {

	   vortexList->timeSort();
	   
	   if (SimplexON == 0) {
	     //looping here to find the index, i, that corresponds to the time of the current volume
// 	     VortexList CurrentList = *vortexList;
// 	     //*CurrentList = *vortexList;
// 	     int i = 5; //doesnt matter what it is initially
// 	     QString forstr = "ddd MMMM d yy hh:mm:ss";
// 	     for (int jj = vortexList->size()-1; jj>=0; jj--) {
// 	       QString t1 = vortexList->at(jj).getTime().toString(forstr);
// 	       QString t2 = radarVolume->getDateTime().toString(forstr);
// 	       if (t1 == t2)  {i = jj;}
// 	       else {CurrentList.removeAt(jj);}
// 	     }
	     
// 	     vortexLat = CurrentList.last().getLat();
// 	     vortexLon = CurrentList.last().getLon();
	     
	     //looping here to find the index, i, that corresponds to the time of the current volume
	     int i = 5; //doesnt matter what it is initially
	     QString forstr = "ddd MMMM d yy hh:mm:ss";
	     for (int jj = vortexList->size()-1; jj>=0; jj--) {
	       QString t1 = vortexList->at(jj).getTime().toString(forstr);
	       QString t2 = radarVolume->getDateTime().toString(forstr);
	       if (t1 == t2)  {i = jj; break;}
	     }
	     
 	     vortexLat = vortexList->at(i).getLat();
	     vortexLon = vortexList->at(i).getLon();
	   }
	   else{
	     vortexLat = vortexList->last().getLat();
	     vortexLon = vortexList->last().getLon();
	   }


	   if(numVolProcessed > 0) {
	     // If only a few volumes have been attempted then we will use 
	     // the storm motion to 'bump' the storm into range
	     // Get direction (degrees cw from north) and speed (m/s)
	     // of initial storm position

	     QDomElement vortex = configData->getConfig("vortex");
	     float stormSpeed = configData->getParam(vortex, 
						     "speed").toFloat();
	     float stormDirection = configData->getParam(vortex, 
							 "direction").toFloat();
	     
	     // Put the storm direction in math coordinates 
	     // radians ccw from east
	     
	     stormDirection = 450-stormDirection;
	     if(stormDirection > 360)
	       stormDirection -=360;
	     stormDirection*=acos(-1)/180.;
	     
	     QDateTime obsDateTime = vortexList->last().getTime();
	     QDateTime volDateTime = radarVolume->getDateTime();

	     int elapsedSeconds =obsDateTime.secsTo(volDateTime);
	     //Message::toScreen("Seconds since start "+QString().setNum(elapsedSeconds)+" in AnalysisThread");
	     if(isnan(elapsedSeconds)) {
	       emit log(Message(QString("Cannot calculate time until storm is in range of radar, Please check the observation time, latitude, longitude, and storm movement parameters"),0,this->objectName(),Yellow,QString("Can not calculate time until storm in range")));
	       beyondRadar = false;
		    }
	     float distanceMoved = elapsedSeconds*stormSpeed/1000.0;
	     float changeInX = distanceMoved*cos(stormDirection);
	     float changeInY = distanceMoved*sin(stormDirection);
	     QString message("changeInX = "+QString().setNum(changeInX)+" changeInY = "+QString().setNum(changeInY));
	     //emit(log(Message(message,0,this->objectName())));
	     float *newLatLon = GriddedData::getAdjustedLatLon(vortexLat,vortexLon, changeInX, changeInY);

		 // Get initial lat and lon
	     float initLat = configData->getParam(vortex,"lat").toFloat();
	     float initLon = configData->getParam(vortex,"lon").toFloat();
	     QString dateString = configData->getParam(vortex,"obsdate");
	     QString timeString = configData->getParam(vortex,"obstime");
	     QDate obsDate = QDate::fromString(configData->getParam(vortex,"obsdate"),"yyyy-MM-dd");
	     //Message::toScreen("obs: "+obsDate.toString("yyyy-MM-dd"));
	     QTime obsTime = QTime::fromString(timeString,"hh:mm:ss");
	     //Message::toScreen("obs: "+obsTime.toString("hh:mm:ss"));
	     obsDateTime = QDateTime(obsDate, obsTime, Qt::UTC);
	     if(!obsDateTime.isValid()){
	       emit log(Message(QString("Observation Date or Time is not of valid format! Date: yyyy-MM-dd Time: hh:mm:ss please adjust the configuration file"),0,this->objectName(),Red, QString("ObsDate or ObsTime invalid in Config")));
	       mutex.unlock();
	       abort = true;
	       return;
	     }
	     elapsedSeconds =obsDateTime.secsTo(volDateTime);
	     //Message::toScreen("Seconds since start "+QString().setNum(elapsedSeconds)+" in AnalysisThread");
	     if(isnan(elapsedSeconds)) {
	       emit log(Message(QString("Cannot calculate time until storm is in range of radar, Please check the observation time, latitude, longitude, and storm movement parameters"),0,this->objectName(),Yellow,QString("Can not calculate time until storm in range")));
	       beyondRadar = false;
		    }
	     distanceMoved = elapsedSeconds*stormSpeed/1000.0;
	     changeInX = distanceMoved*cos(stormDirection);
	     changeInY = distanceMoved*sin(stormDirection);
	     float *extrapLatLon = GriddedData::getAdjustedLatLon(initLat,initLon, changeInX, changeInY);
	     float relDist = GriddedData::getCartesianDistance(&extrapLatLon[0],&extrapLatLon[1],&newLatLon[0],&newLatLon[1]);

		 // Check if Simplex is out to lunch
	     if (relDist > 150) {
			 QString distString;
			 QString M1 = "Simplex center "+distString.setNum((int)relDist)+" (>150 km) from User estimated center, probably lost or estimate is ";
			 QString M2 = "Simplex Center/Estimate "+distString.setNum((int)relDist)+" km apart";
			 emit log(Message(M1,0,this->objectName(),Red, M2));
			 mutex.unlock();
			 abort = true;
			 return;
		 } else if (relDist > 75) {
			 QString distString;
			 QString M1 = "Simplex center "+distString.setNum((int)relDist)+" (>75 km) from User estimated center, may be lost or need to update estimate";
			 QString M2 = "Simplex Center/Estimate "+distString.setNum((int)relDist)+" km apart";
			 emit log(Message(M1,0,this->objectName(),Yellow, M2));
		 }
	     // Message::toScreen("Old vortexLat = "+QString().setNum(vortexLat)+" Old vortexLon = "+QString().setNum(vortexLon));
	     // Message::toScreen("New vortexLat = "+QString().setNum(newLatLon[0])+" New vortexLon = "+QString().setNum(newLatLon[1]));
		 
	     vortexLat = newLatLon[0];
	     vortexLon = newLatLon[1];
	     delete [] newLatLon;
	     delete [] extrapLatLon;
	   }
	 } 
	 else {
	   // If the vortexList is empty than we must load 
	   // information from the configuration to help 
	   // identify our guess center.

	   // This set initializes all the data products lists
	   // which keep a history of the analysis points in the 
	   // interface and on disk.
	   
	   QDomElement radar = configData->getConfig("radar");
	   QDomElement vortex = configData->getConfig("vortex");
	   
	   if(numVolProcessed == 0) {
	     // If no volumes have been attempted then we will use 
	     // the guess center directly from the xml configuration file.
	     
	     QString radarName = configData->getParam(radar,"name");
	     QString vortexName = configData->getParam(vortex,"name");
	     
	     QString year;
	     year.setNum(QDate::fromString(configData->getParam(radar,"startdate"), "yyyy-MM-dd").year());
	     
	     // Initializing Vortex List
	     QString workingPath = configData->getParam(configData->getConfig("vortex"),"dir");
	     QString vortexPath = configData->getParam(configData->getConfig("vtd"), "dir");
	     QString outFileName = workingPath + "/"+vortexName+"_"+radarName+"_"+year+"_vortexList.xml";
	     vortexList->setFileName(outFileName);
	     vortexList->setRadarName(radarName);
	     vortexList->setVortexName(vortexName);
	     vortexList->setNewWorkingDirectory(vortexPath + "/");
	     
	     // Initializing Simplex List
	     
	     QString simplexPath = configData->getParam(configData->getConfig("center"),"dir");
	     outFileName = workingPath + "/"+vortexName+"_"+radarName+"_"+year+"_simplexList.xml";
	     simplexList->setFileName(outFileName);
	     simplexList->setRadarName(radarName);
	     simplexList->setVortexName(vortexName);
	     simplexList->setNewWorkingDirectory(simplexPath + "/");
	     
	     // Initial Pressure Observation List
	     // Put the pressure output in the workingDir 
	     
	     outFileName = workingPath + "/"+vortexName+"_"+radarName+"_"+year+"_pressureList.xml";
	     pressureList->setFileName(outFileName);
	     pressureList->setRadarName(radarName);
	     pressureList->setVortexName(vortexName);
	     pressureList->setNewWorkingDirectory(workingPath + "/");
	     
	     // Initialize DropSonde List
	     // Put the dropSonde output in the workingDir for now
	     
	     outFileName = workingPath + "/"+vortexName+"_"+radarName+"_"+year+"_dropSondeList.xml";
	     dropSondeList->setFileName(outFileName);
	     dropSondeList->setRadarName(radarName);
	     dropSondeList->setVortexName(vortexName);
	     dropSondeList->setNewWorkingDirectory(workingPath + "/");
	   }
	   
	   // Check to make sure that the radar volume is in range
	   // get start date and time from the observation date and time
	   // in the vortex panel of the configuration.
	   
	   QString dateString = configData->getParam(vortex,"obsdate");
	   QString timeString = configData->getParam(vortex,"obstime");
	   QDate obsDate = QDate::fromString(configData->getParam(vortex,"obsdate"),"yyyy-MM-dd");
	   //Message::toScreen("obs: "+obsDate.toString("yyyy-MM-dd"));
	   QTime obsTime = QTime::fromString(timeString,"hh:mm:ss");
	   //Message::toScreen("obs: "+obsTime.toString("hh:mm:ss"));
	   QDateTime obsDateTime = QDateTime(obsDate, obsTime, Qt::UTC);
	   if(!obsDateTime.isValid()){
		   emit log(Message(QString("Observation Date or Time is not of valid format! Date: yyyy-MM-dd Time: hh:mm:ss please adjust the configuration file"),0,this->objectName(),Red, QString("ObsDate or ObsTime invalid in Config")));
		   mutex.unlock();
		   abort = true;
		   return;
	   }

	   QString startDate = configData->getParam(radar,"startdate");
	   QString startTime = configData->getParam(radar,"starttime");
	   QDateTime radarStart(QDate::fromString(startDate, Qt::ISODate), 
				QTime::fromString(startTime, Qt::ISODate), 
				Qt::UTC);

	   if (obsDateTime.secsTo(radarStart)>(3600*6)) {
	     // Trying to extrapolate too far, bail out
		   emit log(Message(QString("Extrapolation time exceeds six hours, Please check the observation time, latitude, longitude, and storm movement parameters"),0,this->objectName(),Red,QString("Excess extrapolation time")));
		   mutex.unlock();
		   abort = true;
		   return;
	   } else if (obsDateTime.secsTo(radarStart)<0) {
	     // Trying to extrapolate into the past, bail out
		   emit log(Message(QString("Extrapolation time exceeds six hours, Please check the observation time, latitude, longitude, and storm movement parameters"),0,this->objectName(),Red,QString("Negative extrapolation time")));
		   mutex.unlock();
		   abort = true;
		   return;
	   }

	   // Get this volume's time
	   //Message::toScreen("obs: "+obsDateTime.toString(Qt::ISODate));
	   
	   QDateTime volDateTime = radarVolume->getDateTime();
	   //Message::toScreen("vol: "+volDateTime.toString(Qt::ISODate));
	   
	   // If the volume time is with 15 minutes of the start time
	   // for accepting radar observations then we will use the 
	   // given vortex center for our center
	   // Message::toScreen(" secs between start and this one "+QString().setNum(obsDateTime.secsTo(volDateTime)));
	   
	   if((abs(obsDateTime.secsTo(volDateTime))<15*60)
	      ||(analyticRun)) {
	     // Message::toScreen("Using lat lon in Config");
	     vortexLat = configData->getParam(vortex,"lat").toFloat();
	     vortexLon = configData->getParam(vortex,"lon").toFloat();
	   } else {
	     //Message::toScreen("Using Velocity");
	     // Use the starting position in conjunction with the storm
	     // velocity to find a current guess for the vortex center
	     
	     // Get direction (degrees cw from north) and speed (m/s)
	     // of initial storm position
	     
	     float stormSpeed = configData->getParam(vortex, 
						     "speed").toFloat();
	     float stormDirection = configData->getParam(vortex, 
							 "direction").toFloat();
	     
	     // Put the storm direction in math coordinates 
	     // radians ccw from east
	     
	     stormDirection = 450-stormDirection;
	     if(stormDirection > 360)
	       stormDirection -=360;
	     stormDirection*=acos(-1)/180.;
	     
	     // Get initial lat and lon
	     vortexLat = configData->getParam(vortex,"lat").toFloat();
	     vortexLon = configData->getParam(vortex,"lon").toFloat();
		    
	     int elapsedSeconds =obsDateTime.secsTo(volDateTime);
	     //Message::toScreen("Seconds since start "+QString().setNum(elapsedSeconds)+" in AnalysisThread");
	     if(isnan(elapsedSeconds)) {
	       emit log(Message(QString("Cannot calculate time until storm is in range of radar, Please check the observation time, latitude, longitude, and storm movement parameters"),0,this->objectName(),Yellow,QString("Can not calculate time until storm in range")));
	       beyondRadar = false;
		    }
	     float distanceMoved = elapsedSeconds*stormSpeed/1000.0;
	     float changeInX = distanceMoved*cos(stormDirection);
	     float changeInY = distanceMoved*sin(stormDirection);
	     QString message("changeInX = "+QString().setNum(changeInX)+" changeInY = "+QString().setNum(changeInY));
	     //emit(log(Message(message,0,this->objectName())));
	     float *newLatLon = GriddedData::getAdjustedLatLon(vortexLat,vortexLon, changeInX, changeInY);

	     vortexLat = newLatLon[0];
	     vortexLon = newLatLon[1];
	     delete [] newLatLon;
	     //Message::toScreen("New vortexLat = "+QString().setNum(vortexLat)+" New vortexLon = "+QString().setNum(vortexLon));
	   }
	 }
	 
	 QString currentCenter("Using ("+QString().setNum(vortexLat)+", "+QString().setNum(vortexLon)+") for storm center estimate");

	 emit log(Message(currentCenter,1,this->objectName())); // 5 %
	 
	 // Check to see if the center is beyond 174 km
	 // If so, tell the user to wait!
	 //Message::toScreen("RLat = "+QString().setNum(*radarVolume->getRadarLat())+" RLon = "+QString().setNum(*radarVolume->getRadarLon())+" VLat = "+QString().setNum(vortexLat)+" VLon = "+QString().setNum(vortexLon));
	 float relDist = GriddedData::getCartesianDistance(
				             radarVolume->getRadarLat(), 
					     radarVolume->getRadarLon(),
					     &vortexLat, &vortexLon);
		
	 //Message::toScreen("Distance Between Radar and Storm "+QString().setNum(relDist));
	 beyondRadar = true;
	 float unambigRange = 0;
	 //bool closeToEdge = false;
	 for(int i = 0; i < radarVolume->getNumSweeps(); i++) {	   
	   if (radarVolume->getSweep(i)->getVel_numgates()> 0) {
	     float range = radarVolume->getSweep(i)->getUnambig_range();
	     if (range > unambigRange) {
	       unambigRange = range;
	     }
	   } 
	   int rangeBuffer = 5;
	   if((relDist < (radarVolume->getSweep(i)->getUnambig_range()+rangeBuffer))
	      &&(radarVolume->getSweep(i)->getVel_numgates()> 0)){
	     beyondRadar = false;
	   }
	   //  if((relDist > radarVolume->getSweep(i)->getUnambig_range()-20)&&(relDist < radarVolume->getSweep(i)->getUnambig_range()+20)) {
	   //closeToEdge = true;
	   //}
	 }
	 
	 // Used for analytic runs only
	 if(analyticRun)
	   beyondRadar = false;
	 
	 if (beyondRadar) {
	   // Too far away for Doppler, need to send a signal
	   // Calculate the estimated time of arrival
	   float* distance;

	   distance = GriddedData::getCartesianPoint(
					    &vortexLat,&vortexLon,
					    radarVolume->getRadarLat(), 
					    radarVolume->getRadarLon());
	   float cca = atan2(distance[0], distance[1]);
	   
	   QDomElement vortex = configData->getConfig("vortex");
	   float stormSpeed = configData->getParam(vortex, 
						   "speed").toFloat()/1000.0;
	   float stormDirection = configData->getParam(vortex, 
				       "direction").toFloat()*acos(-1)/180.;
	   // Message::toScreen("Storm Direction .."+QString().setNum(stormDirection));
	   //Message::toScreen("cca = "+QString().setNum(cca));
	   //float palpha = (relDist*sin(stormDirection-cca)/unambigRange);
	   float palpha = -1*(relDist*sin(stormDirection-cca)/unambigRange);
	   //Message::toScreen(" palpha = "+QString().setNum(palpha));
	   //Message::toScreen(" relDist = "+QString().setNum(relDist));
	   //float alpha = acos(-1)-asin(palpha);
	   float alpha = asin(palpha);
	   //Message::toScreen(" alpha = "+QString().setNum(alpha));
	   float dist2go = 0;
	   if(fabs(stormDirection-cca)<=1) {
	     dist2go = fabs(relDist)-unambigRange;
	   }
	   else {
	     //dist2go = unambigRange*sin(acos(-1)+cca-stormDirection-alpha)/sin(stormDirection-cca);
	     dist2go = sqrt(relDist*relDist*(1-2*sin(stormDirection-cca)*sin(stormDirection-cca))+unambigRange*unambigRange-2*relDist*unambigRange*cos(alpha)*cos(stormDirection-cca));
	   }
	   
	   // Report distance from Doppler Radar Range
	   
	   QString distanceLeft("Circulation Center is "+QString().setNum(relDist)+" km from the radar - Skipping Analysis On This Volume");
	   emit log(Message(distanceLeft,0,this->objectName()));
	   if (relDist > 500) {
		   emit log(Message(QString("Storm position is beyond 500 km from radar, Please check observation parameters"),0,this->objectName(),Red, QString("Storm > 500 km from radar")));
		   mutex.unlock();
		   abort = true;
		   return;
	   }
	   
	   //Message::toScreen(" dist2go = "+QString().setNum(dist2go));
	   
	   if((stormSpeed ==0)||isnan(stormSpeed)||isinf(stormSpeed)) {
	     // Can not calculate time to radar edge estimate
	     // Send Message....
	     QString noTime("Unable to calculate circulation center arrival time, check circulation speed and direction");
	     emit log(Message(noTime, 0,this->objectName(),AllOff,QString(),
			      OutOfRange,QString("Storm Out Of Range")));
	   }
	   else { 
	     
	     float eta = (dist2go/stormSpeed)/60;
	     //Message::toScreen("minutes till radar"+QString().setNum(eta));
	     
	     // These is something wrong with our calculation of time
	     // until storm is in range, stop processing
		   
	     if((eta < 0)||(isnan(eta))||isinf(eta)) {
			 QString ETA;
			 if(eta < 0)
				 ETA = "ETA < 0";
			 if(isnan(eta))
				 ETA = "ETA is NAN";
			 if(isinf(eta))
				 ETA = "ETA is infinite";
	       emit log(Message(QString("Difficulties Processing Time Until Radar is in Range, Please check observation parameters"),0,this->objectName(),Red, ETA));
		   mutex.unlock();
		   abort = true;
		   return;
	     }
	     
	     emit log(Message(
		      QString(),
		      -1,this->objectName(),AllOff,QString(),OutOfRange, 
		      QString("Storm in range in "+QString().setNum(eta, 
						  'f', 0)+" min")));
	     //Message::toScreen("Estimated center is out of Doppler range!");
	     // We are now deleting the volume here
	     // but keeping the pointer live for future iterations
	     // have to delete here, erasing address from pollThread
	   }
	   RadarData *temp = radarVolume;
	   radarVolume = NULL;
	   delete temp;
	   
	   //Message::toScreen("AnalysisThread: Volume Not In Range - Sending Done Processing Signal");
	   
	   emit doneProcessing();
	   //Message::toScreen("AnalysisThread sent doneProcessing");
	   waitForData.wait(&mutex);
	   //Message::toScreen("AnalysisThread: Got Mutex Back For Data - Continuing");
	   mutex.unlock();
	   delete [] distance;
	   continue;
	 }

	 // Volume is valid and in range
	 // Prepare to process volume....
	 
	 //Message::toScreen("gets to create vortexData analysisThread");
	 // Create data instance to hold the analysis results
	 
	 // Use size hints from vtd panel
	 QDomElement vtd = configData->getConfig("vtd");
	 QDomElement cappi = configData->getConfig("cappi");
	 float top = configData->getParam(vtd, "toplevel").toFloat();
	 float bottom = configData->getParam(vtd, "bottomlevel").toFloat();
	 float zSpacing = configData->getParam(cappi, "zgridsp").toFloat();
	 int numLevels = (int)floor((top-bottom)/zSpacing + 1.5);
	 float inner = configData->getParam(vtd, "innerradius").toFloat();
	 float outer = configData->getParam(vtd, "outerradius").toFloat();
	 float ringwidth = configData->getParam(vtd, "ringwidth").toFloat();
	 int numRings = (int)floor((outer-inner)/ringwidth + 1.5);
	 int numWaveNum = configData->getParam(vtd,"maxwavenumber").toInt();
	 VortexData *vortexData = new VortexData(numLevels,numRings,
						 numWaveNum);
	 //VortexData *vortexData = new VortexData(); 
	 vortexData->setTime(radarVolume->getDateTime());
	 for(int i = 0; i < vortexData->getNumLevels(); i++) {
	   vortexData->setLat(i,vortexLat);
	   vortexData->setLon(i,vortexLon);
	   vortexData->setHeight(i,bottom+(zSpacing*(float)i));
	 }		
	 
	 mutex.unlock();
	 if(abort)
	   return;
	 mutex.lock();
	 
	 // Pass VCP value to display
	 emit newVCP(radarVolume->getVCP());
	 
	 if (QCON) {

		// Dealias 		
		if(!radarVolume->isDealiased()){
		  
		  RadarQC *dealiaser = new RadarQC(radarVolume);

		  connect(dealiaser, SIGNAL(log(const Message&)), this, 
			  SLOT(catchLog(const Message&)), Qt::DirectConnection);
		  dealiaser->getConfig(configData->getConfig("qc"));
		  
		  if(dealiaser->dealias()) {
		    emit log(Message("Finished QC and Dealiasing", 
				     1, this->objectName()));  // 10 %
		    radarVolume->isDealiased(true);
		  } else {
		    emit log(Message(QString("Finished Dealias Method with Failures"),0,this->objectName()));
		    analysisGood = false;
		    // Something went wrong
		    // We should probably add a return here of some sort...
		  }
		  delete dealiaser;
		}
		else
		  emit log(Message(QString("RadarVolume is Dealiased"),0,this->objectName()));
	 
		/*
			 // Using this for running FORTRAN version
			 QString name("fchar1824Verify.dat");
			 Message::toScreen("Writing Vortrac Input "+name);
			 radarVolume->writeToFile(name);
			 Message::toScreen("Wrote Vortrac Input "+name);
	    
			 // Testing HVVP before Cappi to save time 
			 // only good for Charley 1824
			 
			 float rt1 = 167.928;
			 float cca1 = 177.204;
			 float rmw1 = 11;
			 
			 // Testing HVVP before Cappi to save time 
			 // only good for Charley 140007
			 
			 float rt1 = 87.7712;
			 float cca1 = 60.1703;
			 float rmw1 = 16.667;
			 
			 
			 // Testing HVVP before Cappi to save time 
			 // only good for Charley 140007
			 
			 float rt1 = 88.8294;
			 float cca1 = 63.466;
			 float rmw1 = 15.667;
			 
			 
			 // Testing HVVP before Cappi to save time 
			 // only good for Katrina 0933
			 
			 float rt1 = 164.892;
			 float cca1 = 172.037;
			 float rmw1 = 31;
			 
			 
			 // Testing HVVP before Cappi to save time 
			 // only good for Katrina 1044
			 
			 float rt1 = 131.505;
			 float cca1 = 168.458;
			 float rmw1 = 25;
		
			 
			 // Testing HVVP before Cappi to save time 
			 // only good for Analytic Charley
			 
			 float rt1 = 70.60;
			 float cca1 = 45.19;
			 float rmw1 = 7;
			
			 Message::toScreen("Vortex (Lat,Lon): ("+QString().setNum(vortexLat)+", "+QString().setNum(vortexLon)+")");
			 Message::toScreen("Hvvp Parameters: Distance to Radar "+QString().setNum(rt1)+" angel to vortex center in degrees ccw from north "+QString().setNum(cca1)+" rmw "+QString().setNum(rmw1));
			 
			 Hvvp *envWindFinder1 = new Hvvp;
			 connect(envWindFinder1, SIGNAL(log(const Message&)),
				 this, SLOT(catchLog(const Message&)), 
				 Qt::DirectConnection);
			 envWindFinder1->setConfig(configData);
			 envWindFinder1->setPrintOutput(true);
			 envWindFinder1->setRadarData(radarVolume,
						      rt1, cca1, rmw1);
			 if(!envWindFinder1->findHVVPWinds(true)){
			   
			 }
			 float missingLink1 = envWindFinder1->getAvAcrossBeamWinds();
			 Message::toScreen("Hvvp gives "+QString().setNum(missingLink1));
			 delete envWindFinder1;
			 
			 return;
		*/ // comments end here
	 }		
	 
		/* mutex.lock(); 
		 *
		 * We have to leave this section unlocked so that a change 
		 * in abort can occur and be sent through the chain to 
		 * cappi routines, otherwise the thread cannot exit until 
		 * they return -LM
		 */
	      

		if (CappiON)
		  {

		    mutex.unlock();
		    if(abort)
		      return;
		    // Create CAPPI
		    
		    emit log(Message(QString("Creating CAPPI..."), 0, 
				     this->objectName()));
		    
		    /*  If Analytic Model is running we need to make an analytic
		     *  gridded data rather than a cappi
		     */
		    //GriddedData *gridData;
		    QDomElement radar = configData->getConfig("radar");
		    QString format = configData->getParam(radar,"format");
		    
		    if(format == "MODEL") {

		      //so that we include ALL wavenumbers in analytic vortex
		      vortexData->setNumWaveNum(2);

		      Configuration *analyticConfig = new Configuration();
		      float radarLat = configData->getParam(radar,"lat").toFloat();
		      float radarLon = configData->getParam(radar,"lon").toFloat();
		      analyticConfig->read(configData->getParam(radar, "dir"));

		      QDomElement winds =analyticConfig->getConfig("wind_field");
		      QString forceCenterFlag = analyticConfig->getParam(winds, "force_center");
		      if (forceCenterFlag == "true") {
			ForceCenter = true;
		      }

		      gridData = gridFactory.makeAnalytic(radarVolume,
							  configData,analyticConfig, 
							  &vortexLat, &vortexLon, 
							  &radarLat, &radarLon);
		    }
		    else {
		      //Message::toScreen("Before Making Cappi vLat = "+QString().setNum(vortexLat)+" vLon = "+QString().setNum(vortexLon));
		      
		      gridData = gridFactory.makeCappi(radarVolume, configData,
						       &vortexLat, &vortexLon);
		      //Message::toScreen("AnalysisThread: outside makeCappi");
		      
		    }
		    
		    emit log(Message("Done with Cappi",15,this->objectName()));
		    
		    if(abort)
		      return;
		    
		    // Pass Cappi to display
		    emit newCappi(gridData);
		    //emit newCappiInfo(.3,.3,.1,.2,.3);
		    
		    
		    if(abort)
		      return;
		    mutex.lock();
		    
		    /* GriddedData not currently a QObject so this will fail
		       connect(gridData, SIGNAL(log(const Message&)),this,
		       SLOT(catchLog(const Message&)), Qt::DirectConnection); */
		
		    // Output Radar data to check if dealias worked
		    
		    gridData->writeAsi();
		    emit log(Message("Wrote Cappi To File",5,this->objectName()));
		    QString cappiTime;
		    cappiTime.setNum((float)analysisTime.elapsed() / 60000);
		    cappiTime.append(" minutes elapsed");
		    emit log(Message(cappiTime));
		    
		  }
		
		//Read in cappi grid from file if CappiON is false 
		if (CappiON == 0) {
		  gridData = gridFactory.readCappi(radarVolume, configData,
						   &vortexLat, &vortexLon);
				  
		  emit log(Message("Done reading Cappi",15,this->objectName()));
		  
		  if(abort)
		    return;
		  
		  // Pass Cappi to display
		  emit newCappi(gridData);

		 //  gridData->writeAsi();
// 		  emit log(Message("Wrote Cappi To File",5,this->objectName()));
		  
		  
		}
       
       
		if (SimplexON) {
	 

		  // Set the initial guess in the data object as a temporary center
		  vortexData->setLat(0,vortexLat);
		  vortexData->setLon(0,vortexLon);
		  
		  //Find Center 
		  simplexThread->findCenter(configData, gridData, radarVolume, 
					    simplexList, vortexData, ForceCenter);
		  waitForCenter.wait(&mutex); 
		  
		  QString simplexTime;
		  simplexTime.setNum((float)analysisTime.elapsed() / 60000);
		  simplexTime.append(" minutes elapsed");
		  emit log(Message(simplexTime));
		  
		  // Send info about simplex search to cappiDisplay 
		  
		  int vortexIndex = vortexData->getHeightIndex(1);
		  if(vortexIndex == -1)
		    vortexIndex = 0;
		  float levelLat = vortexData->getLat(vortexIndex);
		  float levelLon = vortexData->getLon(vortexIndex);
		  QDomElement radar = configData->getConfig("radar");
		  QDomElement simplex = configData->getConfig("center");
		  float radarLat = configData->getParam(radar,"lat").toFloat();
		  float radarLon = configData->getParam(radar,"lon").toFloat();
		  float* xyValues = gridData->getCartesianPoint(&radarLat, &radarLon, &levelLat, &levelLon);
		  float xPercent = float(gridData->getIndexFromCartesianPointI(xyValues[0])+1)/gridData->getIdim();
		  //Message::toScreen("xPercent is "+QString().setNum(xPercent));
		  float yPercent = float(gridData->getIndexFromCartesianPointJ(xyValues[1])+1)/gridData->getJdim();
		  //Message::toScreen("yPercent is "+QString().setNum(yPercent));
		  float sMin = configData->getParam(simplex, "innerradius").toFloat()/(gridData->getIGridsp()*gridData->getIdim());
		  //Message::toScreen("sMin is "+QString().setNum(sMin));
		  float sMax = configData->getParam(simplex, "outerradius").toFloat()/(gridData->getIGridsp()*gridData->getIdim());
		  //Message::toScreen("sMax is "+QString().setNum(sMax));
		  float vMax = configData->getParam(vtd, "outerradius").toFloat()/(gridData->getIGridsp()*gridData->getIdim());
		  //Message::toScreen("vMax is "+QString().setNum(vMax));
		  emit newCappiInfo(xPercent, yPercent, sMin, sMax, vMax, levelLat, levelLon);
		  delete [] xyValues;
		  
		}
       
		if (SimplexON == 0) {
  
		  // Already loaded the new simplex data in PollThread, now peform the second step:

	// 	  float firstLevel = configData->getParam(simplexConfig,
// 						    QString("bottomlevel")).toFloat();
// 		  float lastLevel = configData->getParam(simplexConfig,
// 						   QString("toplevel")).toFloat();
		  
// 		  int numLevels = int(lastLevel-firstLevel+1);
// 		  int loopPercent = int(20.0/float(numLevels));
// 		  int endPercent = 20-(numLevels*loopPercent);
		  
// 		  emit log(Message(QString(),1+endPercent,this->objectName()));
    
		  //Now pick the best center
		  simplexResults = simplexList;
		  simplexResults->timeSort();
		  ChooseCenter *centerFinder = new ChooseCenter(configData,simplexResults,
								vortexData);
		  
		  connect(centerFinder, SIGNAL(errorlog(const Message&)),
			  this, SLOT(catchLog(const Message&)),Qt::DirectConnection);
	
		  centerFinder->setForceCenter(ForceCenter);
		  bool foundCenter = centerFinder->findCenter();
		  
		  // Save to keep mean values found in chooseCenter
		  simplexResults->save();
		  
		  emit log(Message(QString(),4,this->objectName()));
		  
		  // Clean up
		  delete centerFinder;
			  
		  //signal that simplex is done so program can move on
		  if(!foundCenter)
		    {
		      // Some error occurred, notify the user
		      emit log(Message(QString("Failed to Indentify Center!"),0,this->objectName(),AllOff,QString(),SimplexError,QString("Simplex Failed")));
		      // return;
		    } else {
		      // Update the vortex list
		      emit log(Message(QString("Done with Simplex"),0, this->objectName()));
		      
		      // Let the poller know we're done
		      //     emit(simplexThread->centerFound());
		    }
	// 	  mutex.unlock();
		  
// 		  // Go to sleep, wait for more data   
// 		  if (!abort) {
// 		    mutex.lock();
// 		    // Wait until new data is available
// 		    waitForData.wait(&mutex);	
// 		    mutex.unlock();    
// 		  }
// 		  if(abort)
// 		    return;
		
		
		  // Send info about simplex search to cappiDisplay 
		  int vortexIndex = vortexData->getHeightIndex(1);
		  if(vortexIndex == -1) {
		    vortexIndex = 0;
		  }
		  float levelLat = vortexData->getLat(vortexIndex);
		  float levelLon = vortexData->getLon(vortexIndex);
		  QDomElement radar = configData->getConfig("radar");
		  QDomElement simplex = configData->getConfig("center");
		  float radarLat = configData->getParam(radar,"lat").toFloat();
		  float radarLon = configData->getParam(radar,"lon").toFloat();
		  float* xyValues = gridData->getCartesianPoint(&radarLat, &radarLon, &levelLat, &levelLon);
		  float xPercent = float(gridData->getIndexFromCartesianPointI(xyValues[0])+1)/gridData->getIdim();
		  //Message::toScreen("xPercent is "+QString().setNum(xPercent));
		  float yPercent = float(gridData->getIndexFromCartesianPointJ(xyValues[1])+1)/gridData->getJdim();
		  //Message::toScreen("yPercent is "+QString().setNum(yPercent));
		  float sMin = configData->getParam(simplex, "innerradius").toFloat()/(gridData->getIGridsp()*gridData->getIdim());
		  //Message::toScreen("sMin is "+QString().setNum(sMin));
		  float sMax = configData->getParam(simplex, "outerradius").toFloat()/(gridData->getIGridsp()*gridData->getIdim());
		  //Message::toScreen("sMax is "+QString().setNum(sMax));
		  float vMax = configData->getParam(vtd, "outerradius").toFloat()/(gridData->getIGridsp()*gridData->getIdim());
		  //Message::toScreen("vMax is "+QString().setNum(vMax));
		  emit newCappiInfo(xPercent, yPercent, sMin, sMax, vMax, levelLat, levelLon);
		  delete [] xyValues;
		}
		mutex.unlock();  			  
		
		
		if (VtdON) {
		  
		  if (!abort) {
		    
		    mutex.lock();
		    // Get the GBVTD winds
		    emit log(Message(QString(), 2,this->objectName()));
		    vortexThread->getWinds(configData, gridData, radarVolume, 
					   vortexData, pressureList);
		    waitForWinds.wait(&mutex); 
		    mutex.unlock();
		  }
		  
		  if(abort)
		    return;  
		} 
		// Should have all relevant variables now
		// Update timeline and output


		// Check to see if the vortex data is valid
		// Threshold on std deviation of parameters in
		// conjuction with distance or something

		mutex.lock();

		QString vortexTime;
		vortexTime.setNum((float)analysisTime.elapsed() / 60000);
		vortexTime.append(" minutes elapsed");
		emit log(Message(vortexTime));

				// Easiest Thresholding
		
		bool hasConvergingCenters = false;
		for(int l = 0; (l < vortexData->getNumLevels())
		      &&(hasConvergingCenters==false); l++) {
		  if(vortexData->getNumConvergingCenters(l)>0)
		    hasConvergingCenters = true;
		}

		emit log(Message(QString(),2,this->objectName())); // 97 %
		
		//if (SimplexON == 1) {

		QDomElement radar = configData->getConfig("radar");
		QString format = configData->getParam(radar,"format");
		
		if(format == "MODEL") { hasConvergingCenters = true; }

		  if(hasConvergingCenters) {
		    if (SimplexON == 0) {
		      if (radarVolume->getDateTime() > vortexList->last().getTime()) {
			vortexList->append(*vortexData);	
		      }
		    }
		    else {
		      vortexList->append(*vortexData);
		    }
		    vortexList->save();
		  
		
		    // Print out summary information to log
		    QString summary = "VORTRAC ATCF,";
		    QString values;
		    summary += vortexData->getTime().toString(Qt::ISODate) + ",";
		    summary += values.setNum(vortexData->getLat()) + ",";
		    summary += values.setNum(vortexData->getLon()) + ",";
		    summary += values.setNum(vortexData->getPressure()) + ",";
		    summary += values.setNum(vortexData->getPressureUncertainty()) + ",";
		    summary += values.setNum(vortexData->getRMW()) + ",";
		    summary += values.setNum(vortexData->getRMWUncertainty());
		    emit log(Message(summary,0,this->objectName()));
		  }
		
		  else {
		    emit log(Message(QString("Insufficient Convergence of Simplex Centers"),0,this->objectName(),AllOff,QString(),SimplexError,QString("Could Not Obtain Center Convergence")));

		  }
		  //}
		if(!hasConvergingCenters && (vortexList->count() > 0)){
		  emit log(Message(QString("Simplex Analysis Found No Converging Centers in this volume"),0,this->objectName(),Green, QString("No Center Found!"),OutOfRange, QString("No Converging Centers")));
		  analysisGood = false;
		}
		
		// Delete CAPPI and RadarData objects
		
		RadarData *temp = radarVolume;
		radarVolume = NULL;
		delete temp;
		
		delete gridData;
		delete vortexData;
		
		//Message::toScreen("Deleted vortex data.... ???");
		
		if(!analysisGood)
		{
			// Some error occurred, notify the user
			emit log(Message("Radar volume processing error!"));
			emit log(Message(
			   QString("Radar volume processing error!!"), 2,
			   this->objectName()));
			//Message::toScreen("AnalysisThread: Analysis BAD, Done Processing");
			emit(doneProcessing());
		       
			//return; // I got rid of this
			// do we need to fail if the 
			// volume is bad? -LM 1/20/07
		} else {
			// Store the resulting analysis in the vortex list
			archiveAnalysis();
			
			// Complete the progress bar and log that we're done
			emit log(Message(QString("Analysis complete!"),2,
					 this->objectName(), AllOff, 
					 QString(),Ok, QString()));

			// Let the poller know we're done
			//Message::toScreen("AnalysisThread: Analysis GOOD, Done Processing");
			emit(doneProcessing());
		}
		mutex.unlock();
		
		// Go to sleep, wait for more data
		if (!abort) {
		  mutex.lock();
		  // Wait until new data is available
		  waitForData.wait(&mutex);
		  //Message::toScreen("AnalysisThread: Received the Signal To Move Ahead with New Data");
		  mutex.unlock();
		}
		
		if(abort)
		  return;
		
       }
	emit log(Message("End of Analysis Thread Run"));
}



void AnalysisThread::archiveAnalysis()
{

}

void AnalysisThread::foundCenter()
{
	waitForCenter.wakeOne();
}

void AnalysisThread::foundWinds()
{
	waitForWinds.wakeOne();
}

void AnalysisThread::catchLog(const Message& message)
{
  emit log(message);
}

void AnalysisThread::setNumVolProcessed(const float& num)
{
  numVolProcessed = (int)num;
}

void AnalysisThread::setAnalyticRun(const bool& runOnce)
{
  analyticRun = runOnce;
}

void AnalysisThread::checkIntensification()
{
  // Checks for any rapid changes in pressure
  
  QDomElement pressure = configData->getConfig("pressure");

  // Units of mb / hr
  float rapidRate = configData->getParam(pressure, QString("rapidlimit")).toFloat();

  if(isnan(rapidRate)) {
    emit log(Message(QString("Could Not Find Rapid Intensification Rate, Using 3 mb/hr"),
		     0,this->objectName()));
    rapidRate = 3.0;
  }
  
  // So we don't report falsely there must be a rapid increase trend which 
  // spans several measurements

  // Number of volumes which are averaged.
  int volSpan = configData->getParam(pressure, QString("av_interval")).toInt();
  if(isnan(volSpan)) {
    emit log(Message(QString("Could Not Find Pressure Averaging Interval for Rapid Intensification, Using 8 volumes"),0,this->objectName()));
    volSpan = 8;
  }

  int lastVol = vortexList->count()-1;

  if(lastVol > 2*volSpan) {
    if(vortexList->at(int(volSpan/2.)).getTime().secsTo(vortexList->at(lastVol-int(volSpan/2.)).getTime()) > 3600) {
      float recentAv = 0;
      float pastAv = 0;
      int recentCount = 0;
      int pastCenter = 0;
      int pastCount = 0;
      for(int k = lastVol; k > lastVol-volSpan; k--) {
	if(vortexList->at(k).getPressure()==-999)
	  continue;
	recentAv+=vortexList->at(k).getPressure();
	recentCount++;
      }      
      recentAv /= (recentCount*1.);
      float timeSpan = vortexList->at(lastVol-volSpan).getTime().secsTo(vortexList->at(lastVol).getTime());
      QDateTime pastTime = vortexList->at(lastVol).getTime().addSecs(-1*int(timeSpan/2+3600));
      for(int k = 0; k < lastVol; k++) {
	if(vortexList->at(k).getPressure()==-999)
	  continue;
	if(vortexList->at(k).getTime() <= pastTime)
	  pastCenter = k;
      }      
      for(int j = pastCenter-int(volSpan/2.); 
	  (j < pastCenter+int(volSpan/2.))&&(j<lastVol); j++) {
	if(vortexList->at(j).getPressure()==-999)
	  continue;
	pastAv+= vortexList->at(j).getPressure();
	pastCount++;
      }
      pastAv /= (pastCount*1.);
      if(recentAv - pastAv > rapidRate) {
	emit(log(Message(QString("Rapid Increase in Storm Central Pressure Reported @ Rate of "+QString().setNum(recentAv-pastAv)+" mb/hour"), 0,this->objectName(), Green, QString(), RapidIncrease, QString("Storm Pressure Rising"))));
      } else {
	if(recentAv - pastAv < -1.0*rapidRate) {
	  emit(log(Message(QString("Rapid Decline in Storm Central Pressure Reporting @ Rate of "+QString().setNum(recentAv-pastAv)+" mb/hour"), 0, this->objectName(), Green, QString(), RapidDecrease, QString("Storm Pressure Falling"))));
	}
	else {
	  emit(log(Message(QString("Storm Central Pressure Stablized"), 0, this->objectName(),Green,QString(), Ok, QString())));
	}
      }
    }
  }
}
			 

void AnalysisThread::checkListConsistency()
{
  if(vortexList->count()!=simplexList->count()) {
    emit log(Message(QString("Storage Lists Reloaded With Mismatching Volume Entries"),0,this->objectName()));
  }
  
  for(int vv = vortexList->count()-1; vv >= 0; vv--) {
    bool foundMatch = false;
    for(int ss = 0; ss < simplexList->count(); ss++) {
      if(vortexList->at(vv).getTime()==simplexList->at(ss).getTime())
	foundMatch = true;
    }
    if(!foundMatch) {
      emit log(Message(QString("Removing Vortex Entry @ "+vortexList->at(vv).getTime().toString(Qt::ISODate)+" because no matching simplex was found"),0,this->objectName()));
      vortexList->removeAt(vv);
    }
  }

  for(int ss = simplexList->count()-1; ss >= 0; ss--) {
    bool foundMatch = false;
    for(int vv = 0; vv < vortexList->count(); vv++) {
      if(simplexList->at(ss).getTime() == vortexList->at(vv).getTime())
	foundMatch = true;
    }
    if(!foundMatch) {
      emit log(Message(QString("Removing Simplex Entry @ "+simplexList->at(ss).getTime().toString(Qt::ISODate)+" Because No Matching Vortex Was Found"),0,this->objectName()));
      simplexList->removeAt(ss);
    }
  } 
  // Removing the last ones for safety, any partially formed file could do serious damage
  // to data integrity
  simplexList->removeAt(simplexList->count()-1);
  simplexList->save();
  vortexList->removeAt(vortexList->count()-1);
  vortexList->save();
}
