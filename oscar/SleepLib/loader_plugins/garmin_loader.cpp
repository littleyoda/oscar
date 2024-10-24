/* SleepLib ZEO Loader Implementation
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins
 * Copyright (c) 2024 Little Yoda
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// Please only INCREMENT the garmin_data_version in garmin_loader.h when making changes
// that change loader behaviour or modify channels in a manner that fixes old data imports.
// Note that changing the data version will require a reimport of existing data for which OSCAR
// does not keep a backup - so it should be avoided if possible.
// i.e. there is no need to change the version when adding support for new devices
//********************************************************************************************

/*
 */
#include <QDir>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include "garmin_loader.h"
#include "SleepLib/machine.h"

GARMINLoader::GARMINLoader()
{
    m_type = MT_SLEEPSTAGE;
}

GARMINLoader::~GARMINLoader()
{
    closeFile();
}

int GARMINLoader::OpenFile(const QString &filename)
{
    MachineInfo info = newInfo();
    mach = p_profile->CreateMachine(info);

    QString val;
    QFile file;
    file.setFileName(filename);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    val = file.readAll();
    file.close();
    QJsonDocument d = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject jsonObject = d.object();

    // Sleep Level
    QJsonArray levels = jsonObject["sleep"].toObject()["sleepLevels"].toArray();
    importValues(ZEO_SleepStage, mach, levels, 0, 4, "activityLevel");

    QJsonArray heartrates = jsonObject["sleep"].toObject()["sleepHeartRate"].toArray();
    importValues(OXI_Pulse, mach, heartrates, 0, 200, "value");

    QJsonArray sleepmovements = jsonObject["sleep"].toObject()["sleepMovement"].toArray();
    importValues(POS_Movement, mach, sleepmovements, 0, 10, "activityLevel");

    QJsonArray o2 = jsonObject["sleep"].toObject()["wellnessEpochSPO2DataDTOList"].toArray();
    importValues(OXI_SPO2, mach, o2, 70, 100, "spo2Reading");

    // TODO -- Channels missing
    // $.sleep.sleepStress (The stress level feature allows a user to determine their current level of stress based on their heart-rate variability)
    // $.sleep.sleepBodyBattery (The Body Battery™ energy gauge is a feature that uses a combination of heart rate variability, stress, and activity to estimate a user’s energy reserves throughout the day. )
    // $.sleep.hrvData (Heart rate variability (HRV))

    mach->Save();
    mach->SaveSummaryCache();
    p_profile->StoreMachines();

    return levels.size() + heartrates.size();
}

void GARMINLoader::importValues(ChannelID cid, Machine *mi, QJsonArray items, int minRange, int maxRange, QString attr)
{
    QDateTime start_of_night, end_of_night; //, end_of_night, rise_time;

    qint64 st, ts;
    // int stage;
    SessionID sid = 0;
    Session *sess = nullptr;
    QString timeFieldName;
    bool containsEndGmt = items.last().toObject().contains("endGMT");

    if (items.size() == 0)
    {
        qDebug() << "Nothing to import for " << schema::channel[cid].label() << schema::channel[cid].code();
    }

    // Search for field with date/time
    if (items.last().toObject().contains("startGMT"))
    {
        timeFieldName = "startGMT";
    }
    else if (items.last().toObject().contains("epochTimestamp"))
    {
        timeFieldName = "epochTimestamp";
    }
    else
    {
        qCritical() << "No Field with Time/Date found!" << items.last().toObject().keys();
        return;
    }

    start_of_night = getDateTime(items[0].toObject()[timeFieldName]);
    end_of_night = getDateTime(items.last().toObject()[timeFieldName]);
    if (containsEndGmt)
    {
        end_of_night = getDateTime(items.last().toObject()["endGMT"]);
    }
    if (!start_of_night.isValid())
    {
        qCritical() << "First Timestamp is not valid!" << start_of_night;
        return;
    }

    qDebug() << "Importing" << schema::channel[cid].label() << schema::channel[cid].code() << "--- Time: " << start_of_night << "to" << end_of_night << " --- Entries" << items.size();
    sid = start_of_night.toTime_t();
    if (mach->SessionExists(sid))
    {
        qCritical() << "Session already Exists";
        return;
    }

    sess = new Session(mach, sid);
    m_session = sess;
    st = qint64(start_of_night.toTime_t()) * 1000L;
    ts = 0;
    sess->really_set_first(st);
    for (auto item : items)
    {
        QDateTime start = getDateTime(item.toObject()[timeFieldName]);
        ts = qint64(start.toTime_t()) * 1000L;
        double level = item.toObject()[attr].toDouble();
        if (cid == ZEO_SleepStage)
        {
            // Correct Sleep Level
            //
            //          Deep Sleep  Light Sleep REM Awake
            // Garmin   0           1           2    3
            // OSCAR    4           3           2    1
            level = 4 - level;
        }

        AddEvent(cid, ts, level, minRange, maxRange);
        if (containsEndGmt)
        {
            start = getDateTime(item.toObject()["endGMT"]);
            ts = qint64(start.toTime_t()) * 1000L;
        }
    }
    EndEventList(cid, ts);
    sess->really_set_last(ts);

    sess->SetChanged(true);
    mi->AddSession(sess);
}

/**
 * @brief get DateTime-Object from Json DateTime Value
 *
 * Handels Timestamps (1728880508000) and ISO-Format (2024-10-19T19:18:00.0)
 * and correct timezone
 * @param obj QJsonValueRef with Timestamp or DateTime in ISO-Format
 * @return QDateTime
 */
QDateTime GARMINLoader::getDateTime(QJsonValueRef obj)
{
    QDateTime dt;
    if (obj.isDouble())
    {
        dt.setTime_t(obj.toDouble() / 1000.0);
    }
    if (obj.isString())
    {
        QString subString = obj.toString().mid(0, 19);
        dt = QDateTime::fromString(subString, "yyyy-MM-ddTHH:mm:ss");
        dt = dt.addMSecs(timezoneOffset());
    }
    dt = dt.addMSecs(-60 * 60 * 1000); // 1h correction for the incorret time of airsense 11
    return dt;
}

void GARMINLoader::closeFile()
{
    if (file.isOpen())
    {
        file.close();
    }
}

void GARMINLoader::AddEvent(ChannelID channel, qint64 t, EventDataType value, int minRange, int maxRange)
{
    EventList *C = m_importChannels[channel];
    if (C == nullptr)
    {
        C = m_session->AddEventList(channel, EVL_Event, 1, 0, minRange, maxRange);
        Q_ASSERT(C); // Once upon a time AddEventList could return nullptr, but not any more.
        m_importChannels[channel] = C;
    }
    // Add the event
    C->AddEvent(t, value);
    m_importLastValue[channel] = value;
}

void GARMINLoader::EndEventList(ChannelID channel, qint64 t)
{
    EventList *C = m_importChannels[channel];
    if (C != nullptr)
    {
        C->AddEvent(t, m_importLastValue[channel]);

        // Mark this channel's event list as ended.
        m_importChannels[channel] = nullptr;
    }
}

static bool garmin_initialized = false;

void GARMINLoader::Register()
{
    if (garmin_initialized)
    {
        return;
    }

    qDebug("Registering GARMINLoader");
    RegisterLoader(new GARMINLoader());
    // InitModelMap();
    garmin_initialized = true;
}
