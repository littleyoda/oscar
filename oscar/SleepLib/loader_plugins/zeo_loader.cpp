/* SleepLib ZEO Loader Implementation
 *
 * Copyright (c) 2020 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the zeo_data_version in zel_loader.h when making changes to this loader
// that change loader behaviour or modify channels.
//********************************************************************************************

#include <QDir>
#include <QTextStream>
#include "zeo_loader.h"
#include "SleepLib/machine.h"
#include "csv.h"

ZEOLoader::ZEOLoader()
{
    m_type = MT_SLEEPSTAGE;
}

ZEOLoader::~ZEOLoader()
{
    closeCSV();
}

int ZEOLoader::Open(const QString & dirpath)
{
    QString newpath;

    QString dirtag = "zeo";

    // Could Scan the ZEO folder for a list of CSVs

    QString path(dirpath);
    path = path.replace("\\", "/");

    if (path.toLower().endsWith("/" + dirtag)) {
        return 0;
        //newpath=path;
    } else {
        newpath = path + "/" + dirtag.toUpper();
    }

    //QString filename;

    // ZEO folder structure detection stuff here.

    return 0; // number of machines affected
}

/*15233: "Sleep Date"
15234: "ZQ"
15236: "Total Z"
15237: "Time to Z"
15237: "Time in Wake"
15238: "Time in REM"
15238: "Time in Light"
15241: "Time in Deep"
15242: "Awakenings"
15245: "Start of Night"
15246: "End of Night"
15246: "Rise Time"
15247: "Alarm Reason"
15247: "Snooze Time"
15254: "Wake Tone"
15259: "Wake Window"
15259: "Alarm Type"
15260: "First Alarm Ring"
15261: "Last Alarm Ring"
15261: "First Snooze Time"
15265: "Last Snooze Time"
15266: "Set Alarm Time"
15266: "Morning Feel"
15267: "Sleep Graph"
15267: "Detailed Sleep Graph"
15268: "Firmware Version"  */

int ZEOLoader::OpenFile(const QString & filename)
{
    if (!openCSV(filename)) {
        return -1;
    }
    int count = 0;
    Session* sess;
    while ((sess = readNextSession()) != nullptr) {
        sess->SetChanged(true);
        mach->AddSession(sess);
        count++;
    }
    mach->Save();
    closeCSV();
    return count;
}

bool ZEOLoader::openCSV(const QString & filename)
{
    file.setFileName(filename);

    if (filename.toLower().endsWith(".csv")) {
        if (!file.open(QFile::ReadOnly)) {
            qDebug() << "Couldn't open zeo file" << filename;
            return false;
        }
    } else {// if (filename.toLower().endsWith(".dat")) {

        return false;
        // not supported.
    }

    QStringList header;
    csv = new CSVReader(file);
    bool ok = csv->readRow(header);
    if (!ok) {
        qWarning() << "no header row";
        return false;
    }
    csv->setFieldNames(header);

    MachineInfo info = newInfo();
    mach = p_profile->CreateMachine(info);

    return true;
}

void ZEOLoader::closeCSV()
{
    if (csv != nullptr) {
        delete csv;
        csv = nullptr;
    }
    if (file.isOpen()) {
        file.close();
    }
}

//    int idxTotalZ = header.indexOf("Total Z");
//    int idxAlarmReason = header.indexOf("Alarm Reason");
//    int idxSnoozeTime = header.indexOf("Snooze Time");
//    int idxWakeTone = header.indexOf("Wake Tone");
//    int idxWakeWindow = header.indexOf("Wake Window");
//    int idxAlarmType = header.indexOf("Alarm Type");

