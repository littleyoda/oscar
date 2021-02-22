/* SleepLib Viatom Loader Implementation
 *
 * Copyright (c) 2019-2020 The OSCAR Team
 * (Initial importer written by dave madden <dhm@mersenne.com>)
 * Modified 02/21/2021 to allow for CheckMe device data files by Crimson Nape <CrimsonNape@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the viatom_data_version in viatom_loader.h when making changes to this loader
// that change loader behaviour or modify channels.
//********************************************************************************************

#include <QDir>
#include <QTextStream>
#include <QApplication>
#include <QMessageBox>
#include "viatom_loader.h"
#include "SleepLib/machine.h"

static QSet<QString> s_unexpectedMessages;

bool
ViatomLoader::Detect(const QString & path)
{
    // This is only used for CPAP machines, when detecting CPAP cards.
    qDebug() << "ViatomLoader::Detect(" << path << ")";
    return false;
}

int
ViatomLoader::Open(const QString & dirpath)
{
    qDebug() << "ViatomLoader::Open(" << dirpath << ")";
    m_mach = nullptr;
    int imported = 0;
    int found = 0;
    s_unexpectedMessages.clear();

    if (QFileInfo(dirpath).isDir()) {
        QDir dir(dirpath);
        dir.setFilter(QDir::NoDotAndDotDot | QDir::Files | QDir::Hidden);
        dir.setNameFilters(getNameFilter());
        dir.setSorting(QDir::Name);

        for (auto & fi : dir.entryInfoList()) {
            if (OpenFile(fi.canonicalFilePath())) {
                imported++;
            }
            found++;
        }
    }
    else {
        // This filename has already been filtered by QFileDialog.
        if (OpenFile(dirpath)) {
            imported++;
        }
        found++;
    }

    if (!found) {
        return -1;
    }

    Machine* mach = m_mach;
    if (imported && mach == nullptr) qWarning() << "No machine record created?";
    if (mach) {
        qDebug() << "Imported" << imported << "sessions";
        mach->Save();
        mach->SaveSummaryCache();
        p_profile->StoreMachines();
    }
    if (mach && s_unexpectedMessages.count() > 0 && p_profile->session->warnOnUnexpectedData()) {
        // Compare this to the list of messages previously seen for this machine
        // and only alert if there are new ones.
        QSet<QString> newMessages = s_unexpectedMessages - mach->previouslySeenUnexpectedData();
        if (newMessages.count() > 0) {
            // TODO: Rework the importer call structure so that this can become an
            // emit statement to the appropriate import job.
            QMessageBox::information(QApplication::activeWindow(),
                                     QObject::tr("Untested Data"),
                                     QObject::tr("Your Viatom device generated data that OSCAR has never seen before.") +"\n\n"+
                                     QObject::tr("The imported data may not be entirely accurate, so the developers would like a copy of your Viatom files to make sure OSCAR is handling the data correctly.")
                                     ,QMessageBox::Ok);
            mach->previouslySeenUnexpectedData() += newMessages;
        }
    }

    return imported;
}

bool ViatomLoader::OpenFile(const QString & filename)
{
    Machine* mach = nullptr;

    Session* sess = ParseFile(filename);
    if (sess) {
        SaveSessionToDatabase(sess);
        mach = sess->machine();
        m_mach = mach;
    }

    return mach != nullptr;
}

Session* ViatomLoader::ParseFile(const QString & filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Couldn't open Viatom data file" << filename;
        return nullptr;
    }

    ViatomFile v(file);
    if (v.ParseHeader() == false) {
        return nullptr;
    }

    MachineInfo info = newInfo();
    // Check whether the enclosing folder looks like a Viatom serial number, and if so, use it.
    QString foldername = QFileInfo(filename).dir().dirName();
    if (foldername.length() >= 9) {
        bool numeric;
        foldername.rightRef(4).toInt(&numeric);
        if (numeric) {
            info.serial = foldername;
        }
    }
    Machine *mach = p_profile->CreateMachine(info);

    if (mach->SessionExists(v.sessionid())) {
        // Skip already imported session
        //qDebug() << filename << "session already exists, skipping" << v.sessionid();
        return nullptr;
    }

    qint64 time_ms = v.timestamp();
    m_session = new Session(mach, v.sessionid());
    m_session->set_first(time_ms);

    QList<ViatomFile::Record> records = v.ReadData();
    m_step = v.duration() / records.size() * 1000L;

    // Import data
    for (auto & rec : records) {
        if (rec.oximetry_invalid) {
            EndEventList(OXI_Pulse, time_ms);
            EndEventList(OXI_SPO2, time_ms);
        } else {
            AddEvent(OXI_Pulse, time_ms, rec.hr);
            AddEvent(OXI_SPO2, time_ms, rec.spo2);
        }
        AddEvent(POS_Movement, time_ms, rec.motion);
        time_ms += m_step;
    }
    EndEventList(OXI_Pulse, time_ms);
    EndEventList(OXI_SPO2, time_ms);
    EndEventList(POS_Movement, time_ms);
    m_session->set_last(time_ms);

    return m_session;
}

void ViatomLoader::SaveSessionToDatabase(Session* sess)
{
    Machine* mach = sess->machine();

    sess->SetChanged(true);
    mach->AddSession(sess);
}

void ViatomLoader::AddEvent(ChannelID channel, qint64 t, EventDataType value)
{
    EventList* C = m_importChannels[channel];
    if (C == nullptr) {
        C = m_session->AddEventList(channel, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, m_step);
        Q_ASSERT(C);  // Once upon a time AddEventList could return nullptr, but not any more.
        m_importChannels[channel] = C;
    }
    // Add the event
    C->AddEvent(t, value);
    m_importLastValue[channel] = value;
}

void ViatomLoader::EndEventList(ChannelID channel, qint64 /*t*/)
{
    EventList* C = m_importChannels[channel];
    if (C != nullptr) {
        // The below would be needed for square charts if the first sample represents
        // the 4 seconds following the starting timestamp:
        //C->AddEvent(t, m_importLastValue[channel]);

        // Mark this channel's event list as ended.
        m_importChannels[channel] = nullptr;
    }
}

