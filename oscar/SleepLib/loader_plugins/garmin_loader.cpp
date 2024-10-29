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

    QJsonArray levels = jsonObject["sleep"].toObject()["sleepLevels"].toArray();
    QJsonArray heartrates = jsonObject["sleep"].toObject()["sleepHeartRate"].toArray();
    QJsonArray sleepmovements = jsonObject["sleep"].toObject()["sleepMovement"].toArray();
    QJsonArray o2 = jsonObject["sleep"].toObject()["wellnessEpochSPO2DataDTOList"].toArray();
    QJsonArray stress = jsonObject["sleep"].toObject()["sleepStress"].toArray();
    QJsonArray hrvData = jsonObject["sleep"].toObject()["hrvData"].toArray();

    QList<QJsonArray> list = {levels, heartrates, sleepmovements, o2, stress, hrvData};

    QDateTime startTS, endTS;
    int count = 0;
    foreach (QJsonArray items, list)
    {
        if (items.size() == 0)
        {
            continue;
        }
        count += items.size();
        QString timeFieldName = getTimeFieldField(items);
        QDateTime first = getDateTime(items.first().toObject()[timeFieldName]);
        QDateTime last = getDateTime(items.last().toObject()[timeFieldName]);
        if (!startTS.isValid())
        {
            startTS = first;
        }
        if (!endTS.isValid())
        {
            endTS = last;
        }
        startTS = min(startTS, first);
        endTS = max(endTS, last);
    }
    qDebug() << "Starting Import. Timerange: " << startTS << endTS << "Entries:" << count << "Timezone-Offset: " << timezoneOffset(startTS) / 1000 / 60;
    SessionID sid = startTS.toTime_t();
    if (mach->SessionExists(sid))
    {
        qCritical() << "Session already Exists";
        return 0;
    }
    Session *sess = new Session(mach, sid);
    m_session = sess;
    sess->really_set_first(qint64(startTS.toTime_t()) * 1000L);

    importValues(POS_Movement, sleepmovements, 0, 10, "activityLevel");
    importValues(ZEO_SleepStage, levels, 0, 4, "activityLevel");
    importValues(OXI_SPO2, o2, 70, 100, "spo2Reading");

    importValues(OXI_Pulse, heartrates, 0, 200, "value");
    importValues(GARMIN_Stress, stress, 0, 100, "value");
    importValues(GARMIN_HRV, hrvData, 0, 100, "value");

    sess->really_set_last(qint64(endTS.toTime_t()) * 1000L);
    sess->SetChanged(true);

    mach->AddSession(sess);
    mach->Save();
    mach->SaveSummaryCache();

    p_profile->StoreMachines();

    return count;
}

QString GARMINLoader::getTimeFieldField(QJsonArray items)
{
    QString timeFieldName;
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
    }
    return timeFieldName;
}

void GARMINLoader::importValues(ChannelID cid, QJsonArray items, int minRange, int maxRange, QString attr)
{
    qint64 ts = 0;
    bool containsEndGmt = items.last().toObject().contains("endGMT");

    if (items.size() == 0)
    {
        qDebug() << "Nothing to import for " << schema::channel[cid].label() << schema::channel[cid].code();
    }
    QString timeFieldName = getTimeFieldField(items);

    QDateTime first = getDateTime(items.first().toObject()[timeFieldName]);
    QDateTime last = getDateTime(items.last().toObject()[timeFieldName]);
    qDebug() << "Importing" << schema::channel[cid].label() << schema::channel[cid].code() 
             << "Entries:" << items.size() << first << last;

    for (auto item : items)
    {
        QDateTime start     = getDateTime(item.toObject()[timeFieldName]);
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
}


/**
 * @brief get DateTime-Object from Json DateTime Value
 *
 * Handels Timestamps (1728880508000) and ISO-Format (2024-10-19T19:18:00.0)
 * and correct timezone
 *
 * Timestamp: Pulse, hrv, stress
 * ISO-Format: movement, sleep-level, spo2
 * 
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
        dt = dt.addMSecs(timezoneOffset(dt));
    }
    // CEST, CET
    if (dt.timeZoneAbbreviation() == "CEST") 
    {
        dt = dt.addMSecs(-60 * 60 * 1000); // 1h correction for the incorret time of airsense 11
    }
    return dt;
}


qint64 GARMINLoader::timezoneOffset(QDateTime dt)
{
    static qint64 _TZ_offset = 0;

    QDateTime d1 = dt;
    QDateTime d2 = d1;
    d1.setTimeSpec(Qt::UTC);
    _TZ_offset = d2.secsTo(d1);
    _TZ_offset *= 1000L;
    return _TZ_offset;
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