Session* ZEOLoader::readNextSession()
{
    if (csv == nullptr) {
        qWarning() << "no CSV open!";
        return nullptr;
    }
    Session* sess = nullptr;
    QDateTime start_of_night, end_of_night, rise_time;
    SessionID sid;

    //const qint64 WindowSize=30000;
    qint64 st, tt;
    int stage;

    int ZQ, TimeToZ, TimeInWake, TimeInREM, TimeInLight, TimeInDeep, Awakenings;

    //int AlarmReason, SnoozeTime, WakeTone, WakeWindow, AlarmType, TotalZ;
    int MorningFeel;
    QString FirmwareVersion, MyZeoVersion;

    QDateTime FirstAlarmRing, LastAlarmRing, FirstSnoozeTime, LastSnoozeTime, SetAlarmTime;

    QStringList SG, DSG;

    bool ok;
    bool dodgy;

    QHash<QString,QString> row;
    while (csv->readRow(row)) {
        dodgy = false;

        ZQ = row["ZQ"].toInt(&ok);
        if (!ok) { dodgy = true; }

//        TotalZ = linecomp[idxTotalZ].toInt(&ok);
//        if (!ok) { dodgy = true; }

        TimeToZ = row["Time to Z"].toInt(&ok);
        if (!ok) { dodgy = true; }

        TimeInWake = row["Time in Wake"].toInt(&ok);
        if (!ok) { dodgy = true; }

        TimeInREM = row["Time in REM"].toInt(&ok);
        if (!ok) { dodgy = true; }

        TimeInLight = row["Time in Light"].toInt(&ok);
        if (!ok) { dodgy = true; }

        TimeInDeep = row["Time in Deep"].toInt(&ok);
        if (!ok) { dodgy = true; }

        Awakenings = row["Awakenings"].toInt(&ok);
        if (!ok) { dodgy = true; }

        start_of_night = readDateTime(row["Start of Night"]);
        if (!start_of_night.isValid()) { dodgy = true; }

        end_of_night = readDateTime(row["End of Night"]);
        if (!end_of_night.isValid()) { dodgy = true; }

        rise_time = readDateTime(row["Rise Time"]);
        if (!rise_time.isValid()) { dodgy = true; }

//        AlarmReason = linecomp[idxAlarmReason].toInt(&ok);
//        if (!ok) { dodgy = true; }

//        SnoozeTime = linecomp[idxSnoozeTime].toInt(&ok);
//        if (!ok) { dodgy = true; }

//        WakeTone = linecomp[idxWakeTone].toInt(&ok);
//        if (!ok) { dodgy = true; }

//        WakeWindow = linecomp[idxWakeWindow].toInt(&ok);
//        if (!ok) { dodgy = true; }

//        AlarmType = linecomp[idxAlarmType].toInt(&ok);
//        if (!ok) { dodgy = true; }

        if (!row["First Alarm Ring"].isEmpty()) {
            FirstAlarmRing = readDateTime(row["First Alarm Ring"]);
            if (!FirstAlarmRing.isValid()) { dodgy = true; }
        }

        if (!row["Last Alarm Ring"].isEmpty()) {
            LastAlarmRing = readDateTime(row["Last Alarm Ring"]);
            if (!LastAlarmRing.isValid()) { dodgy = true; }
        }

        if (!row["First Snooze Time"].isEmpty()) {
            FirstSnoozeTime = readDateTime(row["First Snooze Time"]);

            if (!FirstSnoozeTime.isValid()) { dodgy = true; }
        }

        if (!row["Last Snooze Time"].isEmpty()) {
            LastSnoozeTime = readDateTime(row["Last Snooze Time"]);
            if (!LastSnoozeTime.isValid()) { dodgy = true; }
        }

        if (!row["Set Alarm Time"].isEmpty()) {
            SetAlarmTime = readDateTime(row["Set Alarm Time"]);
            if (!SetAlarmTime.isValid()) { dodgy = true; }
        }

        MorningFeel = row["Morning Feel"].toInt(&ok);
        if (!ok) { MorningFeel = 0; }

        FirmwareVersion = row["Firmware Version"];

        MyZeoVersion = row["My ZEO Version"];

        if (dodgy) {
            continue;
        }

        SG = row["Sleep Graph"].split(" ");
        DSG = row["Detailed Sleep Graph"].split(" ");

        sid = start_of_night.toTime_t();

        if (DSG.size() == 0) {
            continue;
        }

        if (mach->SessionExists(sid)) {
            continue;
        }

        sess = new Session(mach, sid);
        break;
    };

    if (sess) {
        const int WindowSize = 30 * 1000;

        sess->settings[ZEO_Awakenings] = Awakenings;
        sess->settings[ZEO_MorningFeel] = MorningFeel;
        sess->settings[ZEO_TimeToZ] = TimeToZ;
        sess->settings[ZEO_ZQ] = ZQ;
        sess->settings[ZEO_TimeInWake] = TimeInWake;
        sess->settings[ZEO_TimeInREM] = TimeInREM;
        sess->settings[ZEO_TimeInLight] = TimeInLight;
        sess->settings[ZEO_TimeInDeep] = TimeInDeep;

        st = qint64(start_of_night.toTime_t()) * 1000L;
        sess->really_set_first(st);
        tt = st;
        EventList *sleepstage = sess->AddEventList(ZEO_SleepStage, EVL_Event, 1, 0, 0, 4);

        for (int i = 0; i < DSG.size(); i++) {
            stage = DSG[i].toInt(&ok);
            if (ok) {
                sleepstage->AddEvent(tt, stage);
            }
            tt += WindowSize;
        }

        sess->really_set_last(tt);
        //int size = DSG.size();
        //qDebug() << linecomp[0] << start_of_night << end_of_night << rise_time << size << "30 second chunks";
    }

    return sess;
}

QDateTime ZEOLoader::readDateTime(const QString & text)
{
    QDateTime dt = QDateTime::fromString(text, "MM/dd/yyyy HH:mm");
    if (!dt.isValid()) {
        dt = QDateTime::fromString(text, "yyyy-MM-dd HH:mm:ss");
    }
    return dt;
}

static bool zeo_initialized = false;

void ZEOLoader::Register()
{
    if (zeo_initialized) { return; }

    qDebug("Registering ZEOLoader");
    RegisterLoader(new ZEOLoader());
    //InitModelMap();
    zeo_initialized = true;
}

