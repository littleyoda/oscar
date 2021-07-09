/* SleepLib Fisher & Paykel SleepStyle Loader Implementation
 *
 * Copyright (c) 2020 The Oscar Team
 *
 * Derived from icon_loader.cpp
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDir>
#include <QMessageBox>
#include <QDataStream>
#include <QTextStream>
#include <QCoreApplication>
#include <cmath>

#include "sleepstyle_loader.h"
#include "sleepstyle_EDFinfo.h"

const QString FPHCARE = "FPHCARE";

SleepStyle::SleepStyle(Profile *profile, MachineID id)
    : CPAP(profile, id)
{
}

SleepStyle::~SleepStyle()
{
}

SleepStyleLoader::SleepStyleLoader()
{
    m_buffer = nullptr;
    m_type = MT_CPAP;
}

SleepStyleLoader::~SleepStyleLoader()
{
}

/*
 * getIconDir - returns the path to the ICON directory
 */
QString getIconDir (QString givenpath) {

    QString path = givenpath;

    path = path.replace("\\", "/");

    if (path.endsWith("/")) {
        path.chop(1);
    }

    if (path.endsWith("/" + FPHCARE)) {
        path = path.section("/",0,-2);
    }

    QDir dir(path);

    if (!dir.exists()) {
        return "";
    }

    // If this is a backup directory, higher level directories have been
    // omitted.
    if (path.endsWith("/Backup/", Qt::CaseInsensitive))
        return path;

    // F&P Icon have a folder called FPHCARE in the root directory
    if (!dir.exists(FPHCARE)) {
        return "";
    }

    // CHECKME: I can't access F&P ICON data right now
    if (!dir.exists("FPHCARE/ICON")) {
        return "";
    }

    return dir.filePath("FPHCARE/ICON");
}

/*
 * getSleepStyleMachines returns a list of all SleepStyle machine folders in the ICON directory
 */
