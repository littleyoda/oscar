/* SleepLib PRS1 Loader Parser Header
*
* Copyright (c) 2019-2021 The OSCAR Team
* Portions copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
*
* This file is subject to the terms and conditions of the GNU General Public
* License. See the file COPYING in the main directory of the source code
* for more details. */

#ifndef PRS1PARSER_H
#define PRS1PARSER_H

#include <QMap>
#include <QString>
#include "SleepLib/session.h"

//********************************************************************************************
// Internal PRS1 parsed data types
//********************************************************************************************

// For new events, add an enum here and then a class below with an PRS1_*_EVENT macro
enum PRS1ParsedEventType
{
    EV_PRS1_RAW = -1,     // these only get logged
    EV_PRS1_UNKNOWN = 0,  // these have their value graphed
    EV_PRS1_TB,
    EV_PRS1_OA,
    EV_PRS1_CA,
    EV_PRS1_FL,
    EV_PRS1_PB,
    EV_PRS1_LL,
    EV_PRS1_VB,           // UNCONFIRMED
    EV_PRS1_HY,
    EV_PRS1_OA_COUNT,     // F3V3 only
    EV_PRS1_CA_COUNT,     // F3V3 only
    EV_PRS1_HY_COUNT,     // F3V3 only
    EV_PRS1_TOTLEAK,
    EV_PRS1_LEAK,  // unintentional leak
    EV_PRS1_AUTO_PRESSURE_SET,
    EV_PRS1_PRESSURE_SET,
    EV_PRS1_IPAP_SET,
    EV_PRS1_EPAP_SET,
    EV_PRS1_PRESSURE_AVG,
    EV_PRS1_FLEX_PRESSURE_AVG,
    EV_PRS1_IPAP_AVG,
    EV_PRS1_IPAPLOW,
    EV_PRS1_IPAPHIGH,
    EV_PRS1_EPAP_AVG,
    EV_PRS1_RR,
    EV_PRS1_PTB,
    EV_PRS1_MV,
    EV_PRS1_TV,
    EV_PRS1_SNORE,
    EV_PRS1_VS,
    EV_PRS1_PP,
    EV_PRS1_RERA,
    EV_PRS1_FLOWRATE,
    EV_PRS1_TEST1,
    EV_PRS1_TEST2,
    EV_PRS1_SETTING,
    EV_PRS1_SLICE,
    EV_PRS1_DISCONNECT_ALARM,
    EV_PRS1_APNEA_ALARM,
    EV_PRS1_LOW_MV_ALARM,
    EV_PRS1_SNORES_AT_PRESSURE,
    EV_PRS1_INTERVAL_BOUNDARY,  // An artificial internal-only event used to separate stat intervals
};

enum PRS1ParsedEventUnit
{
    PRS1_UNIT_NONE,
    PRS1_UNIT_CMH2O,
    PRS1_UNIT_ML,
    PRS1_UNIT_S,
};

enum PRS1ParsedSettingType
{
    PRS1_SETTING_CPAP_MODE,
    PRS1_SETTING_AUTO_TRIAL,
    PRS1_SETTING_PRESSURE,
    PRS1_SETTING_PRESSURE_MIN,
    PRS1_SETTING_PRESSURE_MAX,
    PRS1_SETTING_EPAP,
    PRS1_SETTING_EPAP_MIN,
    PRS1_SETTING_EPAP_MAX,
    PRS1_SETTING_IPAP,
    PRS1_SETTING_IPAP_MIN,
    PRS1_SETTING_IPAP_MAX,
    PRS1_SETTING_PS,
    PRS1_SETTING_PS_MIN,
    PRS1_SETTING_PS_MAX,
    PRS1_SETTING_BACKUP_BREATH_MODE,
    PRS1_SETTING_BACKUP_BREATH_RATE,
    PRS1_SETTING_BACKUP_TIMED_INSPIRATION,
    PRS1_SETTING_TIDAL_VOLUME,
    PRS1_SETTING_EZ_START,
    PRS1_SETTING_FLEX_LOCK,
    PRS1_SETTING_FLEX_MODE,
    PRS1_SETTING_FLEX_LEVEL,
    PRS1_SETTING_RISE_TIME,
    PRS1_SETTING_RISE_TIME_LOCK,
    PRS1_SETTING_RAMP_TYPE,
    PRS1_SETTING_RAMP_TIME,
    PRS1_SETTING_RAMP_PRESSURE,
    PRS1_SETTING_HUMID_STATUS,
    PRS1_SETTING_HUMID_MODE,
    PRS1_SETTING_HEATED_TUBE_TEMP,
    PRS1_SETTING_HUMID_LEVEL,
    PRS1_SETTING_MASK_RESIST_LOCK,
    PRS1_SETTING_MASK_RESIST_SETTING,
    PRS1_SETTING_HOSE_DIAMETER,
    PRS1_SETTING_TUBING_LOCK,
    PRS1_SETTING_AUTO_ON,
    PRS1_SETTING_AUTO_OFF,
    PRS1_SETTING_APNEA_ALARM,
    PRS1_SETTING_DISCONNECT_ALARM,  // Is this any different from mask alert?
    PRS1_SETTING_LOW_MV_ALARM,
    PRS1_SETTING_LOW_TV_ALARM,
    PRS1_SETTING_MASK_ALERT,
    PRS1_SETTING_SHOW_AHI,
    PRS1_SETTING_HUMID_TARGET_TIME,
};