QStringList ViatomLoader::getNameFilter()
{
    return QStringList("20[0-5][0-9][01][0-9][0-3][0-9][012][0-9][0-5][0-9][0-5][0-9]");
}

static bool viatom_initialized = false;

void
ViatomLoader::Register()
{
    if (!viatom_initialized) {
        qDebug("Registering ViatomLoader");
        RegisterLoader(new ViatomLoader());
        //InitModelMap();
        viatom_initialized = true;
    }
}


// ===============================================================================================

/*
static QString ts(qint64 msecs)
{
    // TODO: make this UTC so that tests don't vary by where they're run
    return QDateTime::fromMSecsSinceEpoch(msecs).toString(Qt::ISODate);
}

static QString dur(qint64 msecs)
{
    qint64 s = msecs / 1000L;
    int h = s / 3600; s -= h * 3600;
    int m = s / 60; s -= m * 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}
*/

// TODO: Merge this with PRS1 macros and generalize for all loaders.
#define UNEXPECTED_VALUE(SRC, VALS) { \
    QString message = QString("%1:%2: %3 = %4 != %5").arg(__func__).arg(__LINE__).arg(#SRC).arg(SRC).arg(VALS); \
    qWarning() << this->m_sessionid << message; \
    s_unexpectedMessages += message; \
    }
#define CHECK_VALUE(SRC, VAL) if ((SRC) != (VAL)) UNEXPECTED_VALUE(SRC, VAL)
#define CHECK_VALUES(SRC, VAL1, VAL2) if ((SRC) != (VAL1) && (SRC) != (VAL2)) UNEXPECTED_VALUE(SRC, #VAL1 " or " #VAL2)
// for more than 2 values, just write the test manually and use UNEXPECTED_VALUE if it fails


ViatomFile::ViatomFile(QFile & file) : m_file(file)
{
}

