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
#include "viatom_loader.h"
#include "SleepLib/machine.h"

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
    qDebug() << "ViatomLoader::OpenFile(" << filename << ")";

    Session* sess = ParseFile(filename);
    if (sess != nullptr) {
        SaveSessionToDatabase(sess);
        return true;
    }
    return false;
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

    EventList *ev_hr = sess->AddEventList(OXI_Pulse, EVL_Waveform,  1.0, 0.0, 0.0, 0.0, 2000.0);
    EventList *ev_o2 = sess->AddEventList(OXI_SPO2, EVL_Waveform,   1.0, 0.0, 0.0, 0.0, 2000.0);
    EventList *ev_mv = sess->AddEventList(POS_Motion, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, 2000.0);

    quint64  step = 2000;  // records @ 2000ms (2 sec)

    QList<ViatomFile::Record> records = v.ReadData();

    // Import data
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

    qDebug() << "Read Viatom data from" << data_timestamp << "to" << (QDateTime::fromSecsSinceEpoch( time_ms / 1000L))
             << records.count() << "records"
             << ev_mv->Min() << "<=Motion<=" << ev_mv->Max();

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

ViatomFile::ViatomFile(QFile & file) : m_file(file)
{
}

bool ViatomFile::ParseHeader()
{
    QByteArray data;
    qint64 filesize = m_file.size();

    data = m_file.read(50);

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    quint16 sig;
    quint16 Y;
    quint8 m,d,H,M,S;

    in >> sig >> Y >> m >> d >> H >> M >> S;

    if (sig != 0x0003 ||
        (Y < 2015 || Y > 2040) ||
        (m < 1 || m > 12) ||
        (d < 1 || d > 31) ||
        (         H > 23) ||
        (         M > 60) ||
        (         S > 61)) {
        qDebug() << m_file.fileName() << "does not appear to be a Viatom data file";
        return false;
    }

    QDateTime data_timestamp = QDateTime(QDate(Y, m, d), QTime(H, M, S));
    m_timestamp = data_timestamp.toMSecsSinceEpoch();
    m_id = m_timestamp / 1000L;

    qDebug() << m_file.fileName() << "looks like a Viatom file, size" << filesize << "bytes signature" << sig
             << "start date/time" << data_timestamp << "(" << m_timestamp << ")";

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

    return records;
}
