/* SleepLib PRS1 Loader Parser Implementation
 *
 * Copyright (c) 2019-2021 The OSCAR Team
 * Portions copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#include "prs1_parser.h"
#include "prs1_loader.h"
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


//********************************************************************************************
// MARK: Render parsed events as text

static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}

#define ENUMSTRING(ENUM) case ENUM: s = QStringLiteral(#ENUM); break
QString PRS1ParsedEvent::typeName() const
{
    PRS1ParsedEventType t = m_type;
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

QString PRS1ParsedSettingEvent::settingName() const
{
    PRS1ParsedSettingType t = m_setting;
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

QString PRS1ParsedSettingEvent::modeName() const
{
    int m = value();
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

QString PRS1ParsedEvent::timeStr(int t)
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
        v = modeName();
    } else {
        v = QString::number(value());
    }
    out[settingName()] = v;
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


//********************************************************************************************
// MARK: -
// MARK: Chunk parsing

bool PRS1DataChunk::ParseCompliance(void)
{
    switch (this->family) {
    case 0:
        switch (this->familyVersion) {
        case 2:
        case 3:
            return this->ParseComplianceF0V23();
        case 4:
            return this->ParseComplianceF0V4();
        case 5:
            return this->ParseComplianceF0V5();
        case 6:
            return this->ParseComplianceF0V6();
        }
    default:
        ;
    }

    qWarning() << "unexpected compliance family" << this->family << "familyVersion" << this->familyVersion;
    return false;
}


bool PRS1DataChunk::ParseSummary()
{
    switch (this->family) {
    case 0:
        if (this->familyVersion == 6) {
            return this->ParseSummaryF0V6();
        } else if (this->familyVersion == 4) {
            return this->ParseSummaryF0V4();
        } else {
            return this->ParseSummaryF0V23();
        }
    case 3:
        switch (this->familyVersion) {
            case 0: return this->ParseSummaryF3V03();
            case 3: return this->ParseSummaryF3V03();
            case 6: return this->ParseSummaryF3V6();
        }
        break;
    case 5:
        if (this->familyVersion == 1) {
            return this->ParseSummaryF5V012();
        } else if (this->familyVersion == 0) {
            return this->ParseSummaryF5V012();
        } else if (this->familyVersion == 2) {
            return this->ParseSummaryF5V012();
        } else if (this->familyVersion == 3) {
            return this->ParseSummaryF5V3();
        }
    default:
        ;
    }

    qWarning() << "unexpected family" << this->family << "familyVersion" << this->familyVersion;
    return false;
}


// TODO: The nested switch statement below just begs for per-version subclasses.
bool PRS1DataChunk::ParseEvents()
{
    bool ok = false;
    switch (this->family) {
        case 0:
            switch (this->familyVersion) {
                case 2: ok = this->ParseEventsF0V23(); break;
                case 3: ok = this->ParseEventsF0V23(); break;
                case 4: ok = this->ParseEventsF0V4(); break;
                case 6: ok = this->ParseEventsF0V6(); break;
            }
            break;
        case 3:
            switch (this->familyVersion) {
                case 0: ok = this->ParseEventsF3V03(); break;
                case 3: ok = this->ParseEventsF3V03(); break;
                case 6: ok = this->ParseEventsF3V6(); break;
            }
            break;
        case 5:
            switch (this->familyVersion) {
                case 0: ok = this->ParseEventsF5V0(); break;
                case 1: ok = this->ParseEventsF5V1(); break;
                case 2: ok = this->ParseEventsF5V2(); break;
                case 3: ok = this->ParseEventsF5V3(); break;
            }
            break;
        default:
            qDebug() << "Unknown PRS1 family" << this->family << "familyVersion" << this->familyVersion;
    }
    return ok;
}


// TODO: This really should be in some kind of class hierarchy, once we figure out
// the right one.
const QVector<PRS1ParsedEventType> & GetSupportedEvents(const PRS1DataChunk* chunk)
{
    static const QVector<PRS1ParsedEventType> none;
    
    switch (chunk->family) {
        case 0:
            switch (chunk->familyVersion) {
                case 2: return ParsedEventsF0V23; break;
                case 3: return ParsedEventsF0V23; break;
                case 4: return ParsedEventsF0V4; break;
                case 6: return ParsedEventsF0V6; break;
            }
            break;
        case 3:
            switch (chunk->familyVersion) {
                case 0: return ParsedEventsF3V0; break;
                case 3: return ParsedEventsF3V3; break;
                case 6: return ParsedEventsF3V6; break;
            }
            break;
        case 5:
            switch (chunk->familyVersion) {
                case 0: return ParsedEventsF5V0; break;
                case 1: return ParsedEventsF5V1; break;
                case 2: return ParsedEventsF5V2; break;
                case 3: return ParsedEventsF5V3; break;
            }
            break;
    }
    qWarning() << "Missing supported event list for family" << chunk->family << "version" << chunk->familyVersion;
    return none;
}


QString PRS1DataChunk::DumpEvent(int t, int code, const unsigned char* data, int size)
{
    int s = t;
    int h = s / 3600; s -= h * 3600;
    int m = s / 60; s -= m * 60;
    QString dump = QString("%1:%2:%3 ")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
    dump = dump + " " + hex(code) + ":";
    for (int i = 0; i < size; i++) {
        dump = dump + QString(" %1").arg(data[i]);
    }
    return dump;
}


void PRS1DataChunk::AddEvent(PRS1ParsedEvent* const event)
{
    m_parsedData.push_back(event);
}



//********************************************************************************************
// MARK: -
// MARK: Parse settings shared by multiple families

// Humid F0V2 confirmed
// 0x00 = Off (presumably no humidifier present)
// 0x80 = Off
// 0x81 = 1
// 0x82 = 2
// 0x83 = 3
// 0x84 = 4
// 0x85 = 5

// Humid F3V0 confirmed
// 0x03 = 3 (but no humidification shown on hours of usage chart)
// 0x04 = 4 (but no humidification shown on hours of usage chart)
// 0x80 = Off
// 0x81 = 1
// 0x82 = 2
// 0x83 = 3
// 0x84 = 4
// 0x85 = 5

// Humid F5V0 confirmed
// 0x00 = Off (presumably no humidifier present)
// 0x80 = Off
// 0x81 = 1, bypass = no
// 0x82 = 2, bypass = no
// 0x83 = 3, bypass = no
// 0x84 = 4, bypass = no
// 0x85 = 5, bypass = no
// 0xA0 = Off, bypass = yes

void PRS1DataChunk::ParseHumidifierSetting50Series(int humid, bool add_setting)
{
    if (humid & (0x40 | 0x10 | 0x08)) UNEXPECTED_VALUE(humid, "known bits");
    if (humid & 0x20) {
        if (this->family == 5) {
            CHECK_VALUE(humid, 0xA0);  // only example of bypass set, unsure whether it can appear otherwise
        } else {
            CHECK_VALUE(humid & 0x20, 0);  // only ever seen on 950P, where "Bypass System One humidification" is "Yes"
        }
    }
    
    bool humidifier_present = ((humid & 0x80) != 0);  // humidifier connected
    int humidlevel = humid & 7;  // humidification level

    HumidMode humidmode = HUMID_Fixed;  // 50-Series didn't have adaptive or heated tube humidification
    if (add_setting) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_STATUS, humidifier_present));
        if (humidifier_present) {
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_MODE, humidmode));
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, humidlevel));
        }
    }

    // Check for truly unexpected values:
    if (humidlevel > 5) UNEXPECTED_VALUE(humidlevel, "<= 5");
    //if (!humidifier_present) CHECK_VALUES(humidlevel, 0, 1);  // Some machines appear to encode the humidlevel setting even when the humidifier is not present.
}




// F0V4 confirmed:
// B3 0A = HT=5, H=3, HT
// A3 0A = HT=5, H=2, HT
// 33 0A = HT=4, H=3, HT
// 23 4A = HT=4, H=2, HT
// B3 09 = HT=3, H=3, HT
// A4 09 = HT=3, H=2, HT
// A3 49 = HT=3, H=2, HT
// 22 09 = HT=2, H=2, HT
// 33 09 = HT=2, H=3, HT
// 21 09 = HT=2, H=2, HT
// 13 09 = HT=2, H=1, HT
// B5 08 = HT=1, H=3, HT
// 03 08 = HT=off,    HT; data=tube t=0,h=0
// 05 24 =       H=5, S1
// 95 06 =       H=5, S1
// 95 05 =       H=5, S1
// 94 05 =       H=4, S1
// 04 24 =       H=4, S1
// A3 05 =       H=3, S1
// 92 05 =       H=2, S1
// A2 05 =       H=2, S1
// 01 24 =       H=1, S1
// 90 05 =       H=off, S1
// 30 05 =       H=off, S1
// 95 41 =       H=5, Classic
// A4 61 =       H=4, Classic
// A3 61 =       H=3, Classic
// A2 61 =       H=2, Classic
// A1 61 =       H=1, Classic
// 90 41 =       H=Off, Classic; data=classic h=0
// 94 11 =       H=3, S1, no data [note that bits encode H=4, so no data falls back to H=3]
// 93 11 =       H=3, S1, no data
// 04 30 =       H=3, S1, no data

// F0V5 confirmed:
// 00 60 =       H=Off, Classic
// 02 60 =       H=2, Classic
// 05 60 =       H=5, Classic
// 00 70 =       H=Off, no data in chart

// F5V1 confirmed:
// A0 4A = HT=5, H=2, HT
// B1 09 = HT=3, H=3, HT
// 91 09 = HT=3, H=1, HT
// 32 09 = HT=2, H=3, HT
// B2 08 = HT=1, H=3, HT
// 00 48 = HT=off, data=tube t=0,h=0
// 95 05 =       H=5, S1
// 94 05 =       H=4, S1
// 93 05 =       H=3, S1
// 92 05 =       H=2, S1
// 91 05 =       H=1, S1
// 90 05 =       H=Off, S1
// 95 41 =       H=5, Classic
// 94 41 =       H=4, Classic
// 93 41 =       H=3, Classic
// 92 41 =       H=2, Classic
// 01 60 =       H=1, Classic
// 00 60 =       H=Off, Classic
// 00 70 =       H=3, S1, no data [no data ignores Classic mode, H bits, falls back to S1 H=3]

// F5V2 confirmed:
// 00 48 = HT=off, data=tube t=0,h=0
// 93 09 = HT=3, H=1, HT
// 00 10 =       H=3, S1, no data

// XX XX = 60-Series Humidifier bytes
//  7    = humidity level without tube [on tube disconnect / system one with 22mm hose / classic]  :  0 = humidifier off
//  8    = [never seen]
// 3     = humidity level with tube
// 4     = maybe part of humidity level? [never seen]
// 8   3 = tube temperature (high bit of humid 1 is low bit of temp)
//     4 = "System One" mode (valid even when humidifier is off)
//     8 = heated tube present
//    10 = no data in chart, maybe no humidifier attached? Seems to fall back on System One = 3 despite other (humidity level and S1) bits.
//    20 = unknown, something tube related since whenever it's set tubepresent is false
//    40 = "Classic" mode (valid even when humidifier is off, ignored when heated tube is present)
//    80 = [never seen]

void PRS1DataChunk::ParseHumidifierSetting60Series(unsigned char humid1, unsigned char humid2, bool add_setting)
{
    int humidlevel = humid1 & 7;  // Ignored when heated tube is present: humidifier setting on tube disconnect is always reported as 3
    if (humidlevel > 5) UNEXPECTED_VALUE(humidlevel, "<= 5");
    CHECK_VALUE(humid1 & 8, 0);  // never seen
    int tubehumidlevel = (humid1 >> 4) & 7;  // This mask is a best guess based on other masks.
    if (tubehumidlevel > 5) UNEXPECTED_VALUE(tubehumidlevel, "<= 5");
    CHECK_VALUE(tubehumidlevel & 4, 0);  // never seen, but would clarify whether above mask is correct

    int tubetemp = (humid1 >> 7) | ((humid2 & 3) << 1);
    if (tubetemp > 5) UNEXPECTED_VALUE(tubetemp, "<= 5");

    CHECK_VALUE(humid2 & 0x80, 0);  // never seen
    bool humidclassic = (humid2 & 0x40) != 0;  // Set on classic mode reports; evidently ignored (sometimes set!) when tube is present
    //bool no_tube? = (humid2 & 0x20) != 0;  // Something tube related: whenever it is set, tube is never present (inverse is not true)
    bool no_data = (humid2 & 0x10) != 0;  // As described in chart, settings still show up
    int tubepresent = (humid2 & 0x08) != 0;
    bool humidsystemone = (humid2 & 0x04) != 0;  // Set on "System One" humidification mode reports when tubepresent is false
    if (humidsystemone && tubepresent) {
        // On a 560P, we've observed a spurious tubepresent bit being set during two sessions.
        // Those sessions (and the ones that followed) used a 22mm hose.
        CHECK_VALUE(add_setting, false);  // We've only seen this appear during a session, not in the initial settings.
        tubepresent = false;
    }

    // When no_data, reports always say "System One" with humidity level 3, regardless of humidlevel and humidsystemone

    if (humidsystemone + tubepresent + no_data == 0) CHECK_VALUE(humidclassic, true);  // Always set when everything else is off
    if (humidsystemone + tubepresent + no_data > 1) UNEXPECTED_VALUE(humid2, "one bit set");  // Only one of these ever seems to be set at a time
    if (tubepresent && tubetemp == 0) CHECK_VALUE(tubehumidlevel, 0);  // When the heated tube is off, tube humidity seems to be 0
    
    if (tubepresent) humidclassic = false;  // Classic mode bit is evidently ignored when tube is present
    if (no_data) humidclassic = false;  // Classic mode bit is evidently ignored when tube is present

    //qWarning() << this->sessionid << (humidclassic ? "C" : ".") << (humid2 & 0x20 ? "?" : ".") << (tubepresent ? "T" : ".") << (no_data ? "X" : ".") << (humidsystemone ? "1" : ".");
    /*
    if (tubepresent) {
        if (tubetemp) {
            qWarning() << this->sessionid << "tube temp" << tubetemp << "tube humidity" << tubehumidlevel << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
        } else {
            qWarning() << this->sessionid << "heated tube off" << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
        }
    } else {
        qWarning() << this->sessionid << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
    }
    */
    HumidMode humidmode = HUMID_Fixed;
    if (tubepresent) {
        humidmode = HUMID_HeatedTube;
    } else {
        if (humidsystemone + humidclassic > 1) UNEXPECTED_VALUE(humid2, "fixed or adaptive");
        if (humidsystemone) humidmode = HUMID_Adaptive;
    }

    if (add_setting) {
        bool humidifier_present = (no_data == 0);
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_STATUS, humidifier_present));
        if (humidifier_present) {
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_MODE, humidmode));
            if (humidmode == HUMID_HeatedTube) {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HEATED_TUBE_TEMP, tubetemp));
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, tubehumidlevel));
            } else {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, humidlevel));
            }
        }
    }

    // Check for previously unseen data that we expect to be normal:
    if (this->family == 0) {
        // F0V4
        if (tubetemp && (tubehumidlevel < 1 || tubehumidlevel > 3)) UNEXPECTED_VALUE(tubehumidlevel, "1-3");
    } else if (this->familyVersion == 1) {
        // F5V1
        if (tubepresent) {
            // all tube temperatures seen
            if (tubetemp) {
                if (tubehumidlevel == 0 || tubehumidlevel > 3) UNEXPECTED_VALUE(tubehumidlevel, "1-3");
            }
        }
    } else if (this->familyVersion == 2) {
        // F5V2
        if (tubepresent) {
            CHECK_VALUES(tubetemp, 0, 3);
            if (tubetemp) {
                CHECK_VALUE(tubehumidlevel, 1);
            }
        }
        CHECK_VALUE(humidsystemone, false);
        CHECK_VALUE(humidclassic, false);
    }
}


