/* SleepLib CMS50X Loader Header
 *
 * Copyright (c) 2011-2014 Mark Watkins <jedimark@users.sourceforge.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

#ifndef CMS50F37LOADER_H
#define CMS50F37LOADER_H

#include "SleepLib/serialoximeter.h"

const QString cms50f37_class_name = "CMS50F37";
const int cms50f37_data_version = 0;


/*! \class CMS5037Loader
    \brief Bulk Importer for newer CMS50 oximeters
    */
class CMS50F37Loader : public SerialOximeter
{
Q_OBJECT
  public:


    CMS50F37Loader();
    virtual ~CMS50F37Loader();

    virtual bool Detect(const QString &path);
    virtual int Open(QString path);
    virtual bool openDevice();


    static void Register();

    virtual int Version() { return cms50f37_data_version; }
    virtual const QString &loaderName() { return cms50f37_class_name; }

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_OXIMETER, 0, cms50f37_class_name, QObject::tr("Contec"), QObject::tr("CMS50F3.7"), QString(), QString(), QObject::tr("CMS50F"), QDateTime::currentDateTime(), cms50f37_data_version);
    }


  //  Machine *CreateMachine();

    virtual void process();

    virtual bool isStartTimeValid() { return !cms50dplus; }

protected slots:
//    virtual void dataAvailable();
    virtual void resetImportTimeout();
    virtual void startImportTimeout();
    virtual void shutdownPorts();

    void nextCommand();


protected:

    bool readSpoRFile(QString path);
    virtual void processBytes(QByteArray bytes);

    int doImportMode();
    int doLiveMode();

    virtual void killTimers();

    void sendCommand(unsigned char c);
    QList<unsigned char> cmdQue;


    // Switch device to live streaming mode
    virtual void resetDevice();

    // Switch device to record transmission mode
    void requestData();


  private:
    int sequence;

    EventList *PULSE;
    EventList *SPO2;

    QTime m_time;

    QByteArray buffer;

    bool started_import;
    bool finished_import;
    bool started_reading;
    bool cms50dplus;

    int cb_reset,imp_callbacks;

    int received_bytes;

    int m_itemCnt;
    int m_itemTotal;


};


#endif // CMS50F37LOADER_H