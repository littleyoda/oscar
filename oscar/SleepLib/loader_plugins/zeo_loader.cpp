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
    // TODO: add progress bar support, perhaps move shared logic into shared parent class with Dreem loader
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

    qint64 st, tt;
    int stage;

    int ZQ, TimeToZ, TimeInWake, TimeInREM, TimeInLight, TimeInDeep, Awakenings;
    int MorningFeel;
    QString FirmwareVersion, MyZeoVersion;
    QDateTime FirstAlarmRing, LastAlarmRing, FirstSnoozeTime, LastSnoozeTime, SetAlarmTime;
    QStringList SG, DSG;

    QHash<QString,QString> row;
    while (csv->readRow(row)) {
        invalid_fields = false;

        start_of_night = readDateTime(row["Start of Night"]);
        if (start_of_night.isValid()) {
            sid = start_of_night.toTime_t();
            if (mach->SessionExists(sid)) {
                continue;
            }
        }

        ZQ = readInt(row["ZQ"]);
        TimeToZ = readInt(row["Time to Z"]);
        TimeInWake = readInt(row["Time in Wake"]);
        TimeInREM = readInt(row["Time in REM"]);
        TimeInLight = readInt(row["Time in Light"]);
        TimeInDeep = readInt(row["Time in Deep"]);
        Awakenings = readInt(row["Awakenings"]);
        end_of_night = readDateTime(row["End of Night"]);
        rise_time = readDateTime(row["Rise Time"]);
        FirstAlarmRing = readDateTime(row["First Alarm Ring"], false);
        LastAlarmRing = readDateTime(row["Last Alarm Ring"], false);
        FirstSnoozeTime = readDateTime(row["First Snooze Time"], false);
        LastSnoozeTime = readDateTime(row["Last Snooze Time"], false);
        SetAlarmTime = readDateTime(row["Set Alarm Time"], false);
        MorningFeel = readInt(row["Morning Feel"], false);
        FirmwareVersion = row["Firmware Version"];
        MyZeoVersion = row["My ZEO Version"];

        if (invalid_fields) {
            continue;
        }

        SG = row["Sleep Graph"].split(" ");
        DSG = row["Detailed Sleep Graph"].split(" ");

        if (DSG.size() == 0) {
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
        EventList *sleepstage = sess->AddEventList(ZEO_SleepStage, EVL_Event, 1, 0, -4, 0);

        for (int i = 0; i < DSG.size(); i++) {
            bool ok;
            stage = DSG[i].toInt(&ok);
            if (ok) {
                // 1 = Awake, 2 = REM, 3 = Light Sleep, 4 = Deep Sleep
                // TODO: What is 0? What is 6?
                sleepstage->AddEvent(tt, -stage);  // use negative values so that the chart is oriented the right way
            }
            tt += WindowSize;
        }

        sess->really_set_last(tt);
        //int size = DSG.size();
        //qDebug() << linecomp[0] << start_of_night << end_of_night << rise_time << size << "30 second chunks";
    }

    return sess;
}

QDateTime ZEOLoader::readDateTime(const QString & text, bool required)
{
    QDateTime dt = QDateTime::fromString(text, "MM/dd/yyyy HH:mm");
    if (required || !text.isEmpty()) {
        if (!dt.isValid()) {
            dt = QDateTime::fromString(text, "yyyy-MM-dd HH:mm:ss");
            if (!dt.isValid()) {
                invalid_fields = true;
            }
        }
    }
    return dt;
}

int ZEOLoader::readInt(const QString & text, bool required)
{
    bool ok;
    int value = text.toInt(&ok);
    if (!ok) {
        if (required) {
            invalid_fields = true;
        } else {
            value = 0;
        }
    }
    return value;
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

