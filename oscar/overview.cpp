/* Overview GUI Implementation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define xDEBUG_FUNCTIONS
#ifdef DEBUG_FUNCTIONS
#include <QRegularExpression>
#define DEBUGQ  qDebug()
#define DEBUGL  qDebug()<<QString(basename( __FILE__)).remove(QRegularExpression("\\..*$")) << __LINE__
#define DEBUGF  qDebug()<<QString(basename( __FILE__)).remove(QRegularExpression("\\..*$")) << __LINE__ << __func__
#define DEBUGTF qDebug()<<QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz") << QString(basename( __FILE__)).remove(QRegularExpression("\\..*$")) << __LINE__ << __func__
#define DEBUGT qDebug()<<QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz") 

#define O( XX ) " " #XX ":" << XX
#define OO( XX , YY ) " " #XX ":" << YY
#define NAME( id) schema::channel[ id ].label()
#define DATE( XX ) QDateTime::fromMSecsSinceEpoch(XX).toString("dd MMM yyyy")
#define DATETIME( XX ) QDateTime::fromMSecsSinceEpoch(XX).toString("dd MMM yyyy hh:mm:ss.zzz")
#endif


#include <QCalendarWidget>
#include <QTextCharFormat>
#include <QDebug>
#include <QDateTimeEdit>
#include <QCalendarWidget>
#include <QFileDialog>
#include <QMessageBox>

#include "SleepLib/profiles.h"
#include "overview.h"
#include "ui_overview.h"
#include "common_gui.h"
#include "Graphs/gXAxis.h"
#include "Graphs/gLineChart.h"
#include "Graphs/gYAxis.h"
#include "Graphs/gPressureChart.h"
#include "cprogressbar.h"

#include "mainwindow.h"
extern MainWindow *mainwin;


qint64 convertDateToTimeRtn(const QDate &date,int hours,int min,int sec) {
    return QDateTime(date).addSecs(((hours*60+min)*60)+sec).toMSecsSinceEpoch();
}
qint64 convertDateToStartTime(const QDate &date) {
    return convertDateToTimeRtn(date,0,10,0);
}
qint64 convertDateToEndTime(const QDate &date) {
    return convertDateToTimeRtn(date,23,0,0);
}
QDate convertTimeToDate(qint64 time) {
    return QDateTime::fromMSecsSinceEpoch(time).date();
}

Overview::Overview(QWidget *parent, gGraphView *shared) :
    QWidget(parent),
    ui(new Ui::Overview),
    m_shared(shared)
{
    chartsToBeMonitored.clear();
    chartsEmpty.clear();;
    ui->setupUi(this);

    // Set Date controls locale to 4 digit years
    QLocale locale = QLocale::system();
    QString shortformat = locale.dateFormat(QLocale::ShortFormat);

    if (!shortformat.toLower().contains("yyyy")) {
        shortformat.replace("yy", "yyyy");
    }

    ui->toggleVisibility->setVisible(false);    /* get rid of tiny triangle that disables data display */

    ui->dateStart->setDisplayFormat(shortformat);
    ui->dateEnd->setDisplayFormat(shortformat);

    Qt::DayOfWeek dow = firstDayOfWeekFromLocale();

    ui->dateStart->calendarWidget()->setFirstDayOfWeek(dow);
    ui->dateEnd->calendarWidget()->setFirstDayOfWeek(dow);


    // Stop both calendar drop downs highlighting weekends in red
    QTextCharFormat format = ui->dateStart->calendarWidget()->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    ui->dateStart->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->dateStart->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    ui->dateEnd->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->dateEnd->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);

    // Connect the signals to update which days have CPAP data when the month is changed
    connect(ui->dateStart->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateStart_currentPageChanged(int, int)));
    connect(ui->dateEnd->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateEnd_currentPageChanged(int, int)));
    QVBoxLayout *framelayout = new QVBoxLayout;
    ui->graphArea->setLayout(framelayout);

    QFrame *border = new QFrame(ui->graphArea);

    framelayout->setMargin(1);
    border->setFrameShape(QFrame::StyledPanel);
    framelayout->addWidget(border,1);

    ///////////////////////////////////////////////////////////////////////////////
    // Create the horizontal layout to hold the GraphView object and it's scrollbar
    ///////////////////////////////////////////////////////////////////////////////
    layout = new QHBoxLayout(border);
    layout->setSpacing(0); // remove the ugly margins/spacing
    layout->setMargin(0);
    layout->setContentsMargins(0, 0, 0, 0);
    border->setLayout(layout);
    border->setAutoFillBackground(false);

    ///////////////////////////////////////////////////////////////////////////////
    // Create the GraphView Object
    ///////////////////////////////////////////////////////////////////////////////
    GraphView = new gGraphView(ui->graphArea, m_shared, this);
    GraphView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    GraphView->setEmptyText(STR_Empty_NoData);

    // Create the custom scrollbar and attach to GraphView
    scrollbar = new MyScrollBar(ui->graphArea);
    scrollbar->setOrientation(Qt::Vertical);
    scrollbar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    scrollbar->setMaximumWidth(20);
    GraphView->setScrollBar(scrollbar);

    // Add the graphView and scrollbar to the layout.
    layout->addWidget(GraphView, 1);
    layout->addWidget(scrollbar, 0);
    layout->layout();

    ///////////////////////////////////////////////////////////////////////////////
    // Create date display at bottom of page
    ///////////////////////////////////////////////////////////////////////////////
    dateLabel = new MyLabel(this);
    dateLabel->setAlignment(Qt::AlignVCenter);
    dateLabel->setText("[Date Widget]");
    QFont font = dateLabel->font();
    font.setPointSizeF(font.pointSizeF()*1.3F);
    dateLabel->setFont(font);
    QPalette palette = dateLabel->palette();
    palette.setColor(QPalette::Base, Qt::blue);
    dateLabel->setPalette(palette);

    ui->dateLayout->addWidget(dateLabel,1);

    ///////////////////////////////////////////////////////////////////////////////
    // Rebuild contents
    ///////////////////////////////////////////////////////////////////////////////
    RebuildGraphs(false);

    ui->rangeCombo->setCurrentIndex(p_profile->general->lastOverviewRange());

    icon_on = new QIcon(":/icons/session-on.png");
    icon_off = new QIcon(":/icons/session-off.png");
    icon_up_down = new QIcon(":/icons/up-down.png");
    icon_warning = new QIcon(":/icons/warning.png");

    GraphView->resetLayout();
    GraphView->SaveDefaultSettings();
    GraphView->LoadSettings("Overview"); //no trans

    GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));

    connect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    connect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    connect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));
    connect(GraphView, SIGNAL(XBoundsChanged(qint64 ,qint64)), this, SLOT(on_XBoundsChanged(qint64 ,qint64)));
}

