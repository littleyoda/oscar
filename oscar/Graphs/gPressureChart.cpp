/* gPressureChart Implementation
 *
 * Copyright (c) 2020 The Oscar Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "gPressureChart.h"

gPressureChart::gPressureChart()
    : gSummaryChart("Pressure", MT_CPAP)
{

    // Do not reorder these!!! :P
    addCalc(CPAP_Pressure, ST_SETMAX, schema::channel[CPAP_Pressure].defaultColor());    // 00
    addCalc(CPAP_Pressure, ST_MID, schema::channel[CPAP_Pressure].defaultColor());       // 01
    addCalc(CPAP_Pressure, ST_90P, brighten(schema::channel[CPAP_Pressure].defaultColor(), 1.33f)); // 02
    addCalc(CPAP_PressureMin, ST_SETMIN, schema::channel[CPAP_PressureMin].defaultColor());  // 03
    addCalc(CPAP_PressureMax, ST_SETMAX, schema::channel[CPAP_PressureMax].defaultColor());  // 04

    addCalc(CPAP_EPAP, ST_SETMAX, schema::channel[CPAP_EPAP].defaultColor());      // 05
    addCalc(CPAP_IPAP, ST_SETMAX, schema::channel[CPAP_IPAP].defaultColor());      // 06
    addCalc(CPAP_EPAPLo, ST_SETMAX, schema::channel[CPAP_EPAPLo].defaultColor());    // 07
    addCalc(CPAP_IPAPHi, ST_SETMAX, schema::channel[CPAP_IPAPHi].defaultColor());    // 08

    addCalc(CPAP_EPAP, ST_MID, schema::channel[CPAP_EPAP].defaultColor());         // 09
    addCalc(CPAP_EPAP, ST_90P, brighten(schema::channel[CPAP_EPAP].defaultColor(),1.33f));         // 10
    addCalc(CPAP_IPAP, ST_MID, schema::channel[CPAP_IPAP].defaultColor());         // 11
    addCalc(CPAP_IPAP, ST_90P, brighten(schema::channel[CPAP_IPAP].defaultColor(),1.33f));         // 12
}

void gPressureChart::afterDraw(QPainter &, gGraph &graph, QRectF rect)
{
    int pressure_cnt = calcitems[0].cnt;
    int pressuremin_cnt = calcitems[3].cnt;
    int epap_cnt = calcitems[5].cnt;
    int ipap_cnt = calcitems[6].cnt;
    int ipaphi_cnt = calcitems[8].cnt;
    int epaplo_cnt = calcitems[7].cnt;

    QStringList presstr;

    float mid = 0;

    if (pressure_cnt > 0) {
        mid = calcitems[0].mid();
        presstr.append(QString("%1 %2/%3/%4").
                arg(STR_TR_CPAP).
                arg(calcitems[0].min,0,'f',1).
                arg(mid, 0, 'f', 1).
                arg(calcitems[0].max,0,'f',1));
    }
    if (pressuremin_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4/%5").
                arg(STR_TR_APAP).
                arg(calcitems[3].min,0,'f',1).
                arg(calcitems[1].mid(), 0, 'f', 1).
                arg(calcitems[2].mid(),0,'f',1).
                arg(calcitems[4].max, 0, 'f', 1));

    }
    if (epap_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4").
                arg(STR_TR_EPAP).
                arg(calcitems[5].min,0,'f',1).
                arg(calcitems[5].mid(), 0, 'f', 1).
                arg(calcitems[5].max, 0, 'f', 1));
    }
    if (ipap_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4").
             arg(STR_TR_IPAP).
             arg(calcitems[6].min,0,'f',1).
             arg(calcitems[6].mid(), 0, 'f', 1).
             arg(calcitems[6].max, 0, 'f', 1));
    }
    if (epaplo_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4").
            arg(STR_TR_EPAPLo).
            arg(calcitems[7].min,0,'f',1).
            arg(calcitems[7].mid(), 0, 'f', 1).
            arg(calcitems[7].max, 0, 'f', 1));
    }

    if (ipaphi_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4").
            arg(STR_TR_IPAPHi).
            arg(calcitems[8].min,0,'f',1).
            arg(calcitems[8].mid(), 0, 'f', 1).
            arg(calcitems[8].max, 0, 'f', 1));
    }
    QString txt = presstr.join(" ");
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);

}


void gPressureChart::populate(Day * day, int idx)
{
    float tmp;
    CPAPMode mode =  (CPAPMode)(int)qRound(day->settings_wavg(CPAP_Mode));
    QVector<SummaryChartSlice> & slices = cache[idx];

    if (mode == MODE_CPAP) {
        float pr = day->settings_max(CPAP_Pressure);
        slices.append(SummaryChartSlice(&calcitems[0], pr, pr, schema::channel[CPAP_Pressure].label(), calcitems[0].color));
    } else if (mode == MODE_APAP) {
        float min = day->settings_min(CPAP_PressureMin);
        float max = day->settings_max(CPAP_PressureMax);

        tmp = min;

        slices.append(SummaryChartSlice(&calcitems[3], min, min, schema::channel[CPAP_PressureMin].label(), calcitems[3].color));
        if (!day->summaryOnly()) {
            float med = day->calcMiddle(CPAP_Pressure);
            slices.append(SummaryChartSlice(&calcitems[1], med, med - tmp, day->calcMiddleLabel(CPAP_Pressure), calcitems[1].color));
            tmp += med - tmp;

            float p90 = day->calcPercentile(CPAP_Pressure);
            slices.append(SummaryChartSlice(&calcitems[2], p90, p90 - tmp, day->calcPercentileLabel(CPAP_Pressure), calcitems[2].color));
            tmp += p90 - tmp;
        }
        slices.append(SummaryChartSlice(&calcitems[4], max, max - tmp, schema::channel[CPAP_PressureMax].label(), calcitems[4].color));

    } else if (mode == MODE_BILEVEL_FIXED) {
        float epap = day->settings_max(CPAP_EPAP);
        float ipap = day->settings_max(CPAP_IPAP);

        slices.append(SummaryChartSlice(&calcitems[5], epap, epap, schema::channel[CPAP_EPAP].label(), calcitems[5].color));
        slices.append(SummaryChartSlice(&calcitems[6], ipap, ipap - epap, schema::channel[CPAP_IPAP].label(), calcitems[6].color));

    } else if (mode == MODE_BILEVEL_AUTO_FIXED_PS) {
        float epap = day->settings_max(CPAP_EPAPLo);
        tmp = epap;
        float ipap = day->settings_max(CPAP_IPAPHi);

        slices.append(SummaryChartSlice(&calcitems[7], epap, epap, schema::channel[CPAP_EPAPLo].label(), calcitems[7].color));
        if (!day->summaryOnly()) {

            float e50 = day->calcMiddle(CPAP_EPAP);
            slices.append(SummaryChartSlice(&calcitems[9], e50, e50 - tmp, day->calcMiddleLabel(CPAP_EPAP), calcitems[9].color));
            tmp += e50 - tmp;

            float e90 = day->calcPercentile(CPAP_EPAP);
            slices.append(SummaryChartSlice(&calcitems[10], e90, e90 - tmp, day->calcPercentileLabel(CPAP_EPAP), calcitems[10].color));
            tmp += e90 - tmp;

            float i50 = day->calcMiddle(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[11], i50, i50 - tmp, day->calcMiddleLabel(CPAP_IPAP), calcitems[11].color));
            tmp += i50 - tmp;

            float i90 = day->calcPercentile(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[12], i90, i90 - tmp, day->calcPercentileLabel(CPAP_IPAP), calcitems[12].color));
            tmp += i90 - tmp;
        }
        slices.append(SummaryChartSlice(&calcitems[8], ipap, ipap - tmp, schema::channel[CPAP_IPAPHi].label(), calcitems[8].color));
    } else if ((mode == MODE_BILEVEL_AUTO_VARIABLE_PS) || (mode == MODE_ASV_VARIABLE_EPAP)) {
        float epap = day->settings_max(CPAP_EPAPLo);
        tmp = epap;

        slices.append(SummaryChartSlice(&calcitems[7], epap, epap, schema::channel[CPAP_EPAPLo].label(), calcitems[7].color));
        if (!day->summaryOnly()) {
            float e50 = day->calcMiddle(CPAP_EPAP);
            slices.append(SummaryChartSlice(&calcitems[9], e50, e50 - tmp, day->calcMiddleLabel(CPAP_EPAP), calcitems[9].color));
            tmp += e50 - tmp;

            float e90 = day->calcPercentile(CPAP_EPAP);
            slices.append(SummaryChartSlice(&calcitems[10], e90, e90 - tmp, day->calcPercentileLabel(CPAP_EPAP), calcitems[10].color));
            tmp += e90 - tmp;

            float i50 = day->calcMiddle(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[11], i50, i50 - tmp, day->calcMiddleLabel(CPAP_IPAP), calcitems[11].color));
            tmp += i50 - tmp;

            float i90 = day->calcPercentile(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[12], i90, i90 - tmp, day->calcPercentileLabel(CPAP_IPAP), calcitems[12].color));
            tmp += i90 - tmp;
        }
        float ipap = day->settings_max(CPAP_IPAPHi);
        slices.append(SummaryChartSlice(&calcitems[8], ipap, ipap - tmp, schema::channel[CPAP_IPAPHi].label(), calcitems[8].color));
    } else if (mode == MODE_ASV) {
        float epap = day->settings_max(CPAP_EPAP);
        tmp = epap;

        slices.append(SummaryChartSlice(&calcitems[5], epap, epap, schema::channel[CPAP_EPAP].label(), calcitems[5].color));
        if (!day->summaryOnly()) {
            float i50 = day->calcMiddle(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[11], i50, i50 - tmp, day->calcMiddleLabel(CPAP_IPAP), calcitems[11].color));
            tmp += i50 - tmp;

            float i90 = day->calcPercentile(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[12], i90, i90 - tmp, day->calcPercentileLabel(CPAP_IPAP), calcitems[12].color));
            tmp += i90 - tmp;
        }
        float ipap = day->settings_max(CPAP_IPAPHi);
        slices.append(SummaryChartSlice(&calcitems[8], ipap, ipap - tmp, schema::channel[CPAP_IPAPHi].label(), calcitems[8].color));
    }

}
