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
    addCalc(CPAP_Pressure, ST_SETMAX);
    addCalc(CPAP_Pressure, ST_MID);
    addCalc(CPAP_Pressure, ST_90P);
    addCalc(CPAP_PressureMin, ST_SETMIN);
    addCalc(CPAP_PressureMax, ST_SETMAX);

    addCalc(CPAP_EPAP, ST_SETMAX);
    addCalc(CPAP_IPAP, ST_SETMAX);
    addCalc(CPAP_EPAPLo, ST_SETMAX);
    addCalc(CPAP_IPAPHi, ST_SETMAX);

    addCalc(CPAP_EPAP, ST_MID);
    addCalc(CPAP_EPAP, ST_90P);
    addCalc(CPAP_IPAP, ST_MID);
    addCalc(CPAP_IPAP, ST_90P);
}


int gPressureChart::addCalc(ChannelID code, SummaryType type)
{
    QColor color = schema::channel[code].defaultColor();
    if (type == ST_90P) {
        color = brighten(color, 1.33f);
    }

    int index = gSummaryChart::addCalc(code, type, color);

    // Save the code and type used to add this calculation so that getCalc()
    // can retrieve it by code and type instead of by hard-coded index.
    m_calcs[code][type] = index;

    return index;
}


SummaryCalcItem* gPressureChart::getCalc(ChannelID code, SummaryType type)
{
    return &calcitems[m_calcs[code][type]];
}


void gPressureChart::afterDraw(QPainter &, gGraph &graph, QRectF rect)
{
    QStringList presstr;

    if (getCalc(CPAP_Pressure)->cnt > 0) {
        presstr.append(channelRange(CPAP_Pressure, STR_TR_CPAP));
    }
    if (getCalc(CPAP_PressureMin, ST_SETMIN)->cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4/%5").
                arg(STR_TR_APAP).
                arg(getCalc(CPAP_PressureMin, ST_SETMIN)->min,0,'f',1).
                arg(getCalc(CPAP_Pressure, ST_MID)->mid(), 0, 'f', 1).
                arg(getCalc(CPAP_Pressure, ST_90P)->mid(),0,'f',1).
                arg(getCalc(CPAP_PressureMax, ST_SETMAX)->max, 0, 'f', 1));

    }
    if (getCalc(CPAP_EPAP)->cnt > 0) {
        presstr.append(channelRange(CPAP_EPAP, STR_TR_EPAP));
    }
    if (getCalc(CPAP_IPAP)->cnt > 0) {
        presstr.append(channelRange(CPAP_IPAP, STR_TR_IPAP));
    }
    if (getCalc(CPAP_EPAPLo)->cnt > 0) {
        presstr.append(channelRange(CPAP_EPAPLo, STR_TR_EPAPLo));
    }
    if (getCalc(CPAP_IPAPHi)->cnt > 0) {
        presstr.append(channelRange(CPAP_IPAPHi, STR_TR_IPAPHi));
    }
    QString txt = presstr.join(" ");
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);

}


QString gPressureChart::channelRange(ChannelID code, const QString & label)
{
    SummaryCalcItem* calc = getCalc(code);
    return QString("%1 %2/%3/%4").
            arg(label).
            arg(calc->min, 0, 'f', 1).
            arg(calc->mid(), 0, 'f', 1).
            arg(calc->max, 0, 'f', 1);
}


void gPressureChart::addSlice(float value, ChannelID code, SummaryType type)
{
    SummaryCalcItem* calc = getCalc(code, type);
    float height = value - m_height;
    QString label;

    switch (type) {
    case ST_SETMIN:
    case ST_SETMAX:
        label = schema::channel[code].label();
        break;
    case ST_MID:
        label = m_day->calcMiddleLabel(code);
        break;
    case ST_90P:
        label = m_day->calcPercentileLabel(code);
        break;
    default:
        qWarning() << "Unsupported summary type in gPressureChart";
        break;
    }

    m_slices->append(SummaryChartSlice(calc, value, height, label, calc->color));
    m_height += height;
}


