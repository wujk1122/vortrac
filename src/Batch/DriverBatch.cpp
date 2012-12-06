/*
 * DriverBatch.cpp
 * VORTRAC
 *
 * Created by Annette Foerster in 2012
 * Copyright 2005 University Corporation for Atmospheric Research.
 * All rights reserved.
 *
 *
 */

#include "DriverBatch.h"
#include <QtGui>
#include <QtNetwork>
#include <QThread>
#include "Message.h"
#include "thredds_Config.h"

DriverBatch::DriverBatch(QWidget *parent, const QString &fileName)
    : QWidget(parent)
{
    this->setObjectName("Batchmode Driver");
    xmlfile = fileName;
    statusLog = new Log();

    connect(this, SIGNAL(log(const Message&)),statusLog, SLOT(catchLog(const Message&)));

    // Create a new configuration instance
    configData = new Configuration;
    connect(configData, SIGNAL(log(const Message&)),this, SLOT(catchLog(const Message&)));

    configDialog = new ConfigurationDialog(this, configData);

    statusText = new QTextEdit;

    statusLog->catchLog(Message("VORTRAC Status Log for "+QDateTime::currentDateTime().toUTC().toString()+ " UTC"));

    pollThread = NULL;
    atcf = NULL;
    madis = NULL;
    fetchremote = NULL;

    deficitLabel = new QLabel(tr("Pressure Deficit From 0 km (mb):"));
    currDeficit = new QLabel(tr("0"));
    currPressure = new QLabel(tr("0"));
    currRMW = new QLabel(tr("0"));

}

DriverBatch::~DriverBatch()
{
    delete configData;
    delete statusLog;
    delete statusText;
    delete pollThread;
}

bool DriverBatch::initialize()
{
    // Parse the XML configuration file
    if (!parseXMLconfig()) return false;

    return true;
}

bool DriverBatch::run()
{
    QThread* thread = new QThread;
    workThread* pollThread = new workThread(this);
    pollThread->moveToThread(thread);

    connect(thread, SIGNAL(started()), pollThread, SLOT(run()));

    connect(pollThread, SIGNAL(finished()), thread, SLOT(quit()));
    connect(pollThread, SIGNAL(finished()), pollThread, SLOT(deleteLater()));

    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()),this->parentWidget(),SLOT(closeWindow()));

    connect(pollThread, SIGNAL(log(const Message&)),this, SLOT(catchLog(const Message&)));
    connect(pollThread, SIGNAL(vortexListUpdate(VortexList*)),this, SLOT(pollVortexUpdate(VortexList*)),Qt::DirectConnection);  //Necessary?

    atcf = new ATCF(configData);
    connect(atcf, SIGNAL(log(const Message&)),this, SLOT(catchLog(const Message&)));
    connect(atcf, SIGNAL(tcvitalsReady()),this, SLOT(updateTcvitals()));    //Necessary?
    pollThread->setATCF(atcf);

    madis = new MADISFactory(configData);
    connect(madis, SIGNAL(log(const Message&)),this, SLOT(catchLog(const Message&)));
    connect(this, SIGNAL(updateMadis(float, float)),madis, SLOT(setBoundingBox(float, float)));  //Necessary?

    fetchremote = new FetchRemote(configData);
    connect(fetchremote, SIGNAL(log(const Message&)),this, SLOT(catchLog(const Message&)));

    //set a flag in pollThread if continue previous run or not
    // Try to fetch new radar data every 5 minutes
    QString mode = configData->getParam(configData->getConfig("vortex"), "mode");
    if (mode == "operational") {
        QTimer::singleShot(0, fetchremote, SLOT(fetchRemoteData()));
        QTimer *fetchTimer = new QTimer(this);
        connect(fetchTimer, SIGNAL(timeout()), fetchremote, SLOT(fetchRemoteData()));
        fetchTimer->start(300000);

        QTimer::singleShot(0, atcf, SLOT(getTcvitals()));
        QTimer *atcfTimer = new QTimer(this);
        connect(atcfTimer, SIGNAL(timeout()), atcf, SLOT(getTcvitals()));
        atcfTimer->start(3600000);

        QTimer::singleShot(0, madis, SLOT(getPressureObs()));
        QTimer *madisTimer = new QTimer(this);
        connect(madisTimer, SIGNAL(timeout()), madis, SLOT(getPressureObs()));
        madisTimer->start(1800000);

    } else {
//        pollThread->setContinuePreviousRun(continuePreviousRun);
        pollThread->setConfig(configData);
        thread->start();

    }
    return true;
}

bool DriverBatch::finalize()
{
    std::cout << "Finalizing ...\n";
    return true;
}

bool DriverBatch::parseXMLconfig()
{
    this->loadFile(xmlfile);
    return true;
}

