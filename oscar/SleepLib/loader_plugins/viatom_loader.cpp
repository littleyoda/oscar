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

    QByteArray data;
    qint64 filesize = file.size();

    data = file.readAll();

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
        qDebug() << filename << "does not appear to be a Viatom data file";
        return nullptr;
    }

    QDateTime data_timestamp = QDateTime(QDate(Y, m, d), QTime(H, M, S));
    quint64 time_s = data_timestamp.toTime_t();
    quint64 time_ms = time_s * 1000L;
    SessionID sid = time_s;

    qDebug() << filename << "looks like a Viatom file, size" << filesize << "bytes signature" << sig
             << "start date/time" << data_timestamp << "(" << time_ms << ")";

    in.skipRawData(41);  // total 50 byte header, not sure what the rest is.

    MachineInfo  info = newInfo();
    Machine     *mach = p_profile->CreateMachine(info);
    Session     *sess = mach->SessionExists(sid);

    if (!sess) {
        qDebug() << "Session at" << data_timestamp << "not found...create new session" << sid;
        sess = new Session(mach, sid);
        sess->really_set_first(time_ms);
    } else {
        qDebug() << "Session" << sid << "found...add data to it";
    }

    EventList *ev_hr = sess->AddEventList(OXI_Pulse, EVL_Waveform,  1.0, 0.0, 0.0, 0.0, 2000.0);
    EventList *ev_o2 = sess->AddEventList(OXI_SPO2, EVL_Waveform,   1.0, 0.0, 0.0, 0.0, 2000.0);
    EventList *ev_mv = sess->AddEventList(POS_Motion, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, 2000.0);

    unsigned char o2, hr, x, motion;
    unsigned char o2_ = 99, hr_ = 60, motion_ = 0;
    unsigned n_rec = 0;
    quint64  step = 2000;  // records @ 2000ms (2 sec)

    // Read all Pulse, SPO2 and Motion data
    do {
        in >> o2 >> hr >> x >> motion >> x;

        if (o2 < 50 || o2 > 100) o2 = o2_;
        if (hr < 20 || hr > 200) hr = hr_;
        if (motion > 200) motion = motion_;

        sess->set_last(time_ms);
        ev_hr->AddEvent(time_ms, hr);
        ev_o2->AddEvent(time_ms, o2);
        ev_mv->AddEvent(time_ms, motion);

        o2_ = o2;
        hr_ = hr;
        motion_ = motion;
        time_ms += step;
        n_rec += 1;
    } while (!in.atEnd());

    qDebug() << "Read Viatom data from" << data_timestamp << "to" << (QDateTime::fromSecsSinceEpoch( time_ms / 1000L))
             << n_rec << "records"
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