class PRS1ParsedEvent
{
public:
    PRS1ParsedEventType m_type;
    int m_start;     // seconds relative to chunk timestamp at which this event began
    int m_duration;
    int m_value;
    float m_offset;
    float m_gain;
    PRS1ParsedEventUnit m_unit;

    inline float value(void) { return (m_value * m_gain) + m_offset; }
    
    static const PRS1ParsedEventType TYPE = EV_PRS1_UNKNOWN;
    static constexpr float GAIN = 1.0;
    static const PRS1ParsedEventUnit UNIT = PRS1_UNIT_NONE;
    
    virtual QMap<QString,QString> contents(void) = 0;

protected:
    PRS1ParsedEvent(PRS1ParsedEventType type, int start)
    : m_type(type), m_start(start), m_duration(0), m_value(0), m_offset(0.0), m_gain(GAIN), m_unit(UNIT)
    {
    }
public:
    virtual ~PRS1ParsedEvent()
    {
    }
};


class PRS1IntervalBoundaryEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

    static const PRS1ParsedEventType TYPE = EV_PRS1_INTERVAL_BOUNDARY;

    PRS1IntervalBoundaryEvent(int start) : PRS1ParsedEvent(TYPE, start) {}
};


class PRS1ParsedDurationEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

    static const PRS1ParsedEventUnit UNIT = PRS1_UNIT_S;
    
    PRS1ParsedDurationEvent(PRS1ParsedEventType type, int start, int duration) : PRS1ParsedEvent(type, start) { m_duration = duration; }
};


class PRS1ParsedValueEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

protected:
    PRS1ParsedValueEvent(PRS1ParsedEventType type, int start, int value) : PRS1ParsedEvent(type, start) { m_value = value; }
};

/*
class PRS1UnknownValueEvent : public PRS1ParsedValueEvent
{
public:
    virtual QMap<QString,QString> contents(void)
    {
        QMap<QString,QString> out;
        out["start"] = timeStr(m_start);
        out["code"] = hex(m_code);
        out["value"] = QString::number(value());
        return out;
    }

    int m_code;
    PRS1UnknownValueEvent(int code, int start, int value, float gain=1.0) : PRS1ParsedValueEvent(TYPE, start, value), m_code(code) { m_gain = gain; }
};
*/

class PRS1UnknownDataEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

    static const PRS1ParsedEventType TYPE = EV_PRS1_RAW;
    
    int m_pos;
    unsigned char m_code;
    QByteArray m_data;
    
    PRS1UnknownDataEvent(const QByteArray & data, int pos, int len=18)
        : PRS1ParsedEvent(TYPE, 0)
    {
        m_pos = pos;
        m_data = data.mid(pos, len);
        Q_ASSERT(m_data.size() >= 1);
        m_code = m_data.at(0);
    }
};

class PRS1PressureEvent : public PRS1ParsedValueEvent
{
public:
    static constexpr float GAIN = 0.1;
    static const PRS1ParsedEventUnit UNIT = PRS1_UNIT_CMH2O;
    
    PRS1PressureEvent(PRS1ParsedEventType type, int start, int value, float gain=GAIN)
        : PRS1ParsedValueEvent(type, start, value)
    {
        m_gain = gain;
        m_unit = UNIT;
    }
};

class PRS1TidalVolumeEvent : public PRS1ParsedValueEvent
{
public:
    static const PRS1ParsedEventType TYPE = EV_PRS1_TV;

    static constexpr float GAIN = 10.0;
    static const PRS1ParsedEventUnit UNIT = PRS1_UNIT_ML;
    
