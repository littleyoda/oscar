/* PRS1 Parsing for CPAP and BIPAP (Family 0)
 *
 * Copyright (c) 2019-2021 The OSCAR Team
 * Portions copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "prs1_parser.h"
#include "prs1_loader.h"

bool PRS1DataChunk::ParseSummaryF0V6(void)
{
    if (this->family != 0 || this->familyVersion != 6) {
        qWarning() << "ParseSummaryF0V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 1, 0x2b, 9, 4, 2, 4, 1, 4, 0x1b, 2, 4, 0x0b, 1, 2, 6 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: The sizes contained in hblock can vary, even within a single machine, as can the length of hblock itself!

    // TODO: hardcoding this is ugly, think of a better approach
    if (chunk_size < minimum_sizes[0] + minimum_sizes[1] + minimum_sizes[2]) {
        qWarning() << this->sessionid << "summary data too short:" << chunk_size;
        return false;
    }
    if (chunk_size < 59) UNEXPECTED_VALUE(chunk_size, ">= 59");

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
                //CHECK_VALUES(data[pos], 1, 7);  // or 3?
                if (size == 4) {  // 400G has 3 more bytes?
                    //CHECK_VALUE(data[pos+1], 0);  // or 2, 14, 4, etc.
                    //CHECK_VALUES(data[pos+2], 8, 65);  // or 1
                    //CHECK_VALUES(data[pos+3], 0, 20);  // or 21, 22, etc.
                }
                break;
            case 1:  // Settings
                ok = this->ParseSettingsF0V6(data + pos, size);
                break;
            case 3:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            case 4:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
                break;
            case 8:  // vs. 7 in compliance, always follows mask off (except when there's a 5, see below), also longer
                // Maybe statistics of some kind, given the pressure stats that seem to appear before it on AutoCPAP machines?
                //CHECK_VALUES(data[pos], 0x02, 0x01);  // probably 16-bit value
                CHECK_VALUE(data[pos+1], 0x00);
                //CHECK_VALUES(data[pos+2], 0x0d, 0x0a);  // probably 16-bit value, maybe OA count?
                CHECK_VALUE(data[pos+3], 0x00);
                //CHECK_VALUES(data[pos+4], 0x09, 0x0b);  // probably 16-bit value
                CHECK_VALUE(data[pos+5], 0x00);
                //CHECK_VALUES(data[pos+6], 0x1e, 0x35);  // probably 16-bit value
                CHECK_VALUE(data[pos+7], 0x00);
                //CHECK_VALUES(data[pos+8], 0x8c, 0x4c);  // 16-bit value, not sure what
                //CHECK_VALUE(data[pos+9], 0x00);
                //CHECK_VALUES(data[pos+0xa], 0xbb, 0x00);  // 16-bit minutes in large leak
                //CHECK_VALUE(data[pos+0xb], 0x00);
                //CHECK_VALUES(data[pos+0xc], 0x15, 0x02);  // 16-bit minutes in PB
                //CHECK_VALUE(data[pos+0xd], 0x00);
                //CHECK_VALUES(data[pos+0xe], 0x01, 0x00);  // 16-bit VS count
                //CHECK_VALUE(data[pos+0xf], 0x00);
                //CHECK_VALUES(data[pos+0x10], 0x21, 5);  // probably 16-bit value, maybe H count?
                CHECK_VALUE(data[pos+0x11], 0x00);
                //CHECK_VALUES(data[pos+0x12], 0x13, 0);  // 16-bit value, not sure what
                //CHECK_VALUE(data[pos+0x13], 0x00);
                //CHECK_VALUES(data[pos+0x14], 0x05, 0);  // probably 16-bit value, maybe RE count?
                CHECK_VALUE(data[pos+0x15], 0x00);
                //CHECK_VALUE(data[pos+0x16], 0x00, 4);  // probably a 16-bit value, PB or FL count?
                CHECK_VALUE(data[pos+0x17], 0x00);
                //CHECK_VALUES(data[pos+0x18], 0x69, 0x23);
                //CHECK_VALUES(data[pos+0x19], 0x44, 0x18);
                //CHECK_VALUES(data[pos+0x1a], 0x80, 0x49);
                if (size >= 0x1f) {  // 500X is only 0x1b long!
                    //CHECK_VALUES(data[pos+0x1b], 0x00, 6);
                    CHECK_VALUE(data[pos+0x1c], 0x00);
                    //CHECK_VALUES(data[pos+0x1d], 0x0c, 0x0d);
                    //CHECK_VALUES(data[pos+0x1e], 0x31, 0x3b);
                    // TODO: 400G and 500G has 8 more bytes?
                    // TODO: 400G sometimes has another 4 on top of that?
                }
                break;
            case 2:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                //CHECK_VALUE(data[pos+2], 0x08);  // 0x01
                //CHECK_VALUE(data[pos+3], 0x14);  // 0x12
                //CHECK_VALUE(data[pos+4], 0x01);  // 0x00
                //CHECK_VALUE(data[pos+5], 0x22);  // 0x28
                //CHECK_VALUE(data[pos+6], 0x02);  // sometimes 1, 0
                CHECK_VALUE(data[pos+7], 0x00);  // 0x00
                CHECK_VALUE(data[pos+8], 0x00);  // 0x00
                if (size == 0x0c) {  // 400G has 3 more bytes, seem to match Equipment On bytes
                    //CHECK_VALUE(data[pos+1], 0);
                    //CHECK_VALUES(data[pos+2], 8, 65);
                    //CHECK_VALUE(data[pos+3], 0);
                }
                break;
            case 0x09:  // Time Elapsed (event 4 in F0V4)
                tt += data[pos] | (data[pos+1] << 8);
                break;
            case 0x0a:  // Humidifier setting change
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            case 0x0d:  // ???
                // seen on one 500G multiple times
                //CHECK_VALUE(data[pos], 0);  // 16-bit value
                //CHECK_VALUE(data[pos+1], 0);
                break;
            case 0x0e:
                // only seen once on 400G, many times on 500G
                //CHECK_VALUES(data[pos], 0, 6);  // 16-bit value
                //CHECK_VALUE(data[pos+1], 0);
                //CHECK_VALUES(data[pos+2], 7, 9);
                //CHECK_VALUES(data[pos+3], 7, 15);
                //CHECK_VALUES(data[pos+4], 7, 12);
                //CHECK_VALUES(data[pos+5], 0, 3);
                break;
            case 0x05:
                // AutoCPAP-related? First appeared on 500X, follows 4, before 8, look like pressure values
                //CHECK_VALUE(data[pos], 0x4b);    // maybe min pressure? (matches ramp pressure, see ramp on pressure graph)
                //CHECK_VALUE(data[pos+1], 0x5a);  // maybe max pressure? (close to max on pressure graph, time at pressure graph)
                //CHECK_VALUE(data[pos+2], 0x5a);  // seems to match Average 90% Pressure
                //CHECK_VALUE(data[pos+3], 0x58);  // seems to match Average CPAP
                break;
            case 0x07:
                // AutoBiLevel-related? First appeared on 700X, follows 4, before 8, looks like pressure values
                //CHECK_VALUE(data[pos], 0x50);    // maybe min IPAP or max titrated EPAP? (matches time at pressure graph, auto bi-level summary)
                //CHECK_VALUE(data[pos+1], 0x64);  // maybe max IPAP or max titrated IPAP? (matches time at pressure graph, auto bi-level summary)
                //CHECK_VALUE(data[pos+2], 0x4b);  // seems to match 90% EPAP
                //CHECK_VALUE(data[pos+3], 0x64);  // seems to match 90% IPAP
                break;
            case 0x0b:
                // CPAP-Check related, follows Mask On in CPAP-Check mode
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                //CHECK_VALUE(data[pos+2], 0);  // probably 16-bit value
                CHECK_VALUE(data[pos+3], 0);
                //CHECK_VALUE(data[pos+4], 0);  // probably 16-bit value
                CHECK_VALUE(data[pos+5], 0);
                //CHECK_VALUE(data[pos+6], 0);  // probably 16-bit value
                CHECK_VALUE(data[pos+7], 0);
                //CHECK_VALUE(data[pos+8], 0);  // probably 16-bit value
                CHECK_VALUE(data[pos+9], 0);
                //CHECK_VALUES(data[pos+0xa], 20, 60);  // or 0? 44 when changed pressure mid-session?
                break;
            case 0x06:
                // Maybe starting pressure? follows 4, before 8, looks like a pressure value, seen with CPAP-Check and EZ-Start
                // Maybe ending pressure: matches ending CPAP-Check pressure if it changes mid-session.
                // TODO: The daily details will show when it changed, so maybe there's an event that indicates a pressure change.
                //CHECK_VALUES(data[pos], 90, 60);  // maybe CPAP-Check pressure, also matches EZ-Start Pressure
                break;
            case 0x0c:
                // EZ-Start pressure for Auto-CPAP, seen on 500X110 following 4, before 8
                // Appears to reflect the current session's EZ-Start pressure, though reported afterwards
                //CHECK_VALUE(data[pos], 70, 80);
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


// The below is based on a combination of the old mainblock parsing for fileVersion == 3
// in ParseSummary() and the switch statements of ParseSummaryF0V6.
//
// Both compliance and summary files (at least for 200X and 400X machines) seem to have
// the same length for this slice, so maybe the settings are the same? At least 0x0a
// looks like a pressure in compliance files.
bool PRS1DataChunk::ParseSettingsF0V6(const unsigned char* data, int size)
{
    static const QMap<int,int> expected_lengths = { {0x0c,3}, {0x0d,2}, {0x0e,2}, {0x0f,4}, {0x10,3}, {0x35,2} };
    bool ok = true;

    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;
    FlexMode flexmode = FLEX_Unknown;

    int pressure = 0;
    int imin_ps   = 0;
    int imax_ps   = 0;
    int min_pressure = 0;
    int max_pressure = 0;

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
                case 0: cpapmode = PRS1_MODE_CPAP; break;
                case 1: cpapmode = PRS1_MODE_BILEVEL; break;
                case 2: cpapmode = PRS1_MODE_AUTOCPAP; break;
                case 3: cpapmode = PRS1_MODE_AUTOBILEVEL; break;
                case 4: cpapmode = PRS1_MODE_CPAPCHECK; break;
                default:
                    UNEXPECTED_VALUE(data[pos], "known device mode");
                    break;
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));
                break;
            case 1: // ???
                CHECK_VALUES(len, 1, 2);
                if (data[pos] != 0 && data[pos] != 3) {
                    CHECK_VALUES(data[pos], 1, 2);  // 1 when EZ-Start is enabled? 2 when Auto-Trial? 3 when Auto-Trial is off or Opti-Start isn't off?
                }
                if (len == 2) {  // 400G, 500G has extra byte
                    switch (data[pos+1]) {
                        case 0x00:  // 0x00 seen with EZ-Start disabled, no auto-trial, with CPAP-Check on 400X110
                        case 0x10:  // 0x10 seen with EZ-Start enabled, Opti-Start off on 500X110
                        case 0x20:  // 0x20 seen with Opti-Start enabled
                        case 0x30:  // 0x30 seen with both Opti-Start and EZ-Start enabled on 500X110
                        case 0x40:  // 0x40 seen with Auto-Trial
                        case 0x80:  // 0x80 seen with EZ-Start and CPAP-Check+ on 500X150
                            break;
                        default:
                            UNEXPECTED_VALUE(data[pos+1], "[0,0x10,0x20,0x30,0x40,0x80]")
                    }
                }
                break;
            case 0x0a:  // CPAP pressure setting
                CHECK_VALUE(len, 1);
                CHECK_VALUE(cpapmode, PRS1_MODE_CPAP);
                pressure = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, pressure));
                break;
            case 0x0c:  // CPAP-Check pressure setting
                CHECK_VALUE(len, 3);
                CHECK_VALUE(cpapmode, PRS1_MODE_CPAPCHECK);
                min_pressure = data[pos];  // Min Setting on pressure graph
                max_pressure = data[pos+1];  // Max Setting on pressure graph
                pressure = data[pos+2];  // CPAP on pressure graph and CPAP-Check Pressure on settings detail
                // This seems to be the initial pressure. If the pressure changes mid-session, the pressure
                // graph will show either the changed pressure or the majority pressure, not sure which.
                // The time of change is most likely in the events file. See slice 6 for ending pressure.
                //CHECK_VALUE(pressure, 0x5a);
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
                break;
            case 0x0d:  // AutoCPAP pressure setting
                CHECK_VALUE(len, 2);
                CHECK_VALUE(cpapmode, PRS1_MODE_AUTOCPAP);
                min_pressure = data[pos];
                max_pressure = data[pos+1];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
                break;
            case 0x0e:  // Bi-Level pressure setting
                CHECK_VALUE(len, 2);
                CHECK_VALUE(cpapmode, PRS1_MODE_BILEVEL);
                min_pressure = data[pos];
                max_pressure = data[pos+1];
                imin_ps = max_pressure - min_pressure;
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP, max_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS, imin_ps));
                break;
            case 0x0f:  // Auto Bi-Level pressure setting
                CHECK_VALUE(len, 4);
                CHECK_VALUE(cpapmode, PRS1_MODE_AUTOBILEVEL);
                min_pressure = data[pos];
                max_pressure = data[pos+1];
                imin_ps = data[pos+2];
                imax_ps = data[pos+3];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MIN, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, max_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, imin_ps));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, imax_ps));
                break;
            case 0x10: // Auto-Trial mode
                // This is not encoded as a separate mode as in F0V4, but instead as an auto-trial
                // duration on top of the CPAP or CPAP-Check mode. Reports show Auto-CPAP results,
                // but curiously report the use of C-Flex+, even though Auto-CPAP uses A-Flex.
                CHECK_VALUE(len, 3);
                CHECK_VALUES(cpapmode, PRS1_MODE_CPAP, PRS1_MODE_CPAPCHECK);
                if (data[pos] != 30 && data[pos] != 9) {
                    CHECK_VALUES(data[pos], 5, 25);  // Auto-Trial Duration
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_TRIAL, data[pos]));
                // If we want C-Flex+ to be reported as A-Flex, we can set cpapmode = PRS1_MODE_AUTOTRIAL here.
                // (Note that the setting event has already been added above, which is why ImportSummary needs
                // to adjust it when it sees this setting.)
                min_pressure = data[pos+1];
                max_pressure = data[pos+2];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
                break;
            case 0x2a:  // EZ-Start
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0x00, 0x80);  // both seem to mean enabled
                // 0x80 is CPAP Mode - EZ-Start in pressure detail chart, 0x00 is just CPAP mode with no EZ-Start pressure
                // TODO: How to represent which one is active in practice? Should this always be "true" since
                // either value means that the setting is enabled?
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_EZ_START, data[pos] != 0));
                break;
            case 0x42:  // EZ-Start enabled for Auto-CPAP?
                // Seen on 500X110 before 0x2b when EZ-Start is enabled on Auto-CPAP
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0x00, 0x80);  // both seem to mean enabled, 0x00 appears when Opti-Start is used instead
                // TODO: How to represent which one is active in practice? Should this always be "true" since
                // either value means that the setting is enabled?
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_EZ_START, data[pos] != 0));
                break;
            case 0x2b:  // Ramp Type
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);  // 0 == "Linear", 0x80 = "SmartRamp"
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TYPE, data[pos] != 0));
                break;
            case 0x2c:  // Ramp Time
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {  // 0 == ramp off, and ramp pressure setting doesn't appear
                    if (data[pos] < 5 || data[pos] > 45) UNEXPECTED_VALUE(data[pos], "5-45");
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, data[pos]));
                break;
            case 0x2d:  // Ramp Pressure
                CHECK_VALUE(len, 1);
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, data[pos]));
                break;
            case 0x2e:  // Flex mode
                CHECK_VALUE(len, 1);
                switch (data[pos]) {
                case 0:
                    flexmode = FLEX_None;
                    break;
                case 0x80:
                    switch (cpapmode) {
                        case PRS1_MODE_CPAP:
                        case PRS1_MODE_CPAPCHECK:
                        case PRS1_MODE_AUTOCPAP:
                        //case PRS1_MODE_AUTOTRIAL:
                            flexmode = FLEX_CFlex;
                            break;
                        case PRS1_MODE_BILEVEL:
                        case PRS1_MODE_AUTOBILEVEL:
                            flexmode = FLEX_BiFlex;
                            break;
                        default:
                            HEX(flexmode);
                            UNEXPECTED_VALUE(cpapmode, "untested mode");
                            break;
                    }
                    break;
                case 0x90:  // C-Flex+ or A-Flex, depending on machine mode
                    switch (cpapmode) {
                    case PRS1_MODE_CPAP:
                    case PRS1_MODE_CPAPCHECK:
                        flexmode = FLEX_CFlexPlus;
                        break;
                    case PRS1_MODE_AUTOCPAP:
                        flexmode = FLEX_AFlex;
                        break;
                    default:
                        UNEXPECTED_VALUE(cpapmode, "cpap or apap");
                        break;
                    }
                    break;
                case 0xA0:  // Rise Time
                    flexmode = FLEX_RiseTime;
                    switch (cpapmode) {
                        case PRS1_MODE_BILEVEL:
                        case PRS1_MODE_AUTOBILEVEL:
                            break;
                        default:
                            HEX(flexmode);
                            UNEXPECTED_VALUE(cpapmode, "autobilevel");
                            break;
                    }
                    break;
                case 0xB0:  // P-Flex
                    flexmode = FLEX_PFlex;
                    switch (cpapmode) {
                        case PRS1_MODE_AUTOCPAP:
                            break;
                        default:
                            HEX(flexmode);
                            UNEXPECTED_VALUE(cpapmode, "apap");
                            break;
                    }
                    break;
                default:
                    UNEXPECTED_VALUE(data[pos], "known flex mode");
                    break;
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_MODE, flexmode));
                break;
            case 0x2f:  // Flex lock
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, data[pos] != 0));
                break;
            case 0x30:  // Flex level
                CHECK_VALUE(len, 1);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LEVEL, data[pos]));
                if (flexmode == FLEX_PFlex) {
                    CHECK_VALUE(data[pos], 4);  // No number appears on reports.
                }
                if (flexmode == FLEX_RiseTime) {
                    if (data[pos] < 1 || data[pos] > 3) UNEXPECTED_VALUE(data[pos], "1-3");
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
                    CHECK_VALUES(data[pos], 2, 1);  // 15HT = 2, 15 = 1, 22 = 0
                }
                this->ParseTubingTypeV3(data[pos]);
                break;
            case 0x40:  // new to 400G, also seen on 500X110, alternate tubing type? appears after 0x39 and before 0x3c
                CHECK_VALUE(len, 1);
                if (data[pos] > 3) UNEXPECTED_VALUE(data[pos], "0-3");  // 0 = 22mm, 1 = 15mm, 2 = 15HT, 3 = 12mm
                this->ParseTubingTypeV3(data[pos]);
                break;
            case 0x3c:  // View Optional Screens
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_SHOW_AHI, data[pos] != 0));
                break;
            case 0x3e:  // Auto On
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_ON, data[pos] != 0));
                break;
            case 0x3f:  // Auto Off
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_OFF, data[pos] != 0));
                break;
            case 0x43:  // new to 502G, sessions 3-8, Auto-Trial is off, Opti-Start is missing
                CHECK_VALUE(len, 1);
                CHECK_VALUE(data[pos], 0x3C);
                break;
            case 0x44:  // new to 502G, sessions 3-8, Auto-Trial is off, Opti-Start is missing
                CHECK_VALUE(len, 1);
                CHECK_VALUE(data[pos], 0xFF);
                break;
            case 0x45:  // Target Time, specific to DreamStation Go
                CHECK_VALUE(len, 1);
                // Included in the data, but not shown on reports when humidifier is in Fixed mode.
                // According to the FAQ, this setting is only available in Adaptive mode.
                if (data[pos] < 40 || data[pos] > 100) {  // 4.0 through 10.0 hours in 0.5-hour increments
                    CHECK_VALUES(data[pos], 0, 1);  // Off and Auto
                }
                this->AddEvent(new PRS1ScaledSettingEvent(PRS1_SETTING_HUMID_TARGET_TIME, data[pos], 0.1));
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


const QVector<PRS1ParsedEventType> ParsedEventsF0V6 = {
    PRS1PressureSetEvent::TYPE,
    PRS1IPAPSetEvent::TYPE,
    PRS1EPAPSetEvent::TYPE,
    PRS1AutoPressureSetEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1RERAEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1FlowLimitationEvent::TYPE,
    PRS1VibratorySnoreEvent::TYPE,
    PRS1VariableBreathingEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1SnoreEvent::TYPE,
    PRS1PressureAverageEvent::TYPE,
    PRS1FlexPressureAverageEvent::TYPE,
    PRS1SnoresAtPressureEvent::TYPE,
};

// DreamStation family 0 CPAP/APAP machines (400X-700X, 400G-502G)
// Originally derived from F5V3 parsing + (incomplete) F0V234 parsing + sample data
bool PRS1DataChunk::ParseEventsF0V6()
{
    if (this->family != 0 || this->familyVersion != 6) {
        qWarning() << "ParseEventsF0V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 2, 3, 4, 3, 3, 3, 3, 3, 3, 2, 3, 4, 3, 2, 5, 5, 5, 5, 4, 3, 3, 3 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);

    if (chunk_size < 1) {
        // This does occasionally happen.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed, duration, value;
    bool is_bilevel = false;
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
        if (code != 0x12) {  // This one event has no timestamp
            t += data[pos] | (data[pos+1] << 8);
            pos += 2;
        }

        switch (code) {
            //case 0x00:  // never seen
            case 0x01:  // Pressure adjustment
                // Matches pressure setting, both initial and when ramp button pressed.
                // Based on waveform reports, it looks like the pressure graph is drawn by
                // interpolating between these pressure adjustments, by 0.5 cmH2O spaced evenly between
                // adjustments. E.g. 6 at 28:11 and 7.3 at 29:05 results in the following dots:
                // 6 at 28:11, 6.5 around 28:30, 7.0 around 28:50, 7(.3) at 29:05. That holds until
                // subsequent "adjustment" of 7.3 at 30:09 followed by 8.0 at 30:19.
                this->AddEvent(new PRS1PressureSetEvent(t, data[pos]));
                break;
            case 0x02:  // Pressure adjustment (bi-level)
                // See notes above on interpolation.
                this->AddEvent(new PRS1IPAPSetEvent(t, data[pos+1]));
                this->AddEvent(new PRS1EPAPSetEvent(t, data[pos]));  // EPAP needs to be added second to calculate PS
                is_bilevel = true;
                break;
            case 0x03:  // Auto-CPAP starting pressure
                // Most of the time this occurs, it's at the start and end of a session with
                // the same pressure at both. Occasionally an additional event shows up in the
                // middle of a session, and then the pressure at the end matches that.
                // In these cases, the new pressure corresponds to the next night's starting
                // pressure for auto-CPAP. It does not appear to have any effect on the current
                // night's pressure, unless there's a substantial gap between sessions, in
                // which case the next session may use the new starting pressure.
                //CHECK_VALUE(data[pos], 40);
                // TODO: What does this mean in bi-level mode?
                // See F0V4 event 3 for comparison. TODO: See if there's an Opti-Start label on F0V6 reports.
                this->AddEvent(new PRS1AutoPressureSetEvent(t, data[pos]));
                break;
            case 0x04:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x05:  // RERA
                elapsed = data[pos];  // based on sample waveform, the RERA is over after this
                this->AddEvent(new PRS1RERAEvent(t - elapsed, 0));
                break;
            case 0x06:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x07:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            //case 0x08:  // never seen
            //case 0x09:  // never seen
            //case 0x0a:  // Hypopnea, see 0x15
            case 0x0b:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xa, 0x14 and 0x15?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x0c:  // Flow Limitation
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating flow limitations ourselves. Flow limitations aren't
                // as obvious as OA/CA when looking at a waveform.
                elapsed = data[pos];
                this->AddEvent(new PRS1FlowLimitationEvent(t - elapsed, 0));
                break;
            case 0x0d:  // Vibratory Snore
                // VS events are instantaneous flags with no duration, drawn on the official waveform.
                // The current thinking is that these are the snores that cause a change in auto-titrating
                // pressure. The snoring statistics below seem to be a total count. It's unclear whether
                // the trigger for pressure change is severity or count or something else.
                // no data bytes
                this->AddEvent(new PRS1VibratorySnoreEvent(t, 0));
                break;
            case 0x0e:  // Variable Breathing?
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];  // this is always 60 seconds unless it's at the end, so it seems like elapsed
                CHECK_VALUES(elapsed, 60, 0);
                this->AddEvent(new PRS1VariableBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x0f:  // Periodic Breathing
                // PB events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x10:  // Large Leak
                // LL events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x11:  // Statistics
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos]));
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+1]));

                value = data[pos+2];
                if (is_bilevel) {
                    // For bi-level modes, this appears to be the time-weighted average of EPAP and IPAP actually provided.
                    this->AddEvent(new PRS1PressureAverageEvent(t, value));
                } else {
                    // For single-pressure modes, this appears to be the average effective "EPAP" provided by Flex.
                    //
                    // Sample data shows this value around 10.3 cmH2O for a prescribed pressure of 12.0 (C-Flex+ 3).
                    // That's too low for an average pressure over time, but could easily be an average commanded EPAP.
                    // When flex mode is off, this is exactly the current CPAP set point.
                    this->AddEvent(new PRS1FlexPressureAverageEvent(t, value));
                }
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            case 0x12:  // Snore count per pressure
                // Some sessions (with lots of ramps) have multiple of these, each with a
                // different pressure. The total snore count across all of them matches the
                // total found in the stats event.
                if (data[pos] != 0) {
                    CHECK_VALUES(data[pos], 1, 2);  // 0 = CPAP pressure, 1 = bi-level EPAP, 2 = bi-level IPAP
                }
                //CHECK_VALUE(data[pos+1], 0x78);  // pressure
                //CHECK_VALUE(data[pos+2], 1);  // 16-bit snore count
                //CHECK_VALUE(data[pos+3], 0);
                value = (data[pos+2] | (data[pos+3] << 8));
                this->AddEvent(new PRS1SnoresAtPressureEvent(t, data[pos], data[pos+1], value));
                break;
            //case 0x13:  // never seen
            case 0x0a:  // Hypopnea
                // TODO: Why does this hypopnea have a different event code?
                // fall through
            case 0x14:  // Hypopnea, new to F0V6
                // TODO: Why does this hypopnea have a different event code?
                // fall through
            case 0x15:  // Hypopnea, new to F0V6
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating hypopneas ourselves. Their official definition
                // is 40% reduction in flow lasting at least 10s.
                duration = data[pos];
                this->AddEvent(new PRS1HypopneaEvent(t - duration, 0));
                break;
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