// F0V6 confirmed
// 90 B0 = HT=3!,H=3!,data=none [no humidifier appears to ignore HT and H bits and show HT=3,H=3 in details]
// 8C 6C = HT=3, H=3, data=none
// 80 00 = nothing listed in details, data=none, only seen on 400G and 502G
// 54 B4 = HT=5, H=5, data=tube
// 50 90 = HT=4, H=4, data=tube
// 4C 6C = HT=3, H=3, data=tube
// 48 68 = HT=3, H=2, data=tube
// 40 60 = HT=3, H=Off, data=tube t=3,h=0
// 50 50 = HT=2, H=4, data=tube
// 4C 4C = HT=2, H=3, data=tube
// 50 30 = HT=1, H=4, data=tube
// 4C 0C = HT=off, H=3, data=tube t=0,h=3
// 34 74 = HT=3, H=5, data=adaptive (5)
// 50 B0 = HT=5, H=4, adaptive
// 30 B0 = HT=3, H=4, data=adaptive (4)
// 30 50 = HT=3, H=4, data=adaptive (4)
// 30 10 = HT=3!,H=4, data=adaptive (4) [adaptive mode appears to ignore HT bits and show HT=3 in details]
// 30 70 = HT=3, H=4, data=adaptive (4)
// 2C 6C = HT=3, H=3, data=adaptive (3)
// 28 08 =       H=2, data=adaptive (2), no details (400G)
// 28 48 = HT=3!,H=2, data=adaptive (2) [adaptive mode appears to ignore HT bits and show HT=3 in details]
// 28 68 = HT=3, H=2, data=adaptive (2)
// 24 64 = HT=3, H=1, data=adaptive (1)
// 20 60 = HT=3, H=off, data=adaptive (0)
// 14 74 = HT=3, H=5, data=fixed (5)
// 10 70 = HT=3, H=4, data=fixed (4)
// 0C 6C = HT=3, H=3, data=fixed (3)
// 08 48 = HT=3, H=2, data=fixed (2)
// 08 68 = HT=3, H=2, data=fixed (2)
// 04 64 = HT=3, H=1, data=fixed (1)
// 00 00 = HT=3, H=off, data=fixed (0)