bool ViatomFile::ParseHeader()
{
    static const int HEADER_SIZE = 40;
    QByteArray data = m_file.read(HEADER_SIZE);
    if (data.size() < HEADER_SIZE) {
        qDebug() << m_file.fileName() << "too short for a Viatom data file";
        return false;
    }

    const unsigned char* header = (const unsigned char*) data.constData();
    int sig   = header[0] | (header[1] << 8);
    int year  = header[2] | (header[3] << 8);
    int month = header[4];
    int day   = header[5];
    int hour  = header[6];
    int min   = header[7];
    int sec   = header[8];


    /*  CN - Changed the if statement to a switch to accomdate additional Viatom/Wellue signatures in the future
    if (sig != 0x0003) {
        qDebug() << m_file.fileName() << "invalid signature for Viatom data file" << sig;
        return false;
    }
   CN */
    switch (sig){
    case 0x0003:
    case 0x0005:
        break;
    default:
        qDebug() << m_file.fileName() << "invalid signature for Viatom data file" << sig;
        return false;
        break;
    }

    if ((year < 2015 || year > 2059) || (month < 1 || month > 12) || (day < 1 || day > 31) ||
        (hour > 23) || (min > 59) || (sec > 59)) {
        qDebug() << m_file.fileName() << "invalid timestamp in Viatom data file";
        return false;
    }

    // It's unclear what the starting timestamp represents: is it the time at which
    // the device starts measuring data, and the first sample is 4s after that? Or
    // is the starting timestamp the time at which the first 4s average is reported
    // (and the first 4 seconds being average precede the starting timestamp)?
    //
    // If the former, then the chart draws the first sample too early (right at the
    // starting timestamp). Technically these should probably be square charts, but
    // the code currently forces them to be non-square.
    QDateTime data_timestamp = QDateTime(QDate(year, month, day), QTime(hour, min, sec));
    m_timestamp = data_timestamp.toMSecsSinceEpoch();
    m_sessionid = m_timestamp / 1000L;

    int filesize = header[9] | (header[10] << 8);  // possibly 32-bit
    CHECK_VALUE(header[11], 0);
    CHECK_VALUE(header[12], 0);

    m_duration   = header[13] | (header[14] << 8);  // possibly 32-bit
    CHECK_VALUE(header[15], 0);
    CHECK_VALUE(header[16], 0);

    //int spo2_avg = header[17];
    //int spo2_min = header[18];
    //int spo2_3pct = header[19];  // number of events
    //int spo2_4pct = header[20];  // number of events
    //CHECK_VALUE(header[21], 0);  // ??? sometimes nonzero; maybe pulse spike, not a threshold of SpO2 or pulse, not always smaller than spo2_4pct
    //int time_under_90pct = header[22] | (header[23] << 8);  // in seconds
    //int events_under_90pct = header[24];  // number of distinct events
    //float o2_score = header[25] * 0.1;
    CHECK_VALUE(header[26], 0);
    CHECK_VALUE(header[27], 0);
    CHECK_VALUE(header[28], 0);
    CHECK_VALUE(header[29], 0);
    CHECK_VALUE(header[30], 0);
    CHECK_VALUE(header[31], 0);
    CHECK_VALUE(header[32], 0);
    CHECK_VALUE(header[33], 0);
    CHECK_VALUE(header[34], 0);
    CHECK_VALUE(header[35], 0);
    CHECK_VALUE(header[36], 0);
    CHECK_VALUE(header[37], 0);
    CHECK_VALUE(header[38], 0);
    CHECK_VALUE(header[39], 0);

    // Calculate timing resolution (in ms) of the data
    qint64 datasize = m_file.size() - HEADER_SIZE;
    m_record_count = datasize / RECORD_SIZE;
    m_resolution = m_duration / m_record_count * 1000L;
    if (m_resolution == 2000) {
        // Interestingly the file size in the header corresponds the number of
        // distinct samples. These files actually double-report each sample!
        // So this resolution isn't really the real one. The importer should
        // calculate resolution from duration / record count after reading the
        // records, which will be deduplicated.
        CHECK_VALUE(filesize, ((m_file.size() - HEADER_SIZE) / 2) + HEADER_SIZE);
    } else {
        CHECK_VALUE(filesize, m_file.size());
    }
    CHECK_VALUES(m_resolution, 2000, 4000);
//    CHECK_VALUE(datasize % RECORD_SIZE, 0); CN - Commented out these 2 lines because CheckMe can record odd number of entries
//    CHECK_VALUE(m_duration % m_record_count, 0);

    //qDebug().noquote() << m_file.fileName() << ts(m_timestamp) << dur(m_duration * 1000L) << ":" << m_record_count << "records @" << m_resolution << "ms";

    return true;
}

QList<ViatomFile::Record> ViatomFile::ReadData()
{
    QByteArray data = m_file.readAll();
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    int iCheckMeAdj; // Allows for an odd number in the CheckMe  duration/# of records return
    QList<ViatomFile::Record> records;
    // Read all Pulse, SPO2 and Motion data
    do {
        ViatomFile::Record rec;
        in >> rec.spo2 >> rec.hr >> rec.oximetry_invalid >> rec.motion >> rec.vibration;
        CHECK_VALUES(rec.oximetry_invalid, 0, 0xFF); //If it doesn't have one of these 2 values, it's bad
        if (rec.vibration == 0x40) rec.vibration = 0x80; //0x040 (64) or 0x80 (128) when vibration is triggered
        CHECK_VALUES(rec.vibration, 0, 0x80);  // 0x80 (128) when vibration is triggered
        if (rec.oximetry_invalid == 0xFF) {
            CHECK_VALUE(rec.spo2, 0xFF);
            CHECK_VALUE(rec.hr, 0xFF);  // if all 3 have 0xFF, then end of data
        }
        records.append(rec);
     } while (records.size() < m_record_count); // CN Changed to allow for an incomlpete record values
// CN   } while (!in.atEnd());

    // It turns out 2s files are actually just double-reported samples!
    if (m_resolution == 2000) {
        QList<ViatomFile::Record> dedup;
        bool all_are_duplicated = true;

        CHECK_VALUE(records.size() % 2, 0);
        for (int i = 0; i < records.size(); i += 2) {
            auto & a = records.at(i);
            auto & b = records.at(i+1);
            if (a.spo2 != b.spo2
                || a.hr != b.hr
                || a.oximetry_invalid != b.oximetry_invalid
                || a.motion != b.motion
                || a.vibration != b.vibration) {
                all_are_duplicated = false;
                break;
            }
            dedup.append(a);
        }
        CHECK_VALUE(all_are_duplicated, true);
        if (all_are_duplicated) {
            // Return the deduplicated list.
            records = dedup;
        }
    }
    iCheckMeAdj = duration() / records.size();
    if(iCheckMeAdj == 3) iCheckMeAdj = 4; // CN - Sanity check for CheckMe devices since their files do not always terminate on an even number.

    CHECK_VALUE(iCheckMeAdj, 4);  // Crimson Nape - Changed to accomadate the CheckMe data files.
    //    CHECK_VALUE(duration() / records.size(), 4);  // We've only seen 4s true resolution so far.

    return records;
}