QStringList getSleepStyleMachines (QString iconPath) {
    QStringList ssMachines;

    QDir iconDir (iconPath);

    // SleepStyle are mixed alpha and numeric; ICON serial numbers (directory names) are all digits
    iconDir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    iconDir.setSorting(QDir::Name);

    QFileInfoList flist = iconDir.entryInfoList();   // List of Icon subdirectories

    bool isIconFilename;

    // Walk though directory list and save those that appear to be for SleepStyle machins.
    for (int i = 0; i < flist.size(); i++) {
        QFileInfo fi = flist.at(i);
        QString filename = fi.fileName();
        filename.toInt(&isIconFilename);
        if (isIconFilename) // Ignore this directory if named as used for older F&P Icon machine
            continue;
        if (filename.length() < 8)  // F&P machine names are 8 characters long, but we allow more just in case...
            continue;

        // directory is serial number and must not be all digits (which would make it an ICON directory)
        // and it must have *.FPH files within it to be a SleepStyle folder

        QDir machineDir (iconPath + "/" + filename);
        machineDir.setFilter(QDir::NoDotAndDotDot | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
        machineDir.setSorting(QDir::Name);
        QStringList filters;
        filters << "*.fph";
        machineDir.setNameFilters(filters);
        QFileInfoList flist = machineDir.entryInfoList();
        if (flist.size() <= 0) {
            continue;
        }
        ssMachines.push_back(filename);
    }

    return ssMachines;
}

bool SleepStyleLoader::Detect(const QString & givenpath)
{
    QString iconPath = getIconDir(givenpath);
    if (iconPath.isEmpty())
        return false;

    QStringList machines = getSleepStyleMachines(iconPath);
    if (machines.length() <= 0)
        // Did not find any SleepStyle machine directories
        return false;

    return true;
}

bool SleepStyleLoader::backupData (Machine * mach, const QString & path) {

    QDir ipath(path);
    QDir bpath(mach->getBackupPath());

    // Compare QDirs rather than QStrings because separators may be different, especially on Windows.

    if (ipath == bpath) {
        // Don't create backups if importing from backup folder
        rebuild_from_backups = true;
        create_backups = false;
    } else {
        rebuild_from_backups = false;
        create_backups = p_profile->session->backupCardData();
    }

    if (rebuild_from_backups || !create_backups)
        return true;

    // Copy input data to backup location
    copyPath(ipath.absolutePath(), bpath.absolutePath());

    return true;
}


int SleepStyleLoader::Open(const QString & path)
{
    QString iconPath = getIconDir(path);
    if (iconPath.isEmpty())
        return false;

    QStringList serialNumbers = getSleepStyleMachines(iconPath);
    if (serialNumbers.length() <= 0)
        // Did not find any SleepStyle machine directories
        return false;

    Machine *m;

    int c = 0;
    for (int i = 0; i < serialNumbers.size(); i++) {
        MachineInfo info = newInfo();
        info.serial = serialNumbers[i];
        m = p_profile->CreateMachine(info);

        setSerialPath(iconPath + "/" + info.serial);

        try {
            if (m) {
                c+=OpenMachine(m, path, serialPath);
            }
        } catch (OneTypePerDay& e) {
            Q_UNUSED(e)
            p_profile->DelMachine(m);
            MachList.erase(MachList.find(info.serial));
            QMessageBox::warning(nullptr, tr("Import Error"),
                                 tr("This Machine Record cannot be imported in this profile.")+"\n\n"+tr("The Day records overlap with already existing content."),
                                 QMessageBox::Ok);
            delete m;
        }
    }

    return c;
}

int SleepStyleLoader::OpenMachine(Machine *mach, const QString & path, const QString & ssPath)
{
    emit updateMessage(QObject::tr("Getting Ready..."));
    emit setProgressValue(0);
    QCoreApplication::processEvents();

    QDir dir(ssPath);

    if (!dir.exists() || (!dir.isReadable())) {
        return -1;
    }

    backupData(mach, path);

    qDebug() << "Opening F&P SleepStyle" << ssPath;

    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    QString filename, fpath;

    emit updateMessage(QObject::tr("Reading data files..."));
    QCoreApplication::processEvents();

    QStringList summary, det, his;
    Sessions.clear();

    for (int i = 0; i < flist.size(); i++) {
        QFileInfo fi = flist.at(i);
        filename = fi.fileName();
        fpath = ssPath + "/" + filename;

        if (filename.left(3).toUpper() == "SUM") {
            summary.push_back(fpath);
            OpenSummary(mach, fpath);
        } else if (filename.left(3).toUpper() == "DET") {
            det.push_back(fpath);
        } else if (filename.left(3).toUpper() == "HIS") {
            his.push_back(fpath);
        }
    }

    for (int i = 0; i < det.size(); i++) {
        OpenDetail(mach, det[i]);
    }

// Process REALTIME files
    dir.cd("REALTIME");
    QFileInfoList rtlist = dir.entryInfoList();
    for (int i = 0; i < rtlist.size(); i++) {
        QFileInfo fi = rtlist.at(i);
        filename = fi.fileName();
        fpath = ssPath + "/REALTIME/" + filename;
        if (filename.left(3).toUpper() == "HRD"
            && filename.right(3).toUpper() == "EDF"   ) {
            OpenRealTime (mach, filename, fpath);
        }
    }

// LOG files were not processed by icon_loader
// So we don't need to do anything

    SessionID sid;//,st;
    float hours, mins;

    // For diagnostics, print summary of last 20 session or one week
    qDebug() << "SS Loader - last 20 Sessions:";

    int cnt = 0;
    QDateTime dt;
    QString a = "";

    if (Sessions.size() > 0) {

        QMap<SessionID, Session *>::iterator it = Sessions.end();
        it--;

        dt = QDateTime::fromTime_t(qint64(it.value()->first()) / 1000L);
        QDate date = dt.date().addDays(-7);
        it++;

        do {
            it--;
            Session *sess = it.value();
            sid = sess->session();
            hours = sess->hours();
            mins = hours * 60;
            dt = QDateTime::fromTime_t(sid);

            qDebug() << cnt << ":" << dt << "session" << sid << "," << mins << "minutes" << a;

            if (dt.date() < date) {
                break;
            }

            ++cnt;

        } while (it != Sessions.begin());

    }

        //    qDebug() << "Unmatched Sessions";
        //    QList<FPWaveChunk> chunks;
        //    for (QMap<int,QDate>::iterator dit=FLWDate.begin();dit!=FLWDate.end();dit++) {
        //        int k=dit.key();
        //        //QDate date=dit.value();
        ////        QList<Session *> values = SessDate.values(date);
        //        for (int j=0;j<FLWTS[k].size();j++) {

        //            FPWaveChunk chunk(FLWTS[k].at(j),FLWDuration[k].at(j),k);
        //            chunk.flow=FLWMapFlow[k].at(j);
        //            chunk.leak=FLWMapLeak[k].at(j);
        //            chunk.pressure=FLWMapPres[k].at(j);

        //            chunks.push_back(chunk);

        //            zz=FLWTS[k].at(j)/1000;
        //            dur=double(FLWDuration[k].at(j))/60000.0;
        //            bool b,c=false;
        //            if (Sessions.contains(zz)) b=true; else b=false;
        //            if (b) {
        //                if (Sessions[zz]->channelDataExists(CPAP_FlowRate)) c=true;
        //            }
        //            qDebug() << k << "-" <<j << ":" << zz << qRound(dur) << "minutes" << (b ? "*" : "") << (c ? QDateTime::fromTime_t(zz).toString() : "");
        //        }
        //    }
        //    std::sort(chunks.begin(), chunks.end());
        //    bool b,c;
        //    for (int i=0;i<chunks.size();i++) {
        //        const FPWaveChunk & chunk=chunks.at(i);
        //        zz=chunk.st/1000;
        //        dur=double(chunk.duration)/60000.0;
        //        if (Sessions.contains(zz)) b=true; else b=false;
        //        if (b) {
        //            if (Sessions[zz]->channelDataExists(CPAP_FlowRate)) c=true;
        //        }
        //        qDebug() << chunk.file << ":" << i << zz << dur << "minutes" << (b ? "*" : "") << (c ? QDateTime::fromTime_t(zz).toString() : "");
        //    }

    int c = Sessions.size();
    qDebug() << "SS Loader found" << c << "sessions";

    emit updateMessage(QObject::tr("Finishing up..."));
    QCoreApplication::processEvents();

    finishAddingSessions();

    mach->Save();


    return c;
}

// !\brief Convert F&P 32bit date format to 32bit UNIX Timestamp
quint32 ssconvertDate(quint32 timestamp)
{
    quint16 day, month,hour=0, minute=0, second=0;
    quint16 year;


    day = timestamp & 0x1f;
    month = (timestamp  >> 5) & 0x0f;
    year = 2000 + ((timestamp  >> 9) & 0x3f);
    quint32 ts2 = timestamp >> 15;
    second = ts2 & 0x3f;
    minute = (ts2 >> 6) & 0x3f;
    hour = (ts2 >> 12);

    QDateTime dt = QDateTime(QDate(year, month, day), QTime(hour, minute, second), Qt::UTC);

    qDebug().noquote() << "SS timestamp" << timestamp << year << month << day << dt << hour << minute << second;

//    Q NO!!! _ASSERT(dt.isValid());
//    if ((year == 2013) && (month == 9) && (day == 18)) {
//        // this is for testing.. set a breakpoint on here and
//        int i=5;
//    }


    // From Rudd's data set compared to times reported from his F&P software's report (just the time bits left over)
    // 90514 = 00:06:18 WET 23:06:18 UTC 09:06:18 AEST
    // 94360 = 01:02:24 WET
    // 91596 = 00:23:12 WET
    // 19790 = 23:23:50 WET

    return dt.addSecs(-54).toTime_t();  // Huh?  Why do this?
}

// SessionID is in seconds, not msec
SessionID SleepStyleLoader::findSession (SessionID sid) {
    for(auto sessKey : Sessions.keys())
    {
        Session * sess = Sessions.value(sessKey);
        if (sid >= (sess->realFirst() / 1000L) && sid <= (sess->realLast() / 1000L))
            return sessKey;
    }

    return 0;
}

bool SleepStyleLoader::OpenRealTime(Machine *mach, const QString & fname, const QString & filepath)
{
//    Q_UNUSED(filepath)
    Q_UNUSED(mach)
    Q_UNUSED(fname)

    SleepStyleEDFInfo edf;

    // Open the EDF file and read contents into edf object
    if (!edf.Open(filepath)) {
        qWarning() << "SS Realtime failed to open" << filepath;
        return false;
    }

    if (!edf.Parse()) {
        qWarning() << "SS Realtime Parse failed to open" << filepath;
        return false;
    }

    qDebug().noquote() << "SS ORT timestamp" << edf.startdate / 1000L << QDateTime::fromSecsSinceEpoch(edf.startdate / 1000L).toString("MM/dd/yyyy hh:mm:ss");
    SessionID sessKey = findSession(edf.startdate / 1000L);
    if (sessKey == 0)  {
        qWarning() << "SS ORT session not found";
        return true;
    }

    Session * sess = Sessions.value(sessKey);

    if (sess == nullptr) {
        qWarning() << "SS ORT session not found - nullptr";
        return true;
    }

    sess->updateFirst(edf.startdate);

    qint64 duration = edf.GetNumDataRecords() * edf.GetDurationMillis();
    sess->updateLast(edf.startdate + duration);

// Find the leak signal and data
    long leakrecs = 0;
    EDFSignal leakSignal;
    for (auto & esleak : edf.edfsignals) {
        leakrecs = esleak.sampleCnt * edf.GetNumDataRecords();
        if (leakrecs < 0)
            continue;
        if (esleak.label == "Leak") {
            leakSignal = esleak;
        }
    }

// Walk through all signals, ignoring leaks
    for (auto & es : edf.edfsignals) {
        long recs = es.sampleCnt * edf.GetNumDataRecords();
        if (recs < 0)
            continue;
        ChannelID code = 0;

        if (es.label == "Flow") {
            // Flow data appears to include total leaks, which are also reported in the edf file.
            // We subtract the leak from the flow data to get flow data that is centered around zero.
            // This is needed for other derived graphs (tidal volume, insp and exp times, etc.) to be reasonable
            code = CPAP_FlowRate;
            bool done = false;
            if (leakrecs > 0) {
                for (int ileak = 0; ileak < leakrecs && !done; ileak++) {
                    for (int iflow = 0; iflow < 25 && !done; iflow++) {
                        if (ileak*25 + iflow >= recs) {
                            done = true;
                            break;
                        }
                        es.dataArray[ileak*25 + iflow] -= leakSignal.dataArray[ileak] - 500;
                    }
                }
            }

        } else if (es.label == "Pressure") {
            code = CPAP_MaskPressure;

        } else
            continue;

        if (code) {
            double rate = double(duration) / double(recs);
            EventList *a = sess->AddEventList(code, EVL_Waveform, es.gain, es.offset, 0, 0, rate);
            a->setDimension(es.physical_dimension);
            a->AddWaveform(edf.startdate, es.dataArray, recs, duration);

            EventDataType min = a->Min();
            EventDataType max = a->Max();

            // Cap to physical dimensions, because there can be ram glitches/whatever that throw really big outliers.
            if (min < es.physical_minimum)
                min = es.physical_minimum;
            if (max > es.physical_maximum)
                max = es.physical_maximum;

            sess->updateMin(code, min);
            sess->updateMax(code, max);
            sess->setPhysMin(code, es.physical_minimum);
            sess->setPhysMax(code, es.physical_maximum);
        }

    }

    return true;

}

////////////////////////////////////////////////////////////////////////////////////////////
// Open Summary file, create list of sessions and session summary data
////////////////////////////////////////////////////////////////////////////////////////////
bool SleepStyleLoader::OpenSummary(Machine *mach, const QString & filename)
{
    qDebug() << filename;
    QByteArray header;
    QFile file(filename);

    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "SS SUM Couldn't open" << filename;
        return false;
    }

    // Read header of summary file
    header = file.read(0x200);

    if (header.size() != 0x200) {
        qDebug() << "SS SUM Short file" << filename;
        file.close();
        return false;
    }

    // Header is terminated by ';' at 0x1ff
    unsigned char hterm = 0x3b;

    if (hterm != header[0x1ff]) {
        qWarning() << "SS SUM Header missing ';' terminator" << filename;
    }

    QTextStream htxt(&header);
    QString h1, version, fname, serial, model, type, unknownident;
    htxt >> h1;
    htxt >> version;
    htxt >> fname;
    htxt >> serial;
    htxt >> model; //TODO: Should become Series in machine info???
    htxt >> type;  // SPSAAN etc with 4th character being A (Auto) or C (CPAP)
    htxt >> unknownident; // Constant, but has different value when version number is different.

    qDebug() << "SS SUM header" << h1 << version << fname << serial << model << type << unknownident;

    if (type.length() > 4)
        type = (type.at(3) == 'C' ? "CPAP" : "Auto");
    mach->setModel(model + " " + type);

    // Read remainder of summary file
    QByteArray data;
    data = file.readAll();
    file.close();

    QDataStream in(data);
    in.setVersion(QDataStream::Qt_4_8);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 ts;
    //QByteArray line;
    unsigned char p1, p2, p3, j1, x1, x2;

    unsigned char runTime, useTime, minPressSet, maxPressSet, minPressSeen, pct95PressSeen, maxPressSeen;
    unsigned char senseAwakeLevel, humidityLevel, smartFlexLevel;

    quint16 c1, c2, c3, c4;
//    quint16 d1, d2, d3;
    unsigned char d1, d2, d3, d4, d5, d6;

    int usage; //,runtime;

    QDate date;

    int nblock = 0;

    // Go through blocks of data until end marker is found
    do {
        nblock++;

        in >> ts;
        if (ts == 0xffffffff) {
            qDebug() << "SS SUM 0xffffffff terminator found at block" << nblock;
            break;
        }
        if ((ts & 0xffff) == 0xfafe) {
            qDebug() << "SS SUM 0xfafa terminator found at block" << nblock;
            break;
        }

        ts = ssconvertDate(ts);

        qDebug() << "\nSS SUM Session" << nblock << "with timestamp" << ts << QDateTime::fromSecsSinceEpoch(ts).toString("MM/dd/yyyy hh:mm:ss");

        // the following two quite often match in value
        in >> runTime;          // 0x04
        in >> useTime;          // 0x05
        usage = useTime * 360;  // Convert to seconds (durations are in .1 hour intervals)

        in >> minPressSeen;     // 0x06
        in >> pct95PressSeen;   // 0x07
        in >> maxPressSeen;     // 0x08

        in >> d1;  // 0x09
        in >> d2;  // 0x0a
        in >> d3;  // 0x0b
        in >> d4;  // 0x0c
        in >> d5;  // 0x0d
        in >> d6;  // 0x0e

        in >> c1;  // 0x0f
        in >> c2;  // 0x11
        in >> c3;  // 0x13
        in >> c4;  // 0x15

        in >> j1;  // 0x17

        in >> p1;  // 0x18
        in >> p2;  // 0x19
        in >> p3;  // 0x1a

        in >> x1;  // 0x1b
        in >> x2;  // 0x1c

        in >> minPressSet;
        in >> maxPressSet;
        in >> senseAwakeLevel;
        in >> humidityLevel;
        in >> smartFlexLevel;

        // soak up unknown stuff to apparent end of data for the day
        unsigned char s [6];
        for (unsigned int i=0; i < sizeof(s); i++)
            in >> s[i];

        qDebug() << "SS SUM block" << nblock
                 << "a:"   <<"Pressure Min"<<minPressSeen<<"95%"<<pct95PressSeen<<"Max"<<maxPressSeen
                 << "\nd:" <<d1<<d2<<d3<<d4<<d5<<d6
                 << "\nj:" <<j1 << "   c:" << c1 << c2 << c3 << c4
                 << "\np:" <<p1<<p2<<p3
                 << "\nx:" <<x1<<x2
                 << "\ns:" <<"Min set" <<minPressSet<<"Max set"<<maxPressSet<<"SA"<<senseAwakeLevel<<"Humid"<<humidityLevel<<"SmartFlex"<<smartFlexLevel<<s[0]<<s[1]<<s[2]<<s[3]<<s[4]<<s[5];

        if (runTime != useTime) {
            qDebug() << "SS SUM run time" << runTime << "!= use time" << useTime << "-" << nblock << QDateTime::fromSecsSinceEpoch(ts).toString("MM/dd/yyyy hh:mm:ss");
        }

        if (!mach->SessionExists(ts)) {
            Session *sess = new Session(mach, ts);
            sess->really_set_first(qint64(ts) * 1000L);
            sess->really_set_last(qint64(ts + usage) * 1000L);
            sess->SetChanged(true);
/****
            // TODO: None of the apnea numbers have been confirmed
            sess->setCount(CPAP_Obstructive, c3);
//            sess->setCph(CPAP_Obstructive, c3 / (float(usage)/3600.00));

            sess->setCount(CPAP_Hypopnea, c4);
//            sess->setCph(CPAP_Hypopnea, c4 / (float(usage)/3600.00));

            sess->setCount(CPAP_ClearAirway, c1);
//            sess->setCph(CPAP_ClearAirway, c1 / (float(usage)/3600.00));

            sess->setCount(CPAP_FlowLimit, c2);
//            sess->setCph(CPAP_FlowLimit, c2 / (float(usage)/3600.00));
****/
            SessDate.insert(date, sess);

            if (minPressSet != maxPressSet) {
                sess->settings[CPAP_Mode] = (int)MODE_APAP;
                sess->settings[CPAP_PressureMin] = minPressSet / 10.0;
                sess->settings[CPAP_PressureMax] = maxPressSet / 10.0;
            } else {
                sess->settings[CPAP_Mode] = (int)MODE_CPAP;
                sess->settings[CPAP_Pressure] = minPressSet / 10.0;
            }

            sess->settings[CPAP_HumidSetting] = humidityLevel;
            sess->settings[SS_SenseAwakeLevel] = senseAwakeLevel / 10.0;
            sess->settings[CPAP_PresReliefMode] = PR_SMARTFLEX;
            sess->settings[SS_SmartFlexLevel] = smartFlexLevel / 1.0;

            Sessions[ts] = sess;

            addSession(sess);
        }
    } while (!in.atEnd());

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Open Detail record contains list of sessions and pressure, leak, and event flags
////////////////////////////////////////////////////////////////////////////////////////////
bool SleepStyleLoader::OpenDetail(Machine *mach, const QString & filename)
{
    Q_UNUSED(mach);

    qDebug() << "SS DET Opening Detail" << filename;
    QByteArray header;
    QFile file(filename);

    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "SS DET Couldn't open" << filename;
        return false;
    }

    header = file.read(0x200);

    if (header.size() != 0x200) {
        qDebug() << "SS DET short file" << filename;
        file.close();
        return false;
    }

    // Header is terminated by ';' at 0x1ff
    unsigned char hterm = 0x3b;

    if (hterm != header[0x1ff]) {
        file.close();
        qWarning() << "SS DET Header missing ';' terminator" << filename;
        return false;
    }

    QTextStream htxt(&header);
    QString h1, version, fname, serial, model, type, unknownident;
    htxt >> h1;
    htxt >> version;
    htxt >> fname;
    htxt >> serial;
    htxt >> model; //TODO: Should become Series in machine info???
    htxt >> type;  // SPSAAN etc with 4th character being A (Auto) or C (CPAP)
    htxt >> unknownident; // Constant, but has different value when version number is different.

    qDebug() << "SS DET file header" << h1 << version << fname << serial << model << type << unknownident;

    // Read session indices
    QByteArray index = file.read(0x800);
    if (index.size()!=0x800) {
        // faulty file..
        qWarning() << "SS DET file short index block";
        file.close();
        return false;
    }
    QDataStream in(index);
    quint32 ts;

    in.setVersion(QDataStream::Qt_4_6);
    in.setByteOrder(QDataStream::LittleEndian);

    QVector<quint32> times;
    QVector<quint16> start;
    QVector<quint8> records;

    quint16 strt;
    quint8 recs;
    quint16 unknownIndex;

    int totalrecs = 0;

    do {
        // Read timestamp for session and check for end of data signal
        in >> ts;
        if (ts == 0xffffffff) break;
        if ((ts & 0xffff) == 0xfafe) break;

        ts = ssconvertDate(ts);

        in >> strt;
        in >> recs;
        in >> unknownIndex;
        totalrecs += recs;      // Number of data records for this session

        qDebug().noquote() << "SS DET block timestamp" << ts << QDateTime::fromSecsSinceEpoch(ts).toString("MM/dd/yyyy hh:mm:ss") << "start" << strt << "records" << recs << "unknown" << unknownIndex;

        if (Sessions.contains(ts)) {
            times.push_back(ts);
            start.push_back(strt);
            records.push_back(recs);
        }
        else
            qDebug() << "SS DET session not found" << ts;
    } while (!in.atEnd());

    QByteArray databytes = file.readAll();
    file.close();

    in.setVersion(QDataStream::Qt_4_6);
    in.setByteOrder(QDataStream::BigEndian);

    // 7 (was 5) byte repeating patterns

    quint8 *data = (quint8 *)databytes.data();

    qint64 ti;
    quint8 pressure, leak, a1, a2, a3, a4, a5, a6, a7;
//    quint8 sa1, sa2;  // The two sense awake bits per 2 minutes
    SessionID sessid;
    Session *sess;
    int idx;

    for (int r = 0; r < start.size(); r++) {
        sessid = times[r];
        sess = Sessions[sessid];
        ti = qint64(sessid) * 1000L;
        sess->really_set_first(ti);

        EventList *LK = sess->AddEventList(CPAP_LeakTotal, EVL_Event, 1);
        EventList *PR = sess->AddEventList(CPAP_Pressure, EVL_Event, 0.1F);
//        EventList *OA = sess->AddEventList(CPAP_Obstructive, EVL_Event);
        EventList *H =  sess->AddEventList(CPAP_Hypopnea, EVL_Event);
        EventList *FL = sess->AddEventList(CPAP_FlowLimit, EVL_Event);
        EventList *SA = sess->AddEventList(CPAP_SensAwake, EVL_Event);
//        EventList *CA = sess->AddEventList(CPAP_ClearAirway, EVL_Event);
        EventList *UA = sess->AddEventList(CPAP_Apnea, EVL_Event);
// For testing to determine which bit is for which event type:
//        EventList *UF1 = sess->AddEventList(CPAP_UserFlag1, EVL_Event);
//        EventList *UF2 = sess->AddEventList(CPAP_UserFlag2, EVL_Event);

        unsigned stidx = start[r];
        int rec = records[r];

        idx = stidx * 21; // Each record has three blocks of 7 bytes for 21 bytes total

        quint8 bitmask;
        for (int i = 0; i < rec; ++i) {
            for (int j = 0; j < 3; ++j) {
                pressure = data[idx];
                PR->AddEvent(ti/*+120000*/, pressure);

                leak = data[idx + 1];
                LK->AddEvent(ti/*+120000*/, leak);

                                      // Comments below from MW. Appear not to be accurate
                a1 = data[idx + 2];   // [0..5] Obstructive flag, [6..7] Unknown
                a2 = data[idx + 3];   // [0..5] Hypopnea,         [6..7] Unknown
                a3 = data[idx + 4];   // [0..5] Flow Limitation,  [6..7] Unknown
                a4 = data[idx + 5];   // [0..5] UF1,  [6..7] Unknown
                a5 = data[idx + 6];   // [0..5] UF2,  [6..7] Unknown

                // Sure there isn't 6 SenseAwake bits?
                a6 = (a3 >> 6) << 4 | ((a4 >> 6) << 2) | (a5 >> 6);

                // this does the same thing as behaviour
//                a6 = (a3 >> 7) << 3 | ((a3 >> 6) & 1);
                a7 =   (a1 >> 6) | (a2 >> 6); // Are these bits used?

                bitmask = 1;
                for (int k = 0; k < 6; k++) {  // There are 6 flag sets per 2 minutes
                    if (a1 & bitmask) { UA->AddEvent(ti+60000, 0); }
                    if (a2 & bitmask) { UA->AddEvent(ti+60000, 0); } // may be CA?
                    if (a3 & bitmask) {  H->AddEvent(ti+60000, 0); }
                    if (a4 & bitmask) {  H->AddEvent(ti+60000, 0); } // may be OA?
                    if (a5 & bitmask) { FL->AddEvent(ti+60000, 0); }
                    if (a6 & bitmask) { SA->AddEvent(ti+60000, 0); }

                    bitmask <<= 1;
                    ti += 20000L;  // Increment 20 seconds
                }

                // Debug print non-zero flags
                if (a1 != 0 || a2 != 0 || a3 != 0 || a4 != 0 || a5 != 0 || a6 != 0 || a7 != 0) {
                    qDebug() << "SS DET events" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("MM/dd/yyyy hh:mm:ss")
                             << "pressure" << pressure
                             << "leak" << leak
                             << "flags" << a1 << a2 << a3 << a4 << a5 << a6 << "unknown" << a7;
                }

                idx += 7; //was 5;
            }
        }

        // Update indexes, process waveform and perform flagging
        sess->UpdateSummaries();

        //  sess->really_set_last(ti-360000L);
        //        sess->SetChanged(true);
        //       addSession(sess,profile);
    }

    return 1;
}