// F5V3 confirmed:
// 90 70 = HT=3, H=3, adaptive, data=no data
// 54 14 = HT=Off, H=5, adaptive, data=tube t=0,h=5
// 54 34 = HT=1, H=5, adaptive, data=tube t=1,h=5
// 50 70 = HT=3, H=4, adaptive, data=tube t=3,h=4
// 4C 6C = HT=3, H=3, adaptive, data=tube t=3,h=3
// 4C 4C = HT=2, H=3, adaptive, data=tube t=2,h=3
// 4C 2C = HT=1, H=3, adaptive, data=tube t=1,h=3
// 4C 0C = HT=off, H=3, adaptive, data=tube t=0,h=3
// 48 08 = HT=off, H=2, adaptive, data=tube t=0,h=2
// 44 04 = HT=off, H=1, adaptive, data=tube t=0,h=1
// 40 00 = HT=off,H=off, adaptive, data=tube t=0,h=0
// 34 74 = HT=3, H=5, adaptive, data=s1 (5)
// 30 70 = HT=3, H=4, adaptive, data=s1 (4)
// 2C 6C = HT=3, H=3, adaptive, data=s1 (3)
// 28 68 = HT=3, H=2, adaptive, data=s1 (2)
// 24 64 = HT=3, H=1, adaptive, data=s1 (1)

