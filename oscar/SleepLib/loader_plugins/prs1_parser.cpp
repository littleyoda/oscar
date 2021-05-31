/* SleepLib PRS1 Loader Parser Implementation
 *
 * Copyright (c) 2019-2021 The OSCAR Team
 * Portions copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#include "prs1_parser.h"
#include <QDebug>


const PRS1ParsedEventType PRS1TidalVolumeEvent::TYPE;
const PRS1ParsedEventType PRS1SnoresAtPressureEvent::TYPE;

const PRS1ParsedEventType PRS1TimedBreathEvent::TYPE;
const PRS1ParsedEventType PRS1ObstructiveApneaEvent::TYPE;
const PRS1ParsedEventType PRS1ClearAirwayEvent::TYPE;
const PRS1ParsedEventType PRS1FlowLimitationEvent::TYPE;
const PRS1ParsedEventType PRS1PeriodicBreathingEvent::TYPE;
const PRS1ParsedEventType PRS1LargeLeakEvent::TYPE;
const PRS1ParsedEventType PRS1VariableBreathingEvent::TYPE;
const PRS1ParsedEventType PRS1HypopneaEvent::TYPE;
const PRS1ParsedEventType PRS1TotalLeakEvent::TYPE;
const PRS1ParsedEventType PRS1LeakEvent::TYPE;
const PRS1ParsedEventType PRS1AutoPressureSetEvent::TYPE;
const PRS1ParsedEventType PRS1PressureSetEvent::TYPE;
const PRS1ParsedEventType PRS1IPAPSetEvent::TYPE;
const PRS1ParsedEventType PRS1EPAPSetEvent::TYPE;
const PRS1ParsedEventType PRS1PressureAverageEvent::TYPE;
const PRS1ParsedEventType PRS1FlexPressureAverageEvent::TYPE;
const PRS1ParsedEventType PRS1IPAPAverageEvent::TYPE;
const PRS1ParsedEventType PRS1IPAPHighEvent::TYPE;
const PRS1ParsedEventType PRS1IPAPLowEvent::TYPE;
const PRS1ParsedEventType PRS1EPAPAverageEvent::TYPE;
const PRS1ParsedEventType PRS1RespiratoryRateEvent::TYPE;
const PRS1ParsedEventType PRS1PatientTriggeredBreathsEvent::TYPE;
const PRS1ParsedEventType PRS1MinuteVentilationEvent::TYPE;
const PRS1ParsedEventType PRS1SnoreEvent::TYPE;
const PRS1ParsedEventType PRS1VibratorySnoreEvent::TYPE;
const PRS1ParsedEventType PRS1PressurePulseEvent::TYPE;
const PRS1ParsedEventType PRS1RERAEvent::TYPE;
const PRS1ParsedEventType PRS1FlowRateEvent::TYPE;
const PRS1ParsedEventType PRS1Test1Event::TYPE;
const PRS1ParsedEventType PRS1Test2Event::TYPE;
const PRS1ParsedEventType PRS1HypopneaCount::TYPE;
const PRS1ParsedEventType PRS1ClearAirwayCount::TYPE;
const PRS1ParsedEventType PRS1ObstructiveApneaCount::TYPE;
//const PRS1ParsedEventType PRS1DisconnectAlarmEvent::TYPE;
const PRS1ParsedEventType PRS1ApneaAlarmEvent::TYPE;
//const PRS1ParsedEventType PRS1LowMinuteVentilationAlarmEvent::TYPE;


// MARK: Render parsed events as text

static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}

#define ENUMSTRING(ENUM) case ENUM: s = QStringLiteral(#ENUM); break
static QString parsedEventTypeName(PRS1ParsedEventType t)
{
    QString s;
    switch (t) {
        ENUMSTRING(EV_PRS1_RAW);
        ENUMSTRING(EV_PRS1_UNKNOWN);
        ENUMSTRING(EV_PRS1_TB);
        ENUMSTRING(EV_PRS1_OA);
        ENUMSTRING(EV_PRS1_CA);
        ENUMSTRING(EV_PRS1_FL);
        ENUMSTRING(EV_PRS1_PB);
        ENUMSTRING(EV_PRS1_LL);
        ENUMSTRING(EV_PRS1_VB);
        ENUMSTRING(EV_PRS1_HY);
        ENUMSTRING(EV_PRS1_OA_COUNT);
        ENUMSTRING(EV_PRS1_CA_COUNT);
        ENUMSTRING(EV_PRS1_HY_COUNT);
        ENUMSTRING(EV_PRS1_TOTLEAK);
        ENUMSTRING(EV_PRS1_LEAK);
        ENUMSTRING(EV_PRS1_AUTO_PRESSURE_SET);
        ENUMSTRING(EV_PRS1_PRESSURE_SET);
        ENUMSTRING(EV_PRS1_IPAP_SET);
        ENUMSTRING(EV_PRS1_EPAP_SET);
        ENUMSTRING(EV_PRS1_PRESSURE_AVG);
        ENUMSTRING(EV_PRS1_FLEX_PRESSURE_AVG);
        ENUMSTRING(EV_PRS1_IPAP_AVG);
        ENUMSTRING(EV_PRS1_IPAPLOW);
        ENUMSTRING(EV_PRS1_IPAPHIGH);
        ENUMSTRING(EV_PRS1_EPAP_AVG);
        ENUMSTRING(EV_PRS1_RR);
        ENUMSTRING(EV_PRS1_PTB);
        ENUMSTRING(EV_PRS1_MV);
        ENUMSTRING(EV_PRS1_TV);
        ENUMSTRING(EV_PRS1_SNORE);
        ENUMSTRING(EV_PRS1_VS);
        ENUMSTRING(EV_PRS1_PP);
        ENUMSTRING(EV_PRS1_RERA);
        ENUMSTRING(EV_PRS1_FLOWRATE);
        ENUMSTRING(EV_PRS1_TEST1);
        ENUMSTRING(EV_PRS1_TEST2);
        ENUMSTRING(EV_PRS1_SETTING);
        ENUMSTRING(EV_PRS1_SLICE);
        ENUMSTRING(EV_PRS1_DISCONNECT_ALARM);
        ENUMSTRING(EV_PRS1_APNEA_ALARM);
        ENUMSTRING(EV_PRS1_LOW_MV_ALARM);
        ENUMSTRING(EV_PRS1_SNORES_AT_PRESSURE);
        ENUMSTRING(EV_PRS1_INTERVAL_BOUNDARY);
        default:
            s = hex(t);
            qDebug() << "Unknown PRS1ParsedEventType type:" << qPrintable(s);
            return s;
    }
    return s.mid(8).toLower();  // lop off initial EV_PRS1_
}

static QString parsedSettingTypeName(PRS1ParsedSettingType t)
{
    QString s;
    switch (t) {
        ENUMSTRING(PRS1_SETTING_CPAP_MODE);
        ENUMSTRING(PRS1_SETTING_AUTO_TRIAL);
        ENUMSTRING(PRS1_SETTING_PRESSURE);
        ENUMSTRING(PRS1_SETTING_PRESSURE_MIN);
        ENUMSTRING(PRS1_SETTING_PRESSURE_MAX);
        ENUMSTRING(PRS1_SETTING_EPAP);
        ENUMSTRING(PRS1_SETTING_EPAP_MIN);
        ENUMSTRING(PRS1_SETTING_EPAP_MAX);
        ENUMSTRING(PRS1_SETTING_IPAP);
        ENUMSTRING(PRS1_SETTING_IPAP_MIN);
        ENUMSTRING(PRS1_SETTING_IPAP_MAX);
        ENUMSTRING(PRS1_SETTING_PS);
        ENUMSTRING(PRS1_SETTING_PS_MIN);
        ENUMSTRING(PRS1_SETTING_PS_MAX);
        ENUMSTRING(PRS1_SETTING_BACKUP_BREATH_MODE);
        ENUMSTRING(PRS1_SETTING_BACKUP_BREATH_RATE);
        ENUMSTRING(PRS1_SETTING_BACKUP_TIMED_INSPIRATION);
        ENUMSTRING(PRS1_SETTING_TIDAL_VOLUME);
        ENUMSTRING(PRS1_SETTING_EZ_START);
        ENUMSTRING(PRS1_SETTING_FLEX_LOCK);
        ENUMSTRING(PRS1_SETTING_FLEX_MODE);
        ENUMSTRING(PRS1_SETTING_FLEX_LEVEL);
        ENUMSTRING(PRS1_SETTING_RISE_TIME);
        ENUMSTRING(PRS1_SETTING_RISE_TIME_LOCK);
        ENUMSTRING(PRS1_SETTING_RAMP_TYPE);
        ENUMSTRING(PRS1_SETTING_RAMP_TIME);
        ENUMSTRING(PRS1_SETTING_RAMP_PRESSURE);
        ENUMSTRING(PRS1_SETTING_HUMID_STATUS);
        ENUMSTRING(PRS1_SETTING_HUMID_MODE);
        ENUMSTRING(PRS1_SETTING_HEATED_TUBE_TEMP);
        ENUMSTRING(PRS1_SETTING_HUMID_LEVEL);
        ENUMSTRING(PRS1_SETTING_HUMID_TARGET_TIME);
        ENUMSTRING(PRS1_SETTING_MASK_RESIST_LOCK);
        ENUMSTRING(PRS1_SETTING_MASK_RESIST_SETTING);
        ENUMSTRING(PRS1_SETTING_HOSE_DIAMETER);
        ENUMSTRING(PRS1_SETTING_TUBING_LOCK);
        ENUMSTRING(PRS1_SETTING_AUTO_ON);
        ENUMSTRING(PRS1_SETTING_AUTO_OFF);
        ENUMSTRING(PRS1_SETTING_APNEA_ALARM);
        ENUMSTRING(PRS1_SETTING_DISCONNECT_ALARM);
        ENUMSTRING(PRS1_SETTING_LOW_MV_ALARM);
        ENUMSTRING(PRS1_SETTING_LOW_TV_ALARM);
        ENUMSTRING(PRS1_SETTING_MASK_ALERT);
        ENUMSTRING(PRS1_SETTING_SHOW_AHI);
        default:
            s = hex(t);
            qDebug() << "Unknown PRS1ParsedSettingType type:" << qPrintable(s);
            return s;
    }
    return s.mid(13).toLower();  // lop off initial PRS1_SETTING_
}

static QString parsedModeName(int m)
{
    QString s;
    switch ((PRS1Mode) m) {
        ENUMSTRING(PRS1_MODE_UNKNOWN);  // TODO: Remove this when all the parsers are complete.
        ENUMSTRING(PRS1_MODE_CPAP);
        ENUMSTRING(PRS1_MODE_CPAPCHECK);
        ENUMSTRING(PRS1_MODE_AUTOTRIAL);
        ENUMSTRING(PRS1_MODE_AUTOCPAP);
        ENUMSTRING(PRS1_MODE_BILEVEL);
        ENUMSTRING(PRS1_MODE_AUTOBILEVEL);
        ENUMSTRING(PRS1_MODE_ASV);
        ENUMSTRING(PRS1_MODE_S);
        ENUMSTRING(PRS1_MODE_ST);
        ENUMSTRING(PRS1_MODE_PC);
        ENUMSTRING(PRS1_MODE_ST_AVAPS);
        ENUMSTRING(PRS1_MODE_PC_AVAPS);
        default:
            s = hex(m);
            qDebug() << "Unknown PRS1Mode:" << qPrintable(s);
            return s;
    }
    return s.mid(10).toLower();  // lop off initial PRS1_MODE_
}

static QString timeStr(int t)
{
    int h = t / 3600;
    int m = (t - (h * 3600)) / 60;
    int s = t % 60;
#if 1
    // Optimized after profiling regression tests.
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
#else
    // Unoptimized original, slows down regression tests.
    return QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
#endif
}

static QString byteList(QByteArray data, int limit=-1)
{
    int count = data.size();
    if (limit == -1 || limit > count) limit = count;
    QStringList l;
    for (int i = 0; i < limit; i++) {
        l.push_back(QString( "%1" ).arg((int) data[i] & 0xFF, 2, 16, QChar('0') ).toUpper());
    }
    if (limit < count) l.push_back("...");
    QString s = l.join(" ");
    return s;
}

QString _PRS1ParsedEventName(PRS1ParsedEvent* e)
{
    return parsedEventTypeName(e->m_type);
}

QMap<QString,QString> _PRS1ParsedEventContents(PRS1ParsedEvent* e)
{
    return e->contents();
}


QMap<QString,QString> PRS1IntervalBoundaryEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    return out;
}


QMap<QString,QString> PRS1ParsedDurationEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    out["duration"] = timeStr(m_duration);
    return out;
}


QMap<QString,QString> PRS1ParsedValueEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    out["value"] = QString::number(value());
    return out;
}


QMap<QString,QString> PRS1UnknownDataEvent::contents(void)
{
    QMap<QString,QString> out;
    out["pos"] = QString::number(m_pos);
    out["data"] = byteList(m_data);
    return out;
}


QMap<QString,QString> PRS1ParsedSettingEvent::contents(void)
{
    QMap<QString,QString> out;
    QString v;
    if (m_setting == PRS1_SETTING_CPAP_MODE) {
        v = parsedModeName(value());
    } else {
        v = QString::number(value());
    }
    out[parsedSettingTypeName(m_setting)] = v;
    return out;
}


QMap<QString,QString> PRS1ParsedSliceEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    QString s;
    switch ((SliceStatus) m_value) {
        case MaskOn: s = "MaskOn"; break;
        case MaskOff: s = "MaskOff"; break;
        case EquipmentOff: s = "EquipmentOff"; break;
        case UnknownStatus: s = "Unknown"; break;
    }
    out["status"] = s;
    return out;
}


QMap<QString,QString> PRS1ParsedAlarmEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    return out;
}


QMap<QString,QString> PRS1SnoresAtPressureEvent::contents(void)
{
    QString label;
    switch (m_kind) {
        case 0: label = "pressure"; break;
        case 1: label = "epap"; break;
        case 2: label = "ipap"; break;
        default: label = "unknown_pressure"; break;
    }
    
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    out[label] = QString::number(value());
    out["count"] = QString::number(m_count);
    return out;
}
