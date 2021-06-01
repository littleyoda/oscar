/* PRS1 Parsing for S/T and AVAPS ventilators (Family 3)
 *
 * Copyright (c) 2019-2021 The OSCAR Team
 * Portions copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "prs1_parser.h"
#include "prs1_loader.h"

// Originally based on ParseSummaryF5V3, with changes observed in ventilator sample data
//
// TODO: surely there will be a way to merge ParseSummary (FV3) loops and abstract the machine-specific
// encodings into another function or class, but that's probably worth pursuing only after
// the details have been figured out.
bool PRS1DataChunk::ParseSummaryF3V6(void)
{
    if (this->family != 3 || this->familyVersion != 6) {
        qWarning() << "ParseSummaryF3V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 1, 0x25, 9, 7, 4, 2, 1, 2, 2, 1, 0x18, 2, 4 };  // F5V3 = { 1, 0x38, 4, 2, 4, 0x1e, 2, 4, 9 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: The sizes contained in hblock can vary, even within a single machine, as can the length of hblock itself!

    // TODO: hardcoding this is ugly, think of a better approach
    if (chunk_size < minimum_sizes[0] + minimum_sizes[1] + minimum_sizes[2]) {
        qWarning() << this->sessionid << "summary data too short:" << chunk_size;
        return false;
    }
    // We've once seen a short summary with no mask-on/off: just equipment-on, settings, 2, equipment-off
    // (And we've seen something similar in F5V3.)
    if (chunk_size < 58) UNEXPECTED_VALUE(chunk_size, ">= 58");

    bool ok = true;
    int pos = 0;
    int code, size;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        if (!this->hblock.contains(code)) {
            qWarning() << this->sessionid << "missing hblock entry for" << code;
            ok = false;
            break;
        }
        size = this->hblock[code];
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            if (size < minimum_sizes[code]) {
                UNEXPECTED_VALUE(size, minimum_sizes[code]);
                qWarning() << this->sessionid << "slice" << code << "too small" << size << "<" << minimum_sizes[code];
                if (code != 1) {  // Settings are variable-length, so shorter settings slices aren't fatal.
                    ok = false;
                    break;
                }
            }
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }

        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first?
                //CHECK_VALUE(data[pos], 0x10);  // usually 0x10 for 1030X, sometimes 0x40 or 0x80 are set in addition or instead
                CHECK_VALUE(size, 1);
                break;
            case 1:  // Settings
                ok = this->ParseSettingsF3V6(data + pos, size);
                break;
            case 2:  // seems equivalent to F5V3 #9, comes right after settings, usually 9 bytes, identical values
                // TODO: This may be structurally similar to settings: a list of (code, length, value).
                CHECK_VALUE(data[pos], 0);
                CHECK_VALUE(data[pos+1], 1);
                //CHECK_VALUE(data[pos+2], 0);  // Apnea Alarm (0=off, 1=10, 2=20)
                if (data[pos+2] != 0) {
                    CHECK_VALUES(data[pos+2], 1, 2);
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_APNEA_ALARM, data[pos+2] * 10));
                }
                CHECK_VALUE(data[pos+3], 1);
                CHECK_VALUE(data[pos+4], 1);
                CHECK_VALUES(data[pos+5], 0, 1);  // 1 = Low Minute Ventilation Alarm set to 1
                CHECK_VALUE(data[pos+6], 2);
                CHECK_VALUE(data[pos+7], 1);
                CHECK_VALUE(data[pos+8], 0);  // 1 = patient disconnect alarm of 15 sec on F5V3, not sure where time is encoded
                if (size > 9) {
                    CHECK_VALUE(data[pos+9],  3);
                    CHECK_VALUE(data[pos+10], 1);
                    CHECK_VALUE(data[pos+11], 0);
                    CHECK_VALUE(size, 12);
                }
                break;
            case 4:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            case 5:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
                break;
            case 6:  // Ventilator CPAP stats, presumably per mask-on slice
                //CHECK_VALUE(data[pos], 0x3C);  // Average CPAP
                break;
            case 7:  // Ventilator EPAP stats, presumably per mask-on slice
                //CHECK_VALUE(data[pos], 0x69);  // Average EPAP
                //CHECK_VALUE(data[pos+1], 0x80);  // Average 90% EPAP
                break;
            case 8:  // Ventilator IPAP stats, presumably per mask-on slice
                //CHECK_VALUE(data[pos], 0x86);  // Average IPAP
                //CHECK_VALUE(data[pos+1], 0xA8);  // Average 90% IPAP
                break;
            case 0xa:  // Patient statistics, presumably per mask-on slice
                //CHECK_VALUE(data[pos], 0x00);  // 16-bit OA count
                CHECK_VALUE(data[pos+1], 0x00);
                //CHECK_VALUE(data[pos+2], 0x00);  // 16-bit CA count
                CHECK_VALUE(data[pos+3], 0x00);
                //CHECK_VALUE(data[pos+4], 0x00);  // 16-bit minutes in LL
                CHECK_VALUE(data[pos+5], 0x00);
                //CHECK_VALUE(data[pos+6], 0x0A);  // 16-bit VS count
                //CHECK_VALUE(data[pos+7], 0x00);  // We've actually seen someone with more than 255 VS in a night!
                //CHECK_VALUE(data[pos+8], 0x01);  // 16-bit H count (partial)
                CHECK_VALUE(data[pos+9], 0x00);
                //CHECK_VALUE(data[pos+0xa], 0x00);  // 16-bit H count (partial)
                CHECK_VALUE(data[pos+0xb], 0x00);
                //CHECK_VALUE(data[pos+0xc], 0x00);  // 16-bit RE count
                CHECK_VALUE(data[pos+0xd], 0x00);
                //CHECK_VALUE(data[pos+0xe], 0x3e);  // average total leak
                //CHECK_VALUE(data[pos+0xf], 0x03);  // 16-bit H count (partial)
                CHECK_VALUE(data[pos+0x10], 0x00);
                //CHECK_VALUE(data[pos+0x11], 0x11);  // average breath rate
                //CHECK_VALUE(data[pos+0x12], 0x41);  // average TV / 10
                //CHECK_VALUE(data[pos+0x13], 0x60);  // average % PTB
                //CHECK_VALUE(data[pos+0x14], 0x0b);  // average minute vent
                //CHECK_VALUE(data[pos+0x15], 0x1d);  // average leak? (similar position to F5V3, similar delta to total leak)
                //CHECK_VALUE(data[pos+0x16], 0x00);  // 16-bit minutes in PB
                CHECK_VALUE(data[pos+0x17], 0x00);
                break;
            case 3:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                //CHECK_VALUES(data[pos+2], 1, 4);  // bitmask, have seen 1, 4, 6, 0x41
                //CHECK_VALUE(data[pos+3], 0x17);  // 0x16, etc.
                //CHECK_VALUES(data[pos+4], 0, 1);  // or 2
                //CHECK_VALUE(data[pos+5], 0x15);  // 0x16, etc.
                //CHECK_VALUES(data[pos+6], 0, 1);  // or 2
                break;
            case 0xc:  // Humidier setting change
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            default:
                UNEXPECTED_VALUE(code, "known slice code");
                break;
        }
        pos += size;
    }

    this->duration = tt;

    return ok;
}


// Based initially on ParseSettingsF5V3. Many of the codes look the same, like always starting with 0, 0x35 looking like
// a humidifier setting, etc., but the contents are sometimes a bit different, such as mode values and pressure settings.
//
// new settings to find: ...
bool PRS1DataChunk::ParseSettingsF3V6(const unsigned char* data, int size)
{
    static const QMap<int,int> expected_lengths = { {0x1e,3}, {0x35,2} };
    bool ok = true;

    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;
    FlexMode flexmode = FLEX_Unknown;

    // F5V3 and F3V6 use a gain of 0.125 rather than 0.1 to allow for a maximum value of 30 cmH2O
    static const float GAIN = 0.125;  // TODO: parameterize this somewhere better

    int fixed_pressure = 0;
    int fixed_epap = 0;
    int fixed_ipap = 0;
    int min_ipap = 0;
    int max_ipap = 0;
    int breath_rate;
    int timed_inspiration;

    // Parse the nested data structure which contains settings
    int pos = 0;
    do {
        int code = data[pos++];
        int len = data[pos++];

        int expected_len = 1;
        if (expected_lengths.contains(code)) {
            expected_len = expected_lengths[code];
        }
        //CHECK_VALUE(len, expected_len);
        if (len < expected_len) {
            qWarning() << this->sessionid << "setting" << code << "too small" << len << "<" << expected_len;
            ok = false;
            break;
        }
        if (pos + len > size) {
            qWarning() << this->sessionid << "setting" << code << "@" << pos << "longer than remaining slice";
            ok = false;
            break;
        }

        switch (code) {
            case 0: // Device Mode
                CHECK_VALUE(pos, 2);  // always first?
                CHECK_VALUE(len, 1);
                switch (data[pos]) {
                case 0: cpapmode = PRS1_MODE_CPAP; break; // "CPAP" mode
                case 1: cpapmode = PRS1_MODE_S; break;   // "S" mode
                case 2: cpapmode = PRS1_MODE_ST; break;  // "S/T" mode; pressure seems variable?
                case 4: cpapmode = PRS1_MODE_PC; break;  // "PC" mode? Usually "PC - AVAPS", see setting 1 below
                default:
                    UNEXPECTED_VALUE(data[pos], "known device mode");
                    break;
                }
                break;
            case 1: // Flex Mode
                CHECK_VALUE(len, 1);
                switch (data[pos]) {
                    case 0:  // 0 = None
                        switch (cpapmode) {
                            case PRS1_MODE_CPAP: flexmode = FLEX_None; break;
                            case PRS1_MODE_S:    flexmode = FLEX_RiseTime; break;  // reports say "None" but then list a rise time setting
                            case PRS1_MODE_ST:   flexmode = FLEX_RiseTime; break;  // reports say "None" but then list a rise time setting
                            default:
                                UNEXPECTED_VALUE(cpapmode, "CPAP, S, or S/T");
                                break;
                        }
                        break;
                    case 1:  // 1 = Bi-Flex, only seen with "S - Bi-Flex"
                        flexmode = FLEX_BiFlex;
                        CHECK_VALUE(cpapmode, PRS1_MODE_S);
                        break;
                    case 2:  // 2 = AVAPS: usually "PC - AVAPS", sometimes "S/T - AVAPS"
                        switch (cpapmode) {
                            case PRS1_MODE_ST: cpapmode = PRS1_MODE_ST_AVAPS; break;
                            case PRS1_MODE_PC: cpapmode = PRS1_MODE_PC_AVAPS; break;
                            default:
                                UNEXPECTED_VALUE(cpapmode, "S/T or PC");
                                break;
                        }
                        flexmode = FLEX_RiseTime;  // reports say "AVAPS" but then list a rise time setting
                        break;
                    default:
                        UNEXPECTED_VALUE(data[pos], "known flex mode");
                        break;
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_MODE, (int) flexmode));
                break;
            case 2: // ??? Maybe AAM?
                CHECK_VALUE(len, 1);
                CHECK_VALUE(data[pos], 0);
                break;
            case 3: // CPAP Pressure
                CHECK_VALUE(len, 1);
                CHECK_VALUE(cpapmode, PRS1_MODE_CPAP);
                fixed_pressure = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, fixed_pressure, GAIN));
                break;
            case 4: // EPAP Pressure
                CHECK_VALUE(len, 1);
                if (cpapmode == PRS1_MODE_CPAP) UNEXPECTED_VALUE(cpapmode, "!cpap");
                // pressures seem variable on practice, maybe due to ramp or leaks?
                fixed_epap = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP, fixed_epap, GAIN));
                break;
            case 7: // IPAP Pressure
                CHECK_VALUE(len, 1);
                CHECK_VALUES(cpapmode, PRS1_MODE_S, PRS1_MODE_ST);
                // pressures seem variable on practice, maybe due to ramp or leaks?
                fixed_ipap = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP, fixed_ipap, GAIN));
                // TODO: We need to revisit whether PS should be shown as a setting.
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS, fixed_ipap - fixed_epap, GAIN));
                if (fixed_epap == 0) UNEXPECTED_VALUE(fixed_epap, ">0");
                break;
            case 8:  // Min IPAP
                CHECK_VALUE(len, 1);
                CHECK_VALUE(fixed_ipap, 0);
                CHECK_VALUES(cpapmode, PRS1_MODE_ST_AVAPS, PRS1_MODE_PC_AVAPS);
                min_ipap = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MIN, min_ipap, GAIN));
                // TODO: We need to revisit whether PS should be shown as a setting.
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, min_ipap - fixed_epap, GAIN));
                if (fixed_epap == 0) UNEXPECTED_VALUE(fixed_epap, ">0");
                break;
            case 9:  // Max IPAP
                CHECK_VALUE(len, 1);
                CHECK_VALUE(fixed_ipap, 0);
                CHECK_VALUES(cpapmode, PRS1_MODE_ST_AVAPS, PRS1_MODE_PC_AVAPS);
                if (min_ipap == 0) UNEXPECTED_VALUE(min_ipap, ">0");
                max_ipap = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, max_ipap, GAIN));
                // TODO: We need to revisit whether PS should be shown as a setting.
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, max_ipap - fixed_epap, GAIN));
                if (fixed_epap == 0) UNEXPECTED_VALUE(fixed_epap, ">0");
                break;
            case 0x19:  // Tidal Volume (AVAPS)
                CHECK_VALUE(len, 1);
                CHECK_VALUES(cpapmode, PRS1_MODE_ST_AVAPS, PRS1_MODE_PC_AVAPS);
                //CHECK_VALUE(data[pos], 47);  // gain 10.0
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TIDAL_VOLUME, data[pos] * 10.0));
                break;
            case 0x1e:  // (Backup) Breath Rate (S/T and PC)
                CHECK_VALUE(len, 3);
                if (cpapmode == PRS1_MODE_CPAP || cpapmode == PRS1_MODE_S) UNEXPECTED_VALUE(cpapmode, "S/T or PC");
                switch (data[pos]) {
                case 0:  // Breath Rate Off
                    // TODO: Is this mode essentially bilevel? The pressure graphs are confusing.
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Off));
                    CHECK_VALUE(data[pos+1], 0);
                    CHECK_VALUE(data[pos+2], 0);
                    break;
                //case 1:  // Breath Rate Auto in F5V3 setting 0x14
                case 2:  // Breath Rate (fixed BPM)
                    breath_rate = data[pos+1];
                    timed_inspiration = data[pos+2];
                    if (breath_rate < 9 || breath_rate > 15) UNEXPECTED_VALUE(breath_rate, "9-15");
                    if (timed_inspiration < 8 || timed_inspiration > 20) UNEXPECTED_VALUE(timed_inspiration, "8-20");
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Fixed));
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_RATE, breath_rate));
                    this->AddEvent(new PRS1ScaledSettingEvent(PRS1_SETTING_BACKUP_TIMED_INSPIRATION, timed_inspiration, 0.1));
                    break;
                default:
                    CHECK_VALUES(data[pos], 0, 2);  // 0 = Breath Rate off (S), 2 = fixed BPM (1 = auto on F5V3 setting 0x14, haven't seen it on F3V6 yet)
                    break;
                }
                break;
            //0x2b: Ramp type sounds like it's linear for F3V6 unless AAM is enabled, so no setting may be needed.
            case 0x2c:  // Ramp Time
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {  // 0 == ramp off, and ramp pressure setting doesn't appear
                    if (data[pos] < 5 || data[pos] > 45) UNEXPECTED_VALUE(data[pos], "5-45");
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, data[pos]));
                break;
            case 0x2d:  // Ramp Pressure (with ASV/ventilator pressure encoding), only present when ramp is on
                CHECK_VALUE(len, 1);
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, data[pos], GAIN));
                break;
            case 0x2e:  // Bi-Flex level or Rise Time
                // On F5V3 the first byte could specify Bi-Flex or Rise Time, and second byte contained the value.
                // On F3V6 there's only one byte, which seems to correspond to Rise Time on the reports with flex
                // mode None or AVAPS and to Bi-Flex Setting (level) in Bi-Flex mode.
                CHECK_VALUE(len, 1);
                if (flexmode == FLEX_BiFlex) {
                    // Bi-Flex level
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LEVEL, data[pos]));
                } else if (flexmode == FLEX_RiseTime) {
                    // Rise time
                    if (data[pos] < 1 || data[pos] > 6) UNEXPECTED_VALUE(data[pos], "1-6");  // 1-6 have been seen
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME, data[pos]));
                } else {
                    UNEXPECTED_VALUE(flexmode, "BiFlex or RiseTime");
                }
                // Timed inspiration specified in the backup breath rate.
                break;
            case 0x2f:  // Flex / Rise Time lock
                CHECK_VALUE(len, 1);
                if (flexmode == FLEX_BiFlex) {
                    CHECK_VALUE(cpapmode, PRS1_MODE_S);
                    CHECK_VALUES(data[pos], 0, 0x80);  // Bi-Flex Lock
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, data[pos] != 0));
                } else if (flexmode == FLEX_RiseTime) {
                    CHECK_VALUES(data[pos], 0, 0x80);  // Rise Time Lock
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME_LOCK, data[pos] != 0));
                } else {
                    UNEXPECTED_VALUE(flexmode, "BiFlex or RiseTime");
                }
                break;
            case 0x35:  // Humidifier setting
                CHECK_VALUE(len, 2);
                this->ParseHumidifierSettingV3(data[pos], data[pos+1], true);
                break;
            case 0x36:  // Mask Resistance Lock
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_LOCK, data[pos] != 0));
                break;
            case 0x38:  // Mask Resistance
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {  // 0 == mask resistance off
                    if (data[pos] < 1 || data[pos] > 5) UNEXPECTED_VALUE(data[pos], "1-5");
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_SETTING, data[pos]));
                break;
            case 0x39:  // Tubing Type Lock
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TUBING_LOCK, data[pos] != 0));
                break;
            case 0x3b:  // Tubing Type
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {
                    CHECK_VALUES(data[pos], 2, 1);  // 15HT = 2, 15 = 1, 22 = 0, though report only says "15" for 15HT
                }
                this->ParseTubingTypeV3(data[pos]);
                break;
            case 0x3c:  // View Optional Screens
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_SHOW_AHI, data[pos] != 0));
                break;
            default:
                UNEXPECTED_VALUE(code, "known setting");
                qDebug() << "Unknown setting:" << hex << code << "in" << this->sessionid << "at" << pos;
                this->AddEvent(new PRS1UnknownDataEvent(QByteArray((const char*) data, size), pos, len));
                break;
        }

        pos += len;
    } while (ok && pos + 2 <= size);
    
    return ok;
}


const QVector<PRS1ParsedEventType> ParsedEventsF3V6 = {
    PRS1TimedBreathEvent::TYPE,
    PRS1IPAPAverageEvent::TYPE,
    PRS1EPAPAverageEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1RespiratoryRateEvent::TYPE,
    PRS1PatientTriggeredBreathsEvent::TYPE,
    PRS1MinuteVentilationEvent::TYPE,
    PRS1TidalVolumeEvent::TYPE,
    PRS1Test2Event::TYPE,
    PRS1Test1Event::TYPE,
    PRS1SnoreEvent::TYPE,  // No individual VS, only snore count
    PRS1LeakEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1RERAEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
    PRS1ApneaAlarmEvent::TYPE,
    // No FL?
};

// 1030X, 11030X series
// based on ParseEventsF5V3, updated for F3V6
bool PRS1DataChunk::ParseEventsF3V6(void)
{
    if (this->family != 3 || this->familyVersion != 6) {
        qWarning() << "ParseEventsF3V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 2, 3, 0xe, 3, 3, 3, 4, 5, 3, 5, 3, 3, 2, 2, 2, 2 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);

    if (chunk_size < 1) {
        // This does occasionally happen.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    // F3V6 uses a gain of 0.125 rather than 0.1 to allow for a maximum value of 30 cmH2O
    static const float GAIN = 0.125;  // TODO: this should be parameterized somewhere more logical
    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed, duration;
    do {
        code = data[pos++];
        if (!this->hblock.contains(code)) {
            qWarning() << this->sessionid << "missing hblock entry for event" << code;
            ok = false;
            break;
        }
        size = this->hblock[code];
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            if (size < minimum_sizes[code]) {
                qWarning() << this->sessionid << "event" << code << "too small" << size << "<" << minimum_sizes[code];
                ok = false;
                break;
            }
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "event" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        startpos = pos;
        t += data[pos] | (data[pos+1] << 8);
        pos += 2;

        switch (code) {
            // case 0x00?
            case 1:  // Timed Breath
                // TB events have a duration in 0.1s, based on the review of pressure waveforms.
                // TODO: Ideally the starting time here would be adjusted here, but PRS1ParsedEvents
                // currently assume integer seconds rather than ms, so that's done at import.
                duration = data[pos];
                // TODO: make sure F3 import logic matches F5 in adjusting TB start time
                this->AddEvent(new PRS1TimedBreathEvent(t, duration));
                break;
            case 2:  // Statistics
                // These appear every 2 minutes, so presumably summarize the preceding period.
                //data[pos+0];  // TODO: 0 = ???
                this->AddEvent(new PRS1IPAPAverageEvent(t, data[pos+2], GAIN));        // 02=IPAP
                this->AddEvent(new PRS1EPAPAverageEvent(t, data[pos+1], GAIN));        // 01=EPAP, needs to be added second to calculate PS
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos+3]));                // 03=Total leak (average?)
                this->AddEvent(new PRS1RespiratoryRateEvent(t, data[pos+4]));          // 04=Breaths Per Minute (average?)
                this->AddEvent(new PRS1PatientTriggeredBreathsEvent(t, data[pos+5]));  // 05=Patient Triggered Breaths (average?)
                this->AddEvent(new PRS1MinuteVentilationEvent(t, data[pos+6]));        // 06=Minute Ventilation (average?)
                this->AddEvent(new PRS1TidalVolumeEvent(t, data[pos+7]));              // 07=Tidal Volume (average?)
                this->AddEvent(new PRS1Test2Event(t, data[pos+8]));                    // 08=Flow???
                this->AddEvent(new PRS1Test1Event(t, data[pos+9]));                    // 09=TMV???
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+0xa]));                  // 0A=Snore count  // TODO: not a VS on official waveform, but appears in flags and contributes to overall VS index
                this->AddEvent(new PRS1LeakEvent(t, data[pos+0xb]));                   // 0B=Leak (average?)
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            case 0x03:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x04:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x05:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            case 0x06:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xd and 0xe?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x07:  // Periodic Breathing
                // PB events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x08:  // RERA
                elapsed = data[pos];  // based on sample waveform, the RERA is over after this
                this->AddEvent(new PRS1RERAEvent(t - elapsed, 0));
                break;
            case 0x09:  // Large Leak
                // LL events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x0a:  // Hypopnea
                // TODO: Why does this hypopnea have a different event code?
                // fall through
            case 0x0b:  // Hypopnea
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating hypopneas ourselves. Their official definition
                // is 40% reduction in flow lasting at least 10s.
                duration = data[pos];
                this->AddEvent(new PRS1HypopneaEvent(t - duration, 0));
                break;
            case 0x0c:  // Apnea Alarm
                // no additional data
                this->AddEvent(new PRS1ApneaAlarmEvent(t, 0));
                break;
            case 0x0d:  // Low MV Alarm
                // no additional data
                this->AddEvent(new PRS1LowMinuteVentilationAlarmEvent(t, 0));
                break;
            // case 0x0e?
            // case 0x0f?
            default:
                DUMP_EVENT();
                UNEXPECTED_VALUE(code, "known event code");
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos-1, size+1));
                break;
        }
        pos = startpos + size;
    } while (ok && pos < chunk_size);

    this->duration = t;

    return ok;
}