    PRS1TidalVolumeEvent(int start, int value)
        : PRS1ParsedValueEvent(TYPE, start, value)
    {
        m_gain = GAIN;
        m_unit = UNIT;
    }
};

class PRS1ParsedSettingEvent : public PRS1ParsedValueEvent
{
public:
    virtual QMap<QString,QString> contents(void);
    
    static const PRS1ParsedEventType TYPE = EV_PRS1_SETTING;
    PRS1ParsedSettingType m_setting;
    
    PRS1ParsedSettingEvent(PRS1ParsedSettingType setting, int value) : PRS1ParsedValueEvent(TYPE, 0, value), m_setting(setting) {}
};

class PRS1ScaledSettingEvent : public PRS1ParsedSettingEvent
{
public:
    PRS1ScaledSettingEvent(PRS1ParsedSettingType setting, int value, float gain)
        : PRS1ParsedSettingEvent(setting, value)
    {
        m_gain = gain;
    }
};

class PRS1PressureSettingEvent : public PRS1ScaledSettingEvent
{
public:
    static constexpr float GAIN = PRS1PressureEvent::GAIN;
    static const PRS1ParsedEventUnit UNIT = PRS1PressureEvent::UNIT;
    
    PRS1PressureSettingEvent(PRS1ParsedSettingType setting, int value, float gain=GAIN)
        : PRS1ScaledSettingEvent(setting, value, gain)
    {
        m_unit = UNIT;
    }
};

class PRS1ParsedSliceEvent : public PRS1ParsedValueEvent
{
public:
    virtual QMap<QString,QString> contents(void);

    static const PRS1ParsedEventType TYPE = EV_PRS1_SLICE;
    
    PRS1ParsedSliceEvent(int start, SliceStatus status) : PRS1ParsedValueEvent(TYPE, start, (int) status) {}
};


class PRS1ParsedAlarmEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

protected:
    PRS1ParsedAlarmEvent(PRS1ParsedEventType type, int start, int /*unused*/) : PRS1ParsedEvent(type, start) {}
};


class PRS1SnoresAtPressureEvent : public PRS1PressureEvent
{
public:
    static const PRS1ParsedEventType TYPE = EV_PRS1_SNORES_AT_PRESSURE;

    PRS1SnoresAtPressureEvent(int start, int kind, int pressure, int count, float gain=GAIN)
        : PRS1PressureEvent(TYPE, start, pressure, gain)
    {
        m_kind = kind;
        m_count = count;
    }

    virtual QMap<QString,QString> contents(void);

protected:
    int m_kind;
    // m_value is pressure
    int m_count;
};


#define _PRS1_EVENT(T, E, P, ARG) \
class T : public P \
{ \
public: \
    static const PRS1ParsedEventType TYPE = E; \
    T(int start, int ARG) : P(TYPE, start, ARG) {} \
};
#define PRS1_DURATION_EVENT(T, E) _PRS1_EVENT(T, E, PRS1ParsedDurationEvent, duration)
#define PRS1_VALUE_EVENT(T, E)    _PRS1_EVENT(T, E, PRS1ParsedValueEvent, value)
#define PRS1_ALARM_EVENT(T, E)    _PRS1_EVENT(T, E, PRS1ParsedAlarmEvent, value)
#define PRS1_PRESSURE_EVENT(T, E) \
class T : public PRS1PressureEvent \
{ \
public: \
    static const PRS1ParsedEventType TYPE = E; \
    T(int start, int value, float gain=PRS1PressureEvent::GAIN) : PRS1PressureEvent(TYPE, start, value, gain) {} \
};

PRS1_DURATION_EVENT(PRS1TimedBreathEvent, EV_PRS1_TB);
PRS1_DURATION_EVENT(PRS1ObstructiveApneaEvent, EV_PRS1_OA);
PRS1_DURATION_EVENT(PRS1ClearAirwayEvent, EV_PRS1_CA);
PRS1_DURATION_EVENT(PRS1FlowLimitationEvent, EV_PRS1_FL);
PRS1_DURATION_EVENT(PRS1PeriodicBreathingEvent, EV_PRS1_PB);
PRS1_DURATION_EVENT(PRS1LargeLeakEvent, EV_PRS1_LL);
PRS1_DURATION_EVENT(PRS1VariableBreathingEvent, EV_PRS1_VB);
PRS1_DURATION_EVENT(PRS1HypopneaEvent, EV_PRS1_HY);