// F3V6 confirmed:
// 84 24 = HT=3, H=3, disconnect=adaptive, data=no data
// 50 90 = HT=4, H=4, disconnect=adaptive, data=tube t=4,h=4
// 44 84 = HT=4, H=1, disconnect=adaptive, data=tube t=4,h=1
// 40 80 = HT=4, H=Off,disconnect=adaptive, data=tube t=4,h=0
// 4C 6C = HT=3, H=3, disconnect=adaptive, data=tube t=3,h=3
// 48 68 = HT=3, H=2, disconnect=adaptive, data=tube t=3,h=2
// 44 44 = HT=2, H=1, disconnect=adaptive, data=tube t=2,h=1
// 48 28 = HT=1, H=2, disconnect=adaptive, data=tube t=1,h=2
// 54 14 = HT=Off,H=5, disconnect=adaptive data=tube t=0,h=5
// 34 14 = HT=3, H=5, disconnect=adaptive, data=s1 (5)
// 30 70 = HT=3, H=4, disconnect=adaptive, data=s1 (4)
// 2C 6C = HT=3, H=3, disconnect=adaptive, data=s1 (3)
// 28 08 = HT=3, H=2, disconnect=adaptive, data=s1 (2)
// 20 20 = HT=3, H=Off, disconnect=adaptive, data=s1 (0)
// 14 14 = HT=3, H=3, disconnect=fixed, data=classic (5)
// 10 10 = HT=3, H=4, disconnect=fixed, data=classic (4) [fixed mode appears to ignore HT bits and show HT=3 in details]
// 0C 0C = HT=3, H=3, disconnect=fixed, data=classic (3)
// 08 08 = HT=3, H=2, disconnect=fixed, data=classic (2)
// 04 64 = HT=3, H=1, disconnect=fixed, data=classic (1)

