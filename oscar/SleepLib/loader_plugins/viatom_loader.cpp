/* SleepLib Viatom Loader Implementation
 *
 * Copyright (c) 2019-2020 The OSCAR Team
 * (Initial importer written by dave madden <dhm@mersenne.com>)
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
    // I don't know under what circumstances this is called...
    qDebug() << "ViatomLoader::Detect(" << path << ")";
    return true;
}

int
ViatomLoader::Open(const QString & dirpath)
{
    // I don't know under what circumstances this is called...
    qDebug() << "ViatomLoader::Open(" << dirpath << ")";
    return 0;
}

int
ViatomLoader::OpenFile(const QString & filename)
{
    bool ok = false;
    s_unexpectedMessages.clear();
    
    qDebug() << "ViatomLoader::OpenFile(" << filename << ")";

    Session* sess = ParseFile(filename);
    if (sess == nullptr) {
        QMessageBox::information(QApplication::activeWindow(),
                                 QObject::tr("Unrecognized File"),
                                 QObject::tr("This file does not appear to be a Viatom data file.") +"\n\n"+
                                 QObject::tr("If it is, the developers will need a copy of this file so that future versions of OSCAR will be able to read it.")
                                 ,QMessageBox::Ok);
    } else {
        SaveSessionToDatabase(sess);
        ok = true;

        if (s_unexpectedMessages.count() > 0 && p_profile->session->warnOnUnexpectedData()) {
            // Compare this to the list of messages previously seen for this machine
            // and only alert if there are new ones.
            QSet<QString> newMessages = s_unexpectedMessages - sess->machine()->previouslySeenUnexpectedData();
            if (newMessages.count() > 0) {
                // TODO: Rework the importer call structure so that this can become an
                // emit statement to the appropriate import job.
                QMessageBox::information(QApplication::activeWindow(),
                                         QObject::tr("Untested Data"),
                                         QObject::tr("Your Viatom device generated data that OSCAR has never seen before.") +"\n\n"+
                                         QObject::tr("The imported data may not be entirely accurate, so the developers would like a copy of this file to make sure OSCAR is handling the data correctly.")
                                         ,QMessageBox::Ok);
                sess->machine()->previouslySeenUnexpectedData() += newMessages;
            }
        }
    }

    return ok;
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

    // TODO: Figure out what to do about machine ID. Right now OSCAR generates a random ID since we don't specify one.
    // That means you won't be able to import multiple Viatom devices in a single session/profile.
    MachineInfo  info = newInfo();
    Machine     *mach = p_profile->CreateMachine(info);
    Session     *sess = mach->SessionExists(v.sessionid());
    quint64  time_ms = v.timestamp();
    QDateTime data_timestamp = QDateTime::fromMSecsSinceEpoch(time_ms);

    if (!sess) {
        qDebug() << "Session at" << data_timestamp << "not found...create new session" << v.sessionid();
        sess = new Session(mach, v.sessionid());
        sess->really_set_first(time_ms);
    } else {
        qDebug() << "Session" << v.sessionid() << "found...add data to it";
    }

    QList<ViatomFile::Record> records = v.ReadData();

    quint64  step = v.duration() / records.size() * 1000L;
    //CHECK_VALUES(step, 2000, 4000);  // TODO: once ReadData deduplicates the records, there will only be 4000
    EventList *ev_hr = sess->AddEventList(OXI_Pulse, EVL_Waveform,  1.0, 0.0, 0.0, 0.0, step);
    EventList *ev_o2 = sess->AddEventList(OXI_SPO2, EVL_Waveform,   1.0, 0.0, 0.0, 0.0, step);
    EventList *ev_mv = sess->AddEventList(POS_Motion, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, step);

    // Import data
    // TODO: Add support for multiple eventlists rather than holding the previous value.
    ViatomFile::Record prev = { 99, 60, 0, 0, 0 };
    for (auto & rec : records) {
        if (rec.spo2 < 50 || rec.spo2 > 100) rec.spo2 = prev.spo2;
        if (rec.hr < 20 || rec.hr > 200) rec.hr = prev.hr;
        if (rec.motion > 200) rec.motion = prev.motion;

        sess->set_last(time_ms);
        ev_hr->AddEvent(time_ms, rec.hr);
        ev_o2->AddEvent(time_ms, rec.spo2);
        ev_mv->AddEvent(time_ms, rec.motion);

        prev = rec;
        time_ms += step;
    }

    /*
    qDebug() << "Read Viatom data from" << data_timestamp << "to" << (QDateTime::fromSecsSinceEpoch( time_ms / 1000L))
             << records.count() << "records"
             << ev_mv->Min() << "<=Motion<=" << ev_mv->Max();
    */

    sess->setMin(OXI_Pulse,  ev_hr->Min());
    sess->setMax(OXI_Pulse,  ev_hr->Max());
    sess->setMin(OXI_SPO2,   ev_o2->Min());
    sess->setMax(OXI_SPO2,   ev_o2->Max());
    sess->setMin(POS_Motion, ev_mv->Min());
    sess->setMax(POS_Motion, ev_mv->Max());

    sess->really_set_last(time_ms);

    return sess;
}

void ViatomLoader::SaveSessionToDatabase(Session* sess)
{
    Machine* mach = sess->machine();
    
    sess->SetChanged(true);
    mach->AddSession(sess);
    mach->Save();
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

    if (sig != 0x0003) {
        qDebug() << m_file.fileName() << "invalid signature for Viatom data file" << sig;
        return false;
    }
    if ((year < 2015 || year > 2059) || (month < 1 || month > 12) || (day < 1 || day > 31) ||
        (hour > 23) || (min > 59) || (sec > 59)) {
        qDebug() << m_file.fileName() << "invalid timestamp in Viatom data file";
        return false;
    }

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
    CHECK_VALUE(header[21], 0);
    //int time_under_90pct = header[22];  // in seconds
    CHECK_VALUE(header[23], 0);
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
    CHECK_VALUE(datasize % RECORD_SIZE, 0);
    CHECK_VALUE(m_duration % m_record_count, 0);

    qDebug().noquote() << m_file.fileName() << ts(m_timestamp) << dur(m_duration * 1000L) << ":" << m_record_count << "records @" << m_resolution << "ms";

    return true;
}

QList<ViatomFile::Record> ViatomFile::ReadData()
{
    QByteArray data = m_file.readAll();
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    QList<ViatomFile::Record> records;

    // Read all Pulse, SPO2 and Motion data
    do {
        ViatomFile::Record rec;
        in >> rec.spo2 >> rec.hr >> rec._unk1 >> rec.motion >> rec._unk2;
        records.append(rec);
    } while (!in.atEnd());

    // TODO: deduplicate the samples
    /* It turns out 2s files are actually just double-reported samples!
    if (m_resolution == 2000) {
        CHECK_VALUE(records.size() % 2, 0);
        for (int i = 0; i < records.size(); i += 2) {
            auto & a = records.at(i);
            auto & b = records.at(i+1);
            CHECK_VALUE(a.spo2, b.spo2);
            CHECK_VALUE(a.hr, b.hr);
            CHECK_VALUE(a._unk1, b._unk1);
            CHECK_VALUE(a.motion, b.motion);
            CHECK_VALUE(a._unk2, b._unk2);
        }
    }
    */

    return records;
}