Overview::~Overview()
{
    disconnect(GraphView, SIGNAL(XBoundsChanged(qint64 ,qint64)), this, SLOT(on_XBoundsChanged(qint64 ,qint64)));
    disconnect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));
    disconnect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    disconnect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    disconnect(ui->dateEnd->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateEnd_currentPageChanged(int, int)));
    disconnect(ui->dateStart->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateStart_currentPageChanged(int, int)));
    disconnectgSummaryCharts() ;

    // Save graph orders and pin status, etc...
    GraphView->SaveSettings("Overview");//no trans

    delete ui;
}

void Overview::ResetFont()
{
    QFont font = QApplication::font();
    font.setPointSizeF(font.pointSizeF()*1.3F);
    dateLabel->setFont(font);
}


void Overview::connectgSummaryCharts()
{
    for (auto it = chartsToBeMonitored.begin(); it != chartsToBeMonitored.end(); ++it) {
        gSummaryChart* sc =it.key();
        connect(sc, SIGNAL(summaryChartEmpty(gSummaryChart*,qint64,qint64,bool)), this, SLOT(on_summaryChartEmpty(gSummaryChart*,qint64,qint64,bool)));
    }
}

void Overview::disconnectgSummaryCharts()
{
    for (auto it = chartsToBeMonitored.begin(); it != chartsToBeMonitored.end(); ++it) {
        gSummaryChart* sc =it.key();
        disconnect(sc, SIGNAL(summaryChartEmpty(gSummaryChart*,qint64,qint64,bool)), this, SLOT(on_summaryChartEmpty(gSummaryChart*,qint64,qint64,bool)));
    }
    chartsToBeMonitored.clear();
    chartsEmpty.clear();;
}