// The data is consistent among all fileVersion 3 models: F0V6, F5V3, F3V6.
//
// NOTE: F5V3 and F3V6 charts report the "Adaptive" setting as "System One" and the "Fixed"
// setting as "Classic", despite labeling the settings "Adaptive" and "Fixed" just like F0V6.
// F0V6 is consistent and labels both settings and chart as "Adaptive" and "Fixed".
//
// 400G and 502G appear to omit the humidifier settings in their details, though they
// do support humidifiers, and will show the humidification in the charts.

void PRS1DataChunk::ParseHumidifierSettingV3(unsigned char byte1, unsigned char byte2, bool add_setting)
{
    bool humidifier_present = true;
    bool humidfixed = false;  // formerly called "Classic"
    bool humidadaptive = false;  // formerly called "System One"
    bool tubepresent = false;
    bool passover = false;
    bool error = false;

    // Byte 1: 0x90 (no humidifier data), 0x50 (15ht, tube 4/5, humid 4), 0x54 (15ht, tube 5, humid 5) 0x4c (15ht, tube temp 3, humidifier 3)
    // 0x0c (15, tube 3, humid 3, fixed)
    // 0b1001 0000 no humidifier data
    // 0b0101 0000 tube 4 and 5, humidifier 4
    // 0b0101 0100 15ht, tube 5, humidifier 5
    // 0b0100 1100 15ht, tube 3, humidifier 3
    // 0b1011 0000 15, tube 3, humidifier 3, "Error" on humidification chart with asterisk at 4
    // 0b0111 0000 15, tube 3, humidifier 3, "Passover" on humidification chart with notch at 4
    //   842       = humidifier status
    //      1 84   = humidifier setting
    //          ??
    CHECK_VALUE(byte1 & 3, 0);
    int humid = byte1 >> 5;
    switch (humid) {
    case 0: humidfixed = true; break;  // fixed, ignores tubetemp bits and reports tubetemp=3
    case 1: humidadaptive = true; break;  // adaptive, ignores tubetemp bits and reports tubetemp=3
    case 2: tubepresent = true; break;  // heated tube
    case 3: passover = true; break;  // passover mode (only visible in chart)
    case 4: humidifier_present = false; break;  // no humidifier, reports tubetemp=3 and humidlevel=3
    case 5: error = true; break;  // "Error" in humidification chart, reports tubetemp=3 and humidlevel=3 in settings
    default:
        UNEXPECTED_VALUE(humid, "known value");
        break;
    }
    int humidlevel = (byte1 >> 2) & 7;

    // Byte 2: 0xB4 (15ht, tube 5, humid 5), 0xB0 (15ht, tube 5, humid 4), 0x90 (tube 4, humid 4), 0x6C (15ht, tube temp 3, humidifier 3)
    // 0x80?
    // 0b1011 0100 15ht, tube 5, humidifier 5
    // 0b1011 0000 15ht, tube 5, humidifier 4
    // 0b1001 0000 tube 4, humidifier 4
    // 0b0110 1100 15ht, tube 3, humidifier 3
    //   842       = tube temperature
    //      1 84   = humidity level when using heated tube, thus far always identical to humidlevel
    //          ??
    CHECK_VALUE(byte2 & 3, 0);
    int tubehumidlevel = (byte2 >> 2) & 7;
    CHECK_VALUE(humidlevel, tubehumidlevel);  // thus far always the same
    int tubetemp = (byte2 >> 5) & 7;
    if (humidifier_present) {
        if (humidlevel > 5 || humidlevel < 0) UNEXPECTED_VALUE(humidlevel, "0-5");  // 0=off is valid when a humidifier is attached
        if (humid == 2) {  // heated tube
            if (tubetemp > 5 || tubetemp < 0) UNEXPECTED_VALUE(tubetemp, "0-5");  // TODO: maybe this is only if heated tube? 0=off is valid even in heated tube mode
        }
    }

    // TODO: move this up into the switch statement above, given how many modes there now are.
    HumidMode humidmode = HUMID_Fixed;
    if (tubepresent) {
        humidmode = HUMID_HeatedTube;
    } else if (humidadaptive) {
        humidmode = HUMID_Adaptive;
    } else if (passover) {
        humidmode = HUMID_Passover;
    } else if (error) {
        humidmode = HUMID_Error;
    }

    if (add_setting) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_STATUS, humidifier_present));
        if (humidifier_present) {
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_MODE, humidmode));
            if (humidmode == HUMID_HeatedTube) {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HEATED_TUBE_TEMP, tubetemp));
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, tubehumidlevel));
            } else {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, humidlevel));
            }
        }
    }

    // Check for previously unseen data that we expect to be normal:
    if (family == 0) {
        // All variations seen.
    } else if (family == 5) {
        if (tubepresent) {
            // All tube temperature and humidity levels seen.
        } else if (humidadaptive) {
            // All humidity levels seen.
        } else if (humidfixed) {
            if (humidlevel < 3) UNEXPECTED_VALUE(humidlevel, "3-5");
        }
    } else if (family == 3) {
        if (tubepresent) {
            // All tube temperature and humidity levels seen.
        } else if (humidadaptive) {
            // All humidity levels seen.
        } else if (humidfixed) {
            // All humidity levels seen.
        }
    }
}


void PRS1DataChunk::ParseTubingTypeV3(unsigned char type)
{
    int diam;
    switch (type) {
    case 0: diam = 22; break;
    case 1: diam = 15; break;
    case 2: diam = 15; break;  // 15HT, though the reports only say "15" for DreamStation models
    case 3: diam = 12; break;  // seen on DreamStation Go models
    default:
        UNEXPECTED_VALUE(type, "known tubing type");
        return;
    }
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HOSE_DIAMETER, diam));
}