bool DriverBatch::validateXMLconfig()
{   //Not neccessary. Validation is already done within Configuration::read!
    return true;
}

void DriverBatch::saveLog()
{
    statusLog->saveLogFile();
}

void DriverBatch::catchLog(const Message& message)
{
    emit log(message);
}

bool DriverBatch::loadFile(const QString &fileName)
{
    //This is AnalysisPage::loadFile, but without configDialog->read();

    std::cout << "Loading xml file ...\n";
    // Load up a configuration file in the ConfigurationDialog
    if (!configData->read(fileName)) {
        emit log(Message(QString("Couldn't load configuration file"),0,this->objectName()));
        return false;
    }

    // Set the filename
    configFileName = fileName;
    QString directoryString(configData->getParam(configData->getConfig("vortex"),
                                                 "dir"));
    workingDirectory = QDir(directoryString);
    //Message::toScreen(workingDirectory.path());
    if(!workingDirectory.isAbsolute()) {
        workingDirectory.makeAbsolute();
    }
    // Check to make sure the workingDirectory exists
    if(!workingDirectory.exists())
        if(!workingDirectory.mkpath(directoryString)) {
            emit log(Message(QString("Failed to find or create working directory path: "+directoryString),0,this->objectName()));
            return false;
        }
    //Message::toScreen("2"+workingDirectory.path());
    statusLog->setWorkingDirectory(workingDirectory);
    //graph->setWorkingDirectory(workingDirectory);
    isUntitled = false;
    return true;
}

void DriverBatch::pollVortexUpdate(VortexList* list)
{
//    This is AnalysisPage::pollVortexUpdate(VortexList* list)


    if (list==NULL) {
        currPressure->setText(QString().setNum(0));
        currRMW->setText(QString().setNum(0));
        currDeficit->setText(QString().setNum(0));
        deficitLabel->setText(tr("Pressure Deficit Unavailable"));
        emit vortexListChanged(NULL);
    return;
    }
    //Message::toScreen("Made it too pollVortexUpdate in AnalysisPage");
    if(list->count() > 0) {

        // Find the outermost vtd mean wind coefficient that is not equal to -999
        float maxRadius = list->last().getMaxValidRadius();
        currPressure->setText(QString().setNum((int)list->last().getPressure()));

        if(list->last().getAveRMW()==-999.0)
            currRMW->setText(QString().setNum(0));
        else
            currRMW->setText(QString().setNum(list->last().getAveRMW()));

        currDeficit->setText(QString().setNum(list->last().getPressureDeficit(), 'f', 1));
        deficitLabel->setText(tr("Pressure Deficit From ")+QString().setNum(maxRadius)+tr(" km (mb):"));
        emit vortexListChanged(list);
    }
    else {
        currPressure->setText(QString().setNum(0));
        currRMW->setText(QString().setNum(0));
        currDeficit->setText(QString().setNum(0));
        deficitLabel->setText(tr("Pressure Deficit Unavailable"));
        emit vortexListChanged(NULL);
    }
}

void DriverBatch::updateTcvitals()
{
//    This is: AnalysisPage::updateTcvitals()

    // Get info from ATCF
    QDomElement vortex = configData->getConfig("vortex");
    QString vortexName = atcf->getStormName();
    configData->setParam(vortex, "name", vortexName);
    QString lat = QString().setNum(atcf->getLatitude(atcf->getTime()));
    configData->setParam(vortex, "lat", lat);
    QString lon = QString().setNum(atcf->getLongitude(atcf->getTime()));
    configData->setParam(vortex, "lon", lon);
    QString dir = QString().setNum(atcf->getDirection());
    configData->setParam(vortex, "direction", dir);
    QString speed = QString().setNum(atcf->getSpeed());
    configData->setParam(vortex, "speed", speed);
    QString rmw = QString().setNum(atcf->getRMW());
    configData->setParam(vortex, "rmw", rmw);
    QString date = atcf->getTime().toString("yyyy-MM-dd");
    configData->setParam(vortex, "obsdate", date);
    QString time = atcf->getTime().toString("hh:mm:ss");
    configData->setParam(vortex, "obstime", time);

    QDomElement radar = configData->getConfig("radar");
    configData->setParam(radar, "startdate", date);
    configData->setParam(radar, "starttime", time);

    QDomElement choosecenter = configData->getConfig("choosecenter");
    configData->setParam(choosecenter, "startdate", date);
    configData->setParam(choosecenter, "starttime", time);

    configDialog->checkConfig();
    if(!configDialog->checkPanels()) {
        QMessageBox errCfg;
        errCfg.setText("Please check errors: Incorrect entry in the configuration");
        errCfg.exec();
        return;
    }
//    if (!thread->isRunning()) {
//        pollThread->setContinuePreviousRun(false);
//        pollThread->setConfig(configData);
//        thread->start();
//    }
}