void Overview::on_summaryChartEmpty(gSummaryChart*sc,qint64 firstI,qint64 lastI,bool empty)
{

    auto it = chartsToBeMonitored.find(sc);
    if (it==chartsToBeMonitored.end()) {
        return;
    }
    gGraph* graph=it.value();
    if (empty) {
        // on next range change allow empty flag to be recalculated
        chartsEmpty.insert(sc,graph);
        //DEBUGF << graph->name() << "Chart is Empty" << "Range" << convertTimeToDate(firstI) << convertTimeToDate(lastI);
    } else {
        // The chart has some entry with data.
        chartsEmpty.remove(sc);
        updateGraphCombo();
        //DEBUGF << graph->name() << "Chart is enabled with range:" << convertTimeToDate(firstI) << "==>" << convertTimeToDate(lastI) ;
    }
    Q_UNUSED(firstI);
    Q_UNUSED(lastI);
};


// Create all the graphs for the Overview page
void Overview::CreateAllGraphs() {
    ///////////////////////////////////////////////////////////////////////////////
    // Add all the graphs
    // Process is to createGraph() to make the graph object, then add a layer
    // that provides the contents for that graph.
    ///////////////////////////////////////////////////////////////////////////////
    if (chartsToBeMonitored.size()>0) {
        disconnectgSummaryCharts();
    }
    chartsEmpty.clear();;
    chartsToBeMonitored.clear();;

    // Add graphs that are always included
    ChannelID ahicode = p_profile->general->calculateRDI() ? CPAP_RDI : CPAP_AHI;
    if (ahicode == CPAP_RDI) {
        AHI = createGraph("AHIBreakdown", STR_TR_RDI, tr("Respiratory\nDisturbance\nIndex"));
    } else {
        AHI = createGraph("AHIBreakdown", STR_TR_AHI, tr("Apnea\nHypopnea\nIndex"));
    }

    ahi = new gAHIChart();
    AHI->AddLayer(ahi);
    //chartsToBeMonitored.insert(ahi,AHI);

    UC = createGraph(STR_GRAPH_Usage, tr("Usage"), tr("Usage\n(hours)"));
    UC->AddLayer(uc = new gUsageChart());
    //chartsToBeMonitored.insert(uc,UC);

    STG = createGraph("New Session", tr("Session Times"), tr("Session Times"),  YT_Time);
    stg = new gSessionTimesChart();
    STG->AddLayer(stg);

    PR = createGraph("Pressure Settings", STR_TR_Pressure, STR_TR_Pressure + "\n(" + STR_UNIT_CMH2O + ")");
    pres = new gPressureChart();
    PR->AddLayer(pres);
    //chartsToBeMonitored.insert(pres,PR);

    TTIA = createGraph("TTIA", tr("Total Time in Apnea"), tr("Total Time in Apnea\n(Minutes)"));
    ttia = new gTTIAChart();
    TTIA->AddLayer(ttia);
    //chartsToBeMonitored.insert(ttia,TTIA);

    // Add graphs for all channels that have been marked in Preferences Dialog as wanting a graph
    QHash<ChannelID, schema::Channel *>::iterator chit;
    QHash<ChannelID, schema::Channel *>::iterator chit_end = schema::channel.channels.end();
    for (chit = schema::channel.channels.begin(); chit != chit_end; ++chit) {
        schema::Channel * chan = chit.value();

        if (chan->showInOverview()) {
            ChannelID code = chan->id();
            QString name = chan->fullname();
            if (name.length() > 16) name = chan->label();
            gGraph *G = createGraph(chan->code(), name, chan->description());
            gSummaryChart * sc = nullptr;
            if ((chan->type() == schema::FLAG) || (chan->type() == schema::MINOR_FLAG)) {
                sc = new gSummaryChart(chan->code(), chan->machtype()); // gts was MT_CPAP
                sc->addCalc(code, ST_CPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
                chartsToBeMonitored.insert(sc,G);
            } else if (chan->type() == schema::SPAN) {
                sc = new gSummaryChart(chan->code(), MT_CPAP);
                sc->addCalc(code, ST_SPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
                chartsToBeMonitored.insert(sc,G);
            } else if (chan->type() == schema::WAVEFORM) {
                sc= new gSummaryChart(code, chan->machtype());
                G->AddLayer(sc);
                chartsToBeMonitored.insert(sc,G);
            } else if (chan->type() == schema::UNKNOWN) {
                sc = new gSummaryChart(chan->code(), MT_CPAP);
                sc->addCalc(code, ST_CPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
                chartsToBeMonitored.insert(sc,G);
            } 
            if (sc== nullptr) {
                //DEBUGF << "Channel" << name << "type" << chan->type() << "machine type" << chan->machtype() << "IGNORED";
            } else {
                sc ->reCalculate();
                //DEBUGF << "Channel" << name << "type" << chan->type() << "machine type" << chan->machtype() << OO(Empty,sc->isEmpty());
            }
        } // if showInOverview()
    } // for chit
    // Note The following don not use gSummaryChart. They use SummaryChart instead. and can not be monitored.
    WEIGHT = createGraph(STR_GRAPH_Weight, STR_TR_Weight, STR_TR_Weight, YT_Weight);
    weight = new SummaryChart("Weight", GT_LINE);
    weight->setMachineType(MT_JOURNAL);
    weight->addSlice(Journal_Weight, QColor("black"), ST_SETAVG);
    WEIGHT->AddLayer(weight);

    BMI = createGraph(STR_GRAPH_BMI, STR_TR_BMI, tr("Body\nMass\nIndex"));
    bmi = new SummaryChart("BMI", GT_LINE);
    bmi->setMachineType(MT_JOURNAL);
    bmi->addSlice(Journal_BMI, QColor("black"), ST_SETAVG);
    BMI->AddLayer(bmi);

    ZOMBIE = createGraph(STR_GRAPH_Zombie, STR_TR_Zombie, tr("How you felt\n(0-10)"));
    zombie = new SummaryChart("Zombie", GT_LINE);
    zombie->setMachineType(MT_JOURNAL);
    zombie->addSlice(Journal_ZombieMeter, QColor("black"), ST_SETAVG);
    ZOMBIE->AddLayer(zombie);

    connectgSummaryCharts();
}


// Recalculates Overview chart info
void Overview::RebuildGraphs(bool reset)
{
    qint64 minx, maxx;
    if (reset) {
        GraphView->GetXBounds(minx, maxx);
    }

    minRangeStartDate=p_profile->LastDay(MT_CPAP);
    maxRangeEndDate=minRangeStartDate.addDays(-1);      // force a range change;
    disconnectgSummaryCharts() ;
    GraphView->trashGraphs(true);       // Remove all existing graphs
    CreateAllGraphs();

    if (reset) {
        GraphView->resetLayout();
        GraphView->setDay(nullptr);
        SetXBounds(minx, maxx, 0, false);
        GraphView->resetLayout();
        updateGraphCombo();
    }
}

// Create an overview graph, adding it to the overview gGraphView object
// param QString name  The title of the graph
// param QString units The units of measurements to show in the popup
gGraph *Overview::createGraph(QString code, QString name, QString units, YTickerType yttype)
{
    int default_height = AppSetting->graphHeight();
    gGraph *g = new gGraph(code, GraphView, name, units, default_height, 0);

    gYAxis *yt;

    switch (yttype) {
    case YT_Time:
        yt = new gYAxisTime(true); // Time scale
        break;

    case YT_Weight:
        yt = new gYAxisWeight(p_profile->general->unitSystem());
        break;

    default:
        yt = new gYAxis(); // Plain numeric scale
        break;
    }

    g->AddLayer(yt, LayerLeft, gYAxis::Margin);
    gXAxisDay *x = new gXAxisDay();
    g->AddLayer(x, LayerBottom, 0, gXAxisDay::Margin);
    g->AddLayer(new gXGrid());
    return g;
}

void Overview::on_LineCursorUpdate(double time)
{
    if (time > 1) {
        // even though the generated string is displayed to the user
        // no time zone conversion is neccessary, so pass UTC
        // to prevent QT from automatically converting to local time
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(time, Qt::LocalTime/*, Qt::UTC*/);
        QString txt = dt.toString("dd MMM yyyy (dddd)");
        dateLabel->setText(txt);
    } else dateLabel->setText(QString(GraphView->emptyText()));
}

void Overview::on_RangeUpdate(double minx, double /* maxx */)
{
    if (minx > 1) {
        dateLabel->setText(GraphView->getRangeInDaysString());
    } else {
        dateLabel->setText(QString(GraphView->emptyText()));
    }
}

void Overview::ReloadGraphs()
{
    GraphView->setDay(nullptr);
    updateCube();

    on_rangeCombo_activated(ui->rangeCombo->currentIndex());
}

void Overview::setGraphText () {
    int numOff = 0;
    int numTotal = 0;

    gGraph *g;
    for (int i=0;i<GraphView->size();i++) {
        g=(*GraphView)[i];
        if (!g->isEmpty()) {
            numTotal++;
            if (!g->visible()) {
                numOff++;
            }
        }
    }
    ui->graphCombo->setItemIcon(0, numOff ? *icon_warning : *icon_up_down);
    QString graphText;
    if (numOff == 0) graphText = QObject::tr("%1 Charts").arg(numTotal);
    else             graphText = QObject::tr("%1 of %2 Charts").arg(numTotal-numOff).arg(numTotal);
    ui->graphCombo->setItemText(0, graphText);
}

void Overview::updateGraphCombo()
{
    ui->graphCombo->clear();
    gGraph *g;

    ui->graphCombo->addItem(*icon_up_down, tr("10 of 10 Charts"), true); // Translation only to define space required
    for (int i = 0; i < GraphView->size(); i++) {
        g = (*GraphView)[i];

        if (g->isEmpty()) { continue; }

        if (g->visible()) {
            ui->graphCombo->addItem(*icon_on, g->title(), true);
        } else {
            ui->graphCombo->addItem(*icon_off, g->title(), false);
        }
    }

    ui->graphCombo->setCurrentIndex(0);
    setGraphText();
    updateCube();
}

void Overview::RedrawGraphs()
{
    GraphView->redraw();
}

// Updates calendar format and profile data.
void Overview::UpdateCalendarDay(QDateEdit *dateedit, QDate date)
{
    QCalendarWidget *calendar = dateedit->calendarWidget();
    QTextCharFormat bold;
    QTextCharFormat cpapcol;
    QTextCharFormat normal;
    QTextCharFormat oxiday;
    bold.setFontWeight(QFont::Bold);
    cpapcol.setForeground(QBrush(Qt::blue, Qt::SolidPattern));
    cpapcol.setFontWeight(QFont::Bold);
    oxiday.setForeground(QBrush(Qt::red, Qt::SolidPattern));
    oxiday.setFontWeight(QFont::Bold);
    bool hascpap = p_profile->FindDay(date, MT_CPAP) != nullptr;
    bool hasoxi = p_profile->FindDay(date, MT_OXIMETER) != nullptr;
    //bool hasjournal=p_profile->GetDay(date,MT_JOURNAL)!=nullptr;

    if (hascpap) {
        if (hasoxi) {
            calendar->setDateTextFormat(date, oxiday);
        } else {
            calendar->setDateTextFormat(date, cpapcol);
        }
    } else if (p_profile->GetDay(date)) {
        calendar->setDateTextFormat(date, bold);
    } else {
        calendar->setDateTextFormat(date, normal);
    }

    calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
}

void Overview::SetXBounds(qint64 start, qint64 end, short group , bool refresh )
{
     GraphView->SetXBounds( start , end ,group,refresh);
}

void Overview::on_XBoundsChanged(qint64 start,qint64 end)
{
    displayStartDate = convertTimeToDate(start);
    displayEndDate = convertTimeToDate(end);

    bool largerRange=false;
    if (displayStartDate>maxRangeEndDate || minRangeStartDate>displayEndDate) {
        // have non-overlaping ranges 
        // Only occurs when custom mode is switched to/from a latest mode. custom mode to/from last week.
        // All other displays expand the existing range.
        // reset all empty flags to not empty 
        if (displayStartDate>maxRangeEndDate) {
            //DEBUGF << "Two ranges" O(displayStartDate) <<">" << O(maxRangeEndDate);
        }
        if (minRangeStartDate>displayEndDate) {
            //DEBUGF << "Two ranges" O(minRangeStartDate) <<">" << O(displayEndDate);
        }
        largerRange=true;
        chartsEmpty = QHash<gSummaryChart*, gGraph*>( chartsToBeMonitored );
        minRangeStartDate = displayStartDate;
        maxRangeEndDate   = displayEndDate;
    } else {
        // new range overlaps with old range
        if (displayStartDate<minRangeStartDate) {
            //DEBUGF << "Start lower" <<O(minRangeStartDate)<< ">" <<O(displayStartDate);
            largerRange=true;
            minRangeStartDate = displayStartDate;
        }
        if (displayEndDate>maxRangeEndDate) {
            //DEBUGF << "End Higher" <<O(maxRangeEndDate)<< "<" <<O(displayEndDate);
            largerRange=true;
            maxRangeEndDate   = displayEndDate;
        }
    }
    if (!largerRange) {
        if (displayStartDate<minRangeStartDate ) {
            //DEBUGF << "ERROR" <<O(minRangeStartDate)<< "==" <<O(displayStartDate);
        } 
        if (displayEndDate>maxRangeEndDate) {
            //DEBUGF << "ERROR" <<O(maxRangeEndDate)<< "==" <<O(displayEndDate);
        }
    }

    if (largerRange) {
        for (auto it= chartsEmpty.begin();it!=chartsEmpty.end();it++) {
            gSummaryChart* sc = it.key();
            bool empty=sc->isEmpty();
            if (empty) {
                sc ->reCalculate();
                GraphView->updateScale();
                GraphView->redraw();
                GraphView->timedRedraw(150);
            }
        }
        chartsEmpty.clear();
        updateGraphCombo();
    }
}

void Overview::dateStart_currentPageChanged(int year, int month)
{
    QDate d(year, month, 1);
    int dom = d.daysInMonth();

    for (int i = 1; i <= dom; i++) {
        d = QDate(year, month, i);
        UpdateCalendarDay(ui->dateStart, d);
    }
}

void Overview::dateEnd_currentPageChanged(int year, int month)
{
    QDate d(year, month, 1);
    int dom = d.daysInMonth();

    for (int i = 1; i <= dom; i++) {
        d = QDate(year, month, i);
        UpdateCalendarDay(ui->dateEnd, d);
    }
}


void Overview::on_dateEnd_dateChanged(const QDate &date)
{
    qint64 d1 = qint64(QDateTime(ui->dateStart->date(), QTime(0, 10, 0)/*, Qt::UTC*/).toTime_t()) * 1000L;
    qint64 d2 = qint64(QDateTime(date, QTime(23, 0, 0)/*, Qt::UTC*/).toTime_t()) * 1000L;
    SetXBounds(d1, d2);
    ui->dateStart->setMaximumDate(date);
    if (customMode) {
        p_profile->general->setCustomOverviewRangeEnd(date);
    }
}

void Overview::on_dateStart_dateChanged(const QDate &date)
{
    qint64 d1 = qint64(QDateTime(date, QTime(0, 10, 0)/*, Qt::UTC*/).toTime_t()) * 1000L;
    qint64 d2 = qint64(QDateTime(ui->dateEnd->date(), QTime(23, 0, 0)/*, Qt::UTC*/).toTime_t()) * 1000L;
    SetXBounds(d1, d2);
    ui->dateEnd->setMinimumDate(date);
    if (customMode) {
        p_profile->general->setCustomOverviewRangeStart(date);
    }
}

// Zoom to 100% button clicked or called back from 100% zoom in popup menu
void Overview::on_zoomButton_clicked()
{
    qint64 d1 = qint64(QDateTime(ui->dateStart->date(), QTime(0, 10, 0)/*, Qt::UTC*/).toTime_t()) * 1000L;  // GTS why UTC?
    qint64 d2 = qint64(QDateTime(ui->dateEnd->date(), QTime(23, 0, 0)/*, Qt::UTC*/).toTime_t()) * 1000L;  // Interesting: start date set to 10 min after midnight, ending at 11 pm
    SetXBounds(d1, d2);
}

void Overview::ResetGraphLayout()
{
    GraphView->resetLayout();
}

void Overview::ResetGraphOrder(int type)
{
    Q_UNUSED(type)
    GraphView->resetGraphOrder(false);
    ResetGraphLayout();
}

// Process new range selection from combo button
void Overview::on_rangeCombo_activated(int index)
{
    // Block signal so that graphs are not updated for these.
    ui->dateEnd->blockSignals(true);
    ui->dateStart->blockSignals(true);
    ui->dateStart->setMinimumDate(p_profile->FirstDay());  // first and last dates for ANY machine type
    ui->dateEnd->setMaximumDate(p_profile->LastDay());
    // these signals will be reenabled in setRange.
    //ui->dateEnd->blockSignals(false);
    //ui->dateStart->blockSignals(false);

    // Exclude Journal in calculating the last day
    QDate end = p_profile->LastDay(MT_CPAP);
    end = max(end, p_profile->LastDay(MT_OXIMETER));
    end = max(end, p_profile->LastDay(MT_POSITION));
    end = max(end, p_profile->LastDay(MT_SLEEPSTAGE));
    QDate start;


    if (index == 0) {
        start = end.addDays(-6);
    } else if (index == 1) {
        start = end.addDays(-13);
    } else if (index == 2) {
        start = end.addMonths(-1).addDays(1);
    } else if (index == 3) {
        start = end.addMonths(-2).addDays(1);
    } else if (index == 4) {
        start = end.addMonths(-3).addDays(1);
    } else if (index == 5) {
        start = end.addMonths(-6).addDays(1);
    } else if (index == 6) {
        start = end.addYears(-1).addDays(1);
    } else if (index == 7) { // Everything
        start = p_profile->FirstDay();
    } else if (index == 8 || index == 9) { // Custom
        // Validate save Overview Custom Range for first access.
        if (!p_profile->general->customOverviewRangeStart().isValid()
            || (!p_profile->general->customOverviewRangeEnd().isValid() )
            || (index==9 /* New Coustom mode - to reset custom range to displayed date range*/)
        ) {
            // Reset Custom Range to current range displayed
            // on first initialization of this version of OSCAR
            // or on new custom Mode to reset range.
            qint64 istart,iend;
            GraphView->GetXBounds(istart , iend);
            start = QDateTime::fromMSecsSinceEpoch( istart ).date();
            end =   QDateTime::fromMSecsSinceEpoch( iend ).date();
            p_profile->general->setCustomOverviewRangeStart(start);
            p_profile->general->setCustomOverviewRangeEnd(end);
            index=8;
            ui->rangeCombo->setCurrentIndex(index);
        } else if (customMode) {      // last mode was custom.
            // Reset Custom Range to current range in calendar widget
            // Custom mode MUST be initialized to false when the Custom Instance is created.
            start = ui->dateStart->date();
            end = ui->dateEnd->date();
            p_profile->general->setCustomOverviewRangeStart(start);
            p_profile->general->setCustomOverviewRangeEnd(end);
        } else {
            // have a change in RangeCombo selection. Use last saved values.
            start = p_profile->general->customOverviewRangeStart() ;
            end   = p_profile->general->customOverviewRangeEnd() ;
        }
    }

    if (start < p_profile->FirstDay()) { start = p_profile->FirstDay(); }

    customMode = (index == 8) ;
    ui->dateStartLabel->setEnabled(customMode);
    ui->dateEndLabel->setEnabled(customMode);
    ui->dateEnd->setEnabled(customMode);
    ui->dateStart->setEnabled(customMode);


    p_profile->general->setLastOverviewRange(index);  // type of range in last use

    // Ensure that all summary files are available and update version numbers if required
    int size = start.daysTo(end);
    qDebug() << "Overview range combo from" << start << "to" << end << "with" << size << "days";
    QDate dateback = end;
    CProgressBar * progress = new CProgressBar (QObject::tr("Loading summaries"), mainwin, size);
    for (int i=1; i < size; ++i) {
        progress->add(1);
        auto di = p_profile->daylist.find(dateback);
        dateback = dateback.addDays(-1);
        if (di == p_profile->daylist.end())  // Check for no Day entry
           continue;
        Day * day = di.value();
        if (!day)
            continue;
        if (day->size() <= 0)
            continue;
        day->OpenSummary();     // This can be slow if summary needs to be updated to new version
    }
    progress->close();
    delete progress;

    // first and last dates for ANY machine type
    //uiStartDate=start;
    //uiEndDate=end;
    setRange(start, end);
}

// Saves dates in UI, clicks zoom button, and updates combo box
// 1. Updates the dates in the start / end date boxs
// 2. optionally also changes display range for graphs.
void Overview::setRange(QDate& start, QDate& end, bool updateGraphs/*zoom*/)
{
    ui->dateEnd->blockSignals(true);
    ui->dateStart->blockSignals(true);

    ui->dateStart->setMaximumDate(end);
    ui->dateEnd->setMinimumDate(start);
    ui->dateStart->setDate(start);
    ui->dateEnd->setDate(end);

    ui->dateEnd->blockSignals(false);
    ui->dateStart->blockSignals(false);
    if (updateGraphs) this->on_zoomButton_clicked();  // Click on zoom-out to 100% button
    updateGraphCombo();
}

void Overview::on_graphCombo_activated(int index)
{
    if (index < 0) {
        return;
    }

    if (index > 0 ) {
        gGraph *g;
        QString s;
        s = ui->graphCombo->currentText();
        bool b = !ui->graphCombo->itemData(index, Qt::UserRole).toBool();
        ui->graphCombo->setItemData(index, b, Qt::UserRole);

        if (b) {
            ui->graphCombo->setItemIcon(index, *icon_on);
        } else {
            ui->graphCombo->setItemIcon(index, *icon_off);
        }

        g = GraphView->findGraphTitle(s);
        g->setVisible(b);
    }
    ui->graphCombo->setCurrentIndex(0);
    updateCube();
    setGraphText();
    GraphView->updateScale();
    GraphView->redraw();
}
void Overview::updateCube()
{
    if ((GraphView->visibleGraphs() == 0)) {
        ui->toggleVisibility->setArrowType(Qt::UpArrow);
        ui->toggleVisibility->setToolTip(tr("Show all graphs"));
        ui->toggleVisibility->blockSignals(true);
        ui->toggleVisibility->setChecked(true);
        ui->toggleVisibility->blockSignals(false);

        if (ui->graphCombo->count() > 0) {
            GraphView->setEmptyText(STR_Empty_NoGraphs);

        } else {
            GraphView->setEmptyText(STR_Empty_NoData);
        }
    } else {
        ui->toggleVisibility->setArrowType(Qt::DownArrow);
        ui->toggleVisibility->setToolTip(tr("Hide all graphs"));
        ui->toggleVisibility->blockSignals(true);
        ui->toggleVisibility->setChecked(false);
        ui->toggleVisibility->blockSignals(false);
    }
}

void Overview::on_toggleVisibility_clicked(bool checked)
{
    gGraph *g;
    QString s;
    QIcon *icon = checked ? icon_off : icon_on;

    for (int i = 0; i < ui->graphCombo->count(); i++) {
        s = ui->graphCombo->itemText(i);
        ui->graphCombo->setItemIcon(i, *icon);
        ui->graphCombo->setItemData(i, !checked, Qt::UserRole);
        g = GraphView->findGraphTitle(s);
        g->setVisible(!checked);
    }

    updateCube();
    GraphView->updateScale();
    GraphView->redraw();
}