void gPressureChart::populate(Day * day, int idx)
{
    CPAPMode mode =  (CPAPMode)(int)qRound(day->settings_wavg(CPAP_Mode));
    m_day = day;
    m_slices = &cache[idx];
    m_height = 0;

    if (mode == MODE_CPAP) {
        float pr = day->settings_max(CPAP_Pressure);
        addSlice(pr, CPAP_Pressure);
    } else if (mode == MODE_APAP) {
        float min = day->settings_min(CPAP_PressureMin);
        float max = day->settings_max(CPAP_PressureMax);

        addSlice(min, CPAP_PressureMin, ST_SETMIN);
        if (!day->summaryOnly()) {
            float med = day->calcMiddle(CPAP_Pressure);
            addSlice(med, CPAP_Pressure, ST_MID);

            float p90 = day->calcPercentile(CPAP_Pressure);
            addSlice(p90, CPAP_Pressure, ST_90P);
        }
        addSlice(max, CPAP_PressureMax, ST_SETMAX);

    } else if (mode == MODE_BILEVEL_FIXED) {
        float epap = day->settings_max(CPAP_EPAP);
        float ipap = day->settings_max(CPAP_IPAP);

        addSlice(epap, CPAP_EPAP);
        addSlice(ipap, CPAP_IPAP);

    } else if (mode == MODE_BILEVEL_AUTO_FIXED_PS) {
        float epap = day->settings_max(CPAP_EPAPLo);
        float ipap = day->settings_max(CPAP_IPAPHi);

        addSlice(epap, CPAP_EPAPLo);
        if (!day->summaryOnly()) {

            float e50 = day->calcMiddle(CPAP_EPAP);
            addSlice(e50, CPAP_EPAP, ST_MID);

            float e90 = day->calcPercentile(CPAP_EPAP);
            addSlice(e90, CPAP_EPAP, ST_90P);

            float i50 = day->calcMiddle(CPAP_IPAP);
            addSlice(i50, CPAP_IPAP, ST_MID);

            float i90 = day->calcPercentile(CPAP_IPAP);
            addSlice(i90, CPAP_IPAP, ST_90P);
        }
        addSlice(ipap, CPAP_IPAPHi);
    } else if ((mode == MODE_BILEVEL_AUTO_VARIABLE_PS) || (mode == MODE_ASV_VARIABLE_EPAP)) {
        float epap = day->settings_max(CPAP_EPAPLo);

        addSlice(epap, CPAP_EPAPLo);
        if (!day->summaryOnly()) {
            float e50 = day->calcMiddle(CPAP_EPAP);
            addSlice(e50, CPAP_EPAP, ST_MID);

            float e90 = day->calcPercentile(CPAP_EPAP);
            addSlice(e90, CPAP_EPAP, ST_90P);

            float i50 = day->calcMiddle(CPAP_IPAP);
            addSlice(i50, CPAP_IPAP, ST_MID);

            float i90 = day->calcPercentile(CPAP_IPAP);
            addSlice(i90, CPAP_IPAP, ST_90P);
        }
        float ipap = day->settings_max(CPAP_IPAPHi);
        addSlice(ipap, CPAP_IPAPHi);
    } else if (mode == MODE_ASV) {
        float epap = day->settings_max(CPAP_EPAP);

        addSlice(epap, CPAP_EPAP);
        if (!day->summaryOnly()) {
            float i50 = day->calcMiddle(CPAP_IPAP);
            addSlice(i50, CPAP_IPAP, ST_MID);

            float i90 = day->calcPercentile(CPAP_IPAP);
            addSlice(i90, CPAP_IPAP, ST_90P);
        }
        float ipap = day->settings_max(CPAP_IPAPHi);
        addSlice(ipap, CPAP_IPAPHi);
    }
}