PRS1_VALUE_EVENT(PRS1TotalLeakEvent, EV_PRS1_TOTLEAK);
PRS1_VALUE_EVENT(PRS1LeakEvent, EV_PRS1_LEAK);

PRS1_PRESSURE_EVENT(PRS1AutoPressureSetEvent, EV_PRS1_AUTO_PRESSURE_SET);
PRS1_PRESSURE_EVENT(PRS1PressureSetEvent, EV_PRS1_PRESSURE_SET);
PRS1_PRESSURE_EVENT(PRS1IPAPSetEvent, EV_PRS1_IPAP_SET);
PRS1_PRESSURE_EVENT(PRS1EPAPSetEvent, EV_PRS1_EPAP_SET);
PRS1_PRESSURE_EVENT(PRS1PressureAverageEvent, EV_PRS1_PRESSURE_AVG);
PRS1_PRESSURE_EVENT(PRS1FlexPressureAverageEvent, EV_PRS1_FLEX_PRESSURE_AVG);
PRS1_PRESSURE_EVENT(PRS1IPAPAverageEvent, EV_PRS1_IPAP_AVG);
PRS1_PRESSURE_EVENT(PRS1IPAPHighEvent, EV_PRS1_IPAPHIGH);
PRS1_PRESSURE_EVENT(PRS1IPAPLowEvent, EV_PRS1_IPAPLOW);
PRS1_PRESSURE_EVENT(PRS1EPAPAverageEvent, EV_PRS1_EPAP_AVG);

PRS1_VALUE_EVENT(PRS1RespiratoryRateEvent, EV_PRS1_RR);
PRS1_VALUE_EVENT(PRS1PatientTriggeredBreathsEvent, EV_PRS1_PTB);
PRS1_VALUE_EVENT(PRS1MinuteVentilationEvent, EV_PRS1_MV);
PRS1_VALUE_EVENT(PRS1SnoreEvent, EV_PRS1_SNORE);
PRS1_VALUE_EVENT(PRS1VibratorySnoreEvent, EV_PRS1_VS);
PRS1_VALUE_EVENT(PRS1PressurePulseEvent, EV_PRS1_PP);
PRS1_VALUE_EVENT(PRS1RERAEvent, EV_PRS1_RERA);  // TODO: should this really be a duration event?
PRS1_VALUE_EVENT(PRS1FlowRateEvent, EV_PRS1_FLOWRATE);  // TODO: is this a single event or an index/hour?
PRS1_VALUE_EVENT(PRS1Test1Event, EV_PRS1_TEST1);
PRS1_VALUE_EVENT(PRS1Test2Event, EV_PRS1_TEST2);
PRS1_VALUE_EVENT(PRS1HypopneaCount, EV_PRS1_HY_COUNT);  // F3V3 only
PRS1_VALUE_EVENT(PRS1ClearAirwayCount, EV_PRS1_CA_COUNT);  // F3V3 only
PRS1_VALUE_EVENT(PRS1ObstructiveApneaCount, EV_PRS1_OA_COUNT);  // F3V3 only

PRS1_ALARM_EVENT(PRS1DisconnectAlarmEvent, EV_PRS1_DISCONNECT_ALARM);
PRS1_ALARM_EVENT(PRS1ApneaAlarmEvent, EV_PRS1_APNEA_ALARM);
PRS1_ALARM_EVENT(PRS1LowMinuteVentilationAlarmEvent, EV_PRS1_LOW_MV_ALARM);

enum PRS1Mode {
    PRS1_MODE_UNKNOWN = -1,
    PRS1_MODE_CPAPCHECK = 0,    // "CPAP-Check"
    PRS1_MODE_CPAP,             // "CPAP"
    PRS1_MODE_AUTOCPAP,         // "AutoCPAP"
    PRS1_MODE_AUTOTRIAL,        // "Auto-Trial"
    PRS1_MODE_BILEVEL,          // "Bi-Level"
    PRS1_MODE_AUTOBILEVEL,      // "AutoBiLevel"
    PRS1_MODE_ASV,              // "ASV"
    PRS1_MODE_S,                // "S"
    PRS1_MODE_ST,               // "S/T"
    PRS1_MODE_PC,               // "PC"
    PRS1_MODE_ST_AVAPS,         // "S/T - AVAPS"
    PRS1_MODE_PC_AVAPS,         // "PC - AVAPS"
};

// Returns the set of all channels ever reported/supported by the parser for the given chunk.
const QVector<PRS1ParsedEventType> & GetSupportedEvents(const class PRS1DataChunk* chunk);


#endif // PRS1PARSER_H
