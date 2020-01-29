/* SleepLib ZEO Loader Header
 *
 * Copyright (c) 2020 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef ZEOLOADER_H
#define ZEOLOADER_H

#include "SleepLib/machine_loader.h"

const QString zeo_class_name = "ZEO";
const int zeo_data_version = 1;


/*! \class ZEOLoader
    \brief Unfinished stub for loading ZEO Personal Sleep Coach data
*/
class ZEOLoader : public MachineLoader
{
  public:
    ZEOLoader();
    virtual ~ZEOLoader();

    virtual bool Detect(const QString &path) { Q_UNUSED(path); return false; }  // bypass autoscanner

    virtual int Open(const QString & path);
    virtual int OpenFile(const QString & filename);
    static void Register();

    virtual int Version() { return zeo_data_version; }
    virtual const QString &loaderName() { return zeo_class_name; }

    //Machine *CreateMachine();
    virtual MachineInfo newInfo() {
        return MachineInfo(MT_SLEEPSTAGE, 0, zeo_class_name, QObject::tr("Zeo"), QString(), QString(), QString(), QObject::tr("Personal Sleep Coach"), QDateTime::currentDateTime(), zeo_data_version);
    }

    bool openCSV(const QString & filename);
    Session* readNextSession();

  protected:
  private:
    QFile file;
    QTextStream text;
    Machine *mach;
    int idxZQ;
    int idxAwakenings;
    int idxSG;
    int idxDSG;
    int idxTimeInWake;
    int idxTimeToZ;
    int idxTimeInREM;
    int idxTimeInLight;
    int idxTimeInDeep;
    int idxStartOfNight;
    int idxEndOfNight;
    int idxRiseTime;
    int idxFirstAlaramRing;
    int idxLastAlaramRing;
    int idxFirstSnoozeTime;
    int idxLastSnoozeTime;
    int idxSetAlarmTime;
    int idxMorningFeel;
    int idxFirmwareVersion;
    int idxMyZEOVersion;
};

#endif // ZEOLOADER_H