void SleepStyleLoader::initChannels()
{
    using namespace schema;
    Channel * chan = nullptr;

/****
    channel.add(GRP_CPAP, chan = new Channel(INTP_SmartFlexMode = 0x1165, SETTING,  MT_CPAP,  SESSION,
        "INTPSmartFlexMode", QObject::tr("SmartFlex Mode"),
        QObject::tr("Pressure relief mode."),
        QObject::tr("SmartFlex Mode"),
        "", DEFAULT, Qt::green));

    chan->addOption(0, STR_TR_Off);
****/

    channel.add(GRP_CPAP, chan = new Channel(SS_SmartFlexLevel = 0xf304, SETTING,  MT_CPAP,  SESSION,
        "SSSmartFlexLevel", QObject::tr("SmartFlex Level"),
        QObject::tr("Exhalation pressure relief level."),
        QObject::tr("SmartFlex"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);

    channel.add(GRP_CPAP, new Channel(SS_SenseAwakeLevel = 0xf305, SETTING,  MT_CPAP,   SESSION,
        "SS_SenseAwakeLevel",
        QObject::tr("SenseAwake level"),
        QObject::tr("SenseAwake level"),
        QObject::tr("SenseAwake"),
        STR_UNIT_CMH2O, LOOKUP, Qt::black));

}

bool sleepstyle_initialized = false;
void SleepStyleLoader::Register()
{
    if (sleepstyle_initialized) { return; }

    qDebug() << "Registering F&P Sleepstyle Loader";
    RegisterLoader(new SleepStyleLoader());
    //InitModelMap();
    sleepstyle_initialized = true;
}
