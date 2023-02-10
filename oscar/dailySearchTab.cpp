/* user graph settings Implementation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QWidget>
#include <QTabWidget>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPixmap>
#include <QSize>
#include <QChar>
#include <QtGui>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QtGlobal>
#include <QHeaderView>
#include "dailySearchTab.h"
#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "daily.h"


#define OT_NONE              0
#define OT_DISABLED_SESSIONS 1
#define OT_NOTES             2
#define OT_NOTES_STRING      3
#define OT_BOOK_MARKS        4
#define OT_AHI               5
#define OT_SHORT_SESSIONS    6
#define OT_SESSIONS_QTY      7
#define OT_DAILY_USAGE       8

DailySearchTab::DailySearchTab(Daily* daily , QWidget* searchTabWidget  , QTabWidget* dailyTabWidget)  : 
                        daily(daily) , parent(daily) , searchTabWidget(searchTabWidget) ,dailyTabWidget(dailyTabWidget)
{
    icon_on = new QIcon(":/icons/session-on.png");
    icon_off = new QIcon(":/icons/session-off.png");


    #if 0
    // method of find the daily tabWidgets works for english.
    // the question will it work for all other langauges???
    // Right now they are const int in the header file.
    int maxIndex = dailyTabWidget->count();
    for (int index=0 ; index<maxIndex; index++) {
        QString title = dailyTabWidget->tabText(index);
        if (title.contains("detail",Qt::CaseInsensitive) )  {TW_DETAILED = index; continue;};
        if (title.contains("event",Qt::CaseInsensitive) )  {TW_EVENTS = index; continue;};
        if (title.contains("note",Qt::CaseInsensitive) )  {TW_NOTES = index; continue;};
        if (title.contains("bookmark",Qt::CaseInsensitive) )  {TW_BOOKMARK = index; continue;};
        if (title.contains("search",Qt::CaseInsensitive) )  {TW_SEARCH = index; continue;};
    }
    #endif

    createUi();
    daily->connect(enterString, SIGNAL(textEdited(QString)), this, SLOT(on_textEdited(QString)) );
    daily->connect(enterInteger, SIGNAL(valueChanged(int)), this, SLOT(on_intValueChanged(int)) );
    daily->connect(enterDouble, SIGNAL(valueChanged(double)), this, SLOT(on_doubleValueChanged(double)) );
    daily->connect(startButton, SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
    daily->connect(continueButton, SIGNAL(clicked()), this, SLOT(on_continueButton_clicked()) );
    //daily->connect(guiDisplayTable,     SIGNAL(itemActivated(QTableWidgetItem*)), this, SLOT(on_itemActivated(QTableWidgetItem*)   ));
    daily->connect(guiDisplayTable,     SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(on_itemActivated(QTableWidgetItem*)   ));
    daily->connect(guiDisplayTable,     SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(on_itemActivated(QTableWidgetItem*)   ));
    daily->connect(selectCommand,     SIGNAL(activated(int)), this, SLOT(on_selectCommand_activated(int)   ));
    daily->connect(dailyTabWidget,     SIGNAL(currentChanged(int)), this, SLOT(on_dailyTabWidgetCurrentChanged(int)   ));
}

DailySearchTab::~DailySearchTab() {
    daily->disconnect(enterString, SIGNAL(textEdited(QString)), this, SLOT(on_textEdited(QString)) );
    daily->disconnect(dailyTabWidget,     SIGNAL(currentChanged(int)), this, SLOT(on_dailyTabWidgetCurrentChanged(int)   ));
    daily->disconnect(enterInteger, SIGNAL(valueChanged(int)), this, SLOT(on_intValueChanged(int)) );
    daily->disconnect(enterDouble, SIGNAL(valueChanged(double)), this, SLOT(on_doubleValueChanged(double)) );
    daily->disconnect(selectCommand,     SIGNAL(activated(int)), this, SLOT(on_selectCommand_activated(int)   ));
    //daily->disconnect(guiDisplayTable,     SIGNAL(itemActivated(QTableWidgetItem*)), this, SLOT(on_itemActivated(QTableWidgetItem*)   ));
    daily->disconnect(guiDisplayTable,     SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(on_itemActivated(QTableWidgetItem*)   ));
    daily->disconnect(guiDisplayTable,     SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(on_itemActivated(QTableWidgetItem*)   ));
    daily->disconnect(continueButton, SIGNAL(clicked()), this, SLOT(on_continueButton_clicked()) );
    daily->disconnect(startButton, SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
    delete icon_on ;
    delete icon_off ;
};

void    DailySearchTab::createUi() {

        QFont baseFont =*defaultfont;
        baseFont.setPointSize(1+baseFont.pointSize());
        searchTabWidget ->setFont(baseFont);

        searchTabLayout  = new QVBoxLayout(searchTabWidget);
        criteriaLayout   = new QHBoxLayout();
        searchLayout     = new QHBoxLayout();
        statusLayout     = new QHBoxLayout();
        summaryLayout    = new QHBoxLayout();
        searchTabLayout  ->setObjectName(QString::fromUtf8("verticalLayout_21"));
        searchTabLayout  ->setContentsMargins(4, 4, 4, 4);

        introduction    = new QLabel(this);
        selectLabel     = new QLabel(this);
        selectCommand   = new QComboBox(this);
        startButton     = new QPushButton(this);
        continueButton  = new QPushButton(this);
        guiDisplayTable = new QTableWidget(this);
        enterDouble     = new QDoubleSpinBox(this);
        enterInteger    = new QSpinBox(this);
        enterString     = new QLineEdit(this);
        criteriaOperation = new QLabel(this);
        statusA         = new QLabel(this);
        statusB         = new QLabel(this);
        statusC         = new QLabel(this);
        summaryStatsA   = new QLabel(this);
        summaryStatsB   = new QLabel(this);
        summaryStatsC   = new QLabel(this);

        searchTabLayout ->addWidget(introduction);

        criteriaLayout  ->addWidget(selectLabel);
        criteriaLayout  ->insertStretch(1,5);
        criteriaLayout  ->addWidget(selectCommand);
        criteriaLayout  ->addWidget(criteriaOperation);
        criteriaLayout  ->addWidget(enterInteger);
        criteriaLayout  ->addWidget(enterString);
        criteriaLayout  ->addWidget(enterDouble);
        criteriaLayout  ->insertStretch(-1,5);
        searchTabLayout ->addLayout(criteriaLayout);


        searchLayout    ->addWidget(startButton);
        searchLayout    ->addWidget(continueButton);
        searchTabLayout ->addLayout(searchLayout);

        statusLayout    ->insertStretch(0,1);
        statusLayout    ->addWidget(statusA);
        statusLayout    ->addWidget(statusB);
        statusLayout    ->addWidget(statusC);
        statusLayout    ->insertStretch(-1,1);
        searchTabLayout ->addLayout(statusLayout);

        summaryLayout   ->addWidget(summaryStatsA);
        summaryLayout   ->addWidget(summaryStatsB);
        summaryLayout   ->addWidget(summaryStatsC);
        searchTabLayout ->addLayout(summaryLayout);

        DEBUGFW ;


        // searchTabLayout ->addWidget(guiDisplayList);
        searchTabLayout ->addWidget(guiDisplayTable);

        DEBUGFW ;

        searchTabWidget ->setFont(baseFont);
        guiDisplayTable->setFont(baseFont);
        guiDisplayTable->setColumnCount(2);
        guiDisplayTable->setObjectName(QString::fromUtf8("guiDisplayTable"));
        //guiDisplayTable->setAlternatingRowColors(true);
        guiDisplayTable->setSelectionMode(QAbstractItemView::SingleSelection);
        guiDisplayTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        guiDisplayTable->setSortingEnabled(true);
        QHeaderView* horizontalHeader = guiDisplayTable->horizontalHeader();
        QHeaderView* verticalHeader = guiDisplayTable->verticalHeader();
        horizontalHeader->setStretchLastSection(true);
        horizontalHeader->setVisible(false);
        horizontalHeader->setSectionResizeMode(QHeaderView::Stretch);
        //guiDisplayTable->horizontalHeader()->setDefaultSectionSize(QFontMetrics(baseFont).height());
        verticalHeader->setVisible(false);
        verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
        verticalHeader->setDefaultSectionSize(24);

        introduction    ->setText(introductionStr());
        introduction    ->setFont(baseFont);
        statusA         ->show();
        //statusB         ->show();
        //statusC         ->show();
        summaryStatsA   ->setFont(baseFont);
        summaryStatsB   ->setFont(baseFont);
        summaryStatsC   ->setFont(baseFont);
        summaryStatsA   ->show();
        summaryStatsB   ->show();
        summaryStatsC   ->show();
        enterDouble     ->hide();
        enterInteger    ->hide();
        enterString     ->hide();

        enterString->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        enterDouble->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        enterInteger->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectCommand->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

        startButton->setObjectName(QString::fromUtf8("startButton"));
        startButton->setText(tr("Start Search"));
        continueButton->setObjectName(QString::fromUtf8("continueButton"));
        continueButton->setText(tr("Continue Search"));
        continueButton->setEnabled(false);
        startButton->setEnabled(false);

        selectCommand->setObjectName(QString::fromUtf8("selectCommand"));
        selectCommand->setFont(baseFont);
}

void DailySearchTab::delayedCreateUi() {
        // meed delay to insure days are populated.
        if (createUiFinished) return;

        #if 0
        // to change alignment of a combo box
        selectCommand->setEditable(true);
        QLineEdit* lineEdit = selectCommand->lineEdit();
        lineEdit->setAlignment(Qt::AlignCenter);
        if (lineEdit) {
            lineEdit->setReadOnly(true);
        }
        #endif

        createUiFinished = true;
        selectLabel->setText(tr("Match:"));

        selectCommand->clear();
        selectCommand->addItem(tr("Select Match"),OT_NONE);
        selectCommand->insertSeparator(selectCommand->count()); 
        selectCommand->addItem(tr("Notes"),OT_NOTES);
        selectCommand->addItem(tr("Notes containng"),OT_NOTES_STRING);
        selectCommand->addItem(tr("BookMarks"),OT_BOOK_MARKS);
        selectCommand->addItem(tr("Disabled Sessions"),OT_DISABLED_SESSIONS);
        selectCommand->addItem(tr("Session Duration" ),OT_SHORT_SESSIONS);
        selectCommand->addItem(tr("Number of Sessions"),OT_SESSIONS_QTY);
        selectCommand->addItem(tr("Daily Usage"),OT_DAILY_USAGE);
        selectCommand->addItem(tr("AHI "),OT_AHI);
        selectCommand->insertSeparator(selectCommand->count()); 
        // Now add events
        QDate date = p_profile->LastDay(MT_CPAP);
        if ( !date.isValid()) return;
        Day* day = p_profile->GetDay(date);
        if (!day) return;
        // the following is copied from daily.
        quint32 chans = schema::SPAN | schema::FLAG | schema::MINOR_FLAG;
        if (p_profile->general->showUnknownFlags()) chans |= schema::UNKNOWN;
        QList<ChannelID> available;
        available.append(day->getSortedMachineChannels(chans));
        for (int i=0; i < available.size(); ++i) {
            ChannelID id = available.at(i);
            schema::Channel chan = schema::channel[ id  ];
            // new stuff now
            //QString displayName= chan.code();
            QString displayName= chan.fullname();
            //QString item = QString("%1 :%2").arg(displayName).arg(tr(""));
            selectCommand->addItem(displayName,id);
        }
}

void   DailySearchTab::selectAligment(bool withParameters) {
    if (withParameters) {
        // operand is right justified and value is left justified.
    } {
        // only operand and it is centered.
    }
}

void    DailySearchTab::on_selectCommand_activated(int index) {
        int item = selectCommand->itemData(index).toInt();
        enterDouble->hide();
        enterDouble->setDecimals(3);
        enterInteger->hide();
        enterString->hide();
        criteriaOperation->hide();
        minMaxValid = false;
        minMaxMode = none;
        minMaxUnit = noUnit;
        minMaxInteger=0;
        minMaxDouble=0.0;

        searchType = OT_NONE;
        switch (item) {
            case OT_NONE :
                break;
            case OT_DISABLED_SESSIONS :
                searchType = item;
                break;
            case OT_NOTES :
                searchType = item;
                break;
            case OT_BOOK_MARKS :
                searchType = item;
                break;
            case OT_NOTES_STRING :
                searchType = item;
                enterString->clear();
                enterString->show();
                criteriaOperation->setText(QChar(0x2208));   // use either 0x220B or 0x2208
                criteriaOperation->show();
                break;
            case OT_AHI :
                searchType = item;
                enterDouble->setRange(0,999);
                enterDouble->setValue(5.0);
                enterDouble->setDecimals(2);
                criteriaOperation->setText(">");
                criteriaOperation->show();
                enterDouble->show();
                minMaxMode = maxDouble;
                break;
            case OT_SHORT_SESSIONS :
                searchType = item;
                enterDouble->setRange(0,2000);
                enterDouble->setSuffix(" Miniutes");
                enterDouble->setDecimals(2);
                criteriaOperation->setText("<");
                criteriaOperation->show();
                enterDouble->setValue(5);
                enterDouble->show();
                minMaxUnit = time;
                minMaxMode = minInteger;
                break;
            case OT_SESSIONS_QTY :
                searchType = item;
                enterInteger->setRange(0,99);
                enterInteger->setSuffix("");
                enterInteger->setValue(1);
                criteriaOperation->setText(">");
                criteriaOperation->show();
                minMaxMode = maxInteger;
                enterInteger->show();
                break;
            case OT_DAILY_USAGE :
                searchType = item;
                enterDouble->setRange(0,999);
                enterDouble->setSuffix(" Hours");
                enterDouble->setDecimals(2);
                criteriaOperation->setText("<");
                criteriaOperation->show();
                enterDouble->setValue(p_profile->cpap->complianceHours());
                enterDouble->show();
                minMaxUnit = time;
                minMaxMode = minInteger;
                break;
            default:
                // Have an Event 
                enterInteger->setRange(0,999);
                enterInteger->setSuffix("");
                enterInteger->setValue(0);
                criteriaOperation->setText(">");
                criteriaOperation->show();
                minMaxMode = maxInteger;
                enterInteger->show();
                searchType = item;      //item is channel id which is >= 0x1000
                break;
        }
        criteriaChanged();
        if (searchType == OT_NONE) {
            statusA->show();
            statusA->setText(centerLine("Please select a Match"));
            //statusB->hide();
            summaryStatsA->clear();
            summaryStatsB->clear();
            summaryStatsC->clear();
            continueButton->setEnabled(false);
            startButton->setEnabled(false);
            return;
        }

}


bool DailySearchTab::find(QDate& date,Day* day)
{
        if (!day) return false;
        bool found=false;
        QString extra="";
        switch (searchType) {
            case OT_DISABLED_SESSIONS :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP,true);
                for (auto & sess : sessions) {
                    if (!sess->enabled()) {
                        found=true;
                        nextTab = TW_DETAILED ;
                    }
                }
                }
                break;
            case OT_NOTES :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Journal_Notes)) {
                    QString jcontents = convertRichText2Plain(journal->settings[Journal_Notes].toString());
                    extra = jcontents.trimmed().left(20);
                    found=true;
                    nextTab = TW_NOTES ;
                }
                }
                break;
            case OT_BOOK_MARKS :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Bookmark_Start)) {
                    found=true;
                    nextTab = TW_BOOKMARK ;
                }
                }
                break;
            case OT_NOTES_STRING :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Journal_Notes)) {
                    QString jcontents = convertRichText2Plain(journal->settings[Journal_Notes].toString());
                    QString findStr = enterString->text();
                    if (jcontents.contains(findStr,Qt::CaseInsensitive) ) {
                       found=true;
                       extra = jcontents.trimmed().left(20);
                       nextTab = TW_NOTES ;
                    }
                }
                }
                break;
            case OT_AHI :   
                {
                EventDataType ahi = calculateAhi(day);
                EventDataType limit = enterDouble->value();
                if (!minMaxValid || ahi > minMaxDouble ) {
                    minMaxDouble = ahi;
                    minMaxValid = true;
                }
                if (ahi > limit ) {
                       found=true;
                       nextTab = TW_EVENTS ;
                       nextTab = TW_DETAILED ;
                       extra = QString::number(ahi,'f', 2);
                }
                }
                break;
            case OT_SHORT_SESSIONS :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                for (auto & sess : sessions) {
                    qint64 ms = sess->length();
                    double minutes= ((double)ms)/60000.0;
                    if (!minMaxValid || ms < minMaxInteger ) {
                        minMaxInteger = ms;
                        minMaxValid = true;
                    }
                    if (minutes < enterDouble->value()) {
                        found=true;
                        nextTab = TW_DETAILED ;
                        extra = formatTime(ms);
                    }
                }
                }
                break;
            case OT_SESSIONS_QTY :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                quint32 size = sessions.size();
                if (!minMaxValid || size > minMaxInteger ) {
                    minMaxInteger = size;
                    minMaxValid = true;
                }
                if (size > (quint32)enterInteger->value()) {
                    found=true;
                    nextTab = TW_DETAILED ;
                    extra = QString::number(size);
                }
                }
                break;
            case OT_DAILY_USAGE :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                qint64 sum = 0  ;
                for (auto & sess : sessions) {
                    sum += sess->length();
                }
                double hours= ((double)sum)/3600000.0;
                if (!minMaxValid || sum < minMaxInteger ) {
                    minMaxInteger = sum;
                    minMaxValid = true;
                }
                if (hours < enterDouble->value()) {
                    found=true;
                    nextTab = TW_DETAILED ;
                    extra = formatTime(sum);
                }
                }
                break;
            default : 
                {
                quint32 count = day->count(searchType);
                if (count<=0) break;
                //DEBUGFW Q(count) Q(minMaxInteger) Q(minMaxValid) ;
                if (!minMaxValid || (quint32)count > minMaxInteger ) {
                    //DEBUGFW Q(count) Q(minMaxInteger) Q(minMaxValid) ;
                    minMaxInteger = count;
                    minMaxValid = true;
                }
                if (count > (quint32) enterInteger->value()) {
                    found=true;
                    extra = QString::number(count);
                    nextTab = TW_EVENTS ;
                }
                }
                break;
            case OT_NONE :
                return false;
                break;
        }
        if (found) {
            addItem(date , extra );
            return true;
        }
        return false;
};

void DailySearchTab::findall(QDate date, bool start)
{
        Q_UNUSED(start);
        guiDisplayTable->clear();
        passFound=0;
        int count = 0;
        int no_data = 0;
        Day*day;
        while (true) {
            count++;
            nextDate = date;
            if (passFound >= passDisplayLimit) {
                break;
            }
            if (date < firstDate) {
                break;
            }
            if (date > lastDate) {
                break;
            }
            daysSearched++;
            if (date.isValid()) {
                // use date
                // find and add
                //daysSearched++;
                day= p_profile->GetDay(date);
                if (day) {
                    if (find(date, day) ) {
                        passFound++;
                        daysFound++;
                    }
                } else {
                    no_data++;
                    daysSkipped++;
                    // Skip day. maybe no sleep or sdcard was no inserted.
                }
            } else {
                qWarning() << "DailySearchTab::findall invalid date." << date;
                break;
            }
            date=date.addDays(-1);
        }
        endOfPass();
        return ;

};

void DailySearchTab::addItem(QDate date, QString value) {
        int row = passFound;

        QTableWidgetItem *item = new QTableWidgetItem(*icon_off,date.toString());
        item->setData(dateRole,date);
        item->setData(valueRole,value);
        item->setIcon (*icon_off);
        item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);

        QTableWidgetItem *item2 = new QTableWidgetItem(value);
        item2->setTextAlignment(Qt::AlignCenter);
        //item2->setData(dateRole,date);
        //item2->setData(valueRole,value);
        item2->setFlags(Qt::NoItemFlags);
        if (guiDisplayTable->rowCount()<(row+1)) {
            guiDisplayTable->insertRow(row);
        }

        guiDisplayTable->setItem(row,0,item);
        guiDisplayTable->setItem(row,1,item2);
}

void    DailySearchTab::endOfPass() {
        QString display;
        if ((passFound >= passDisplayLimit) && (daysSearched<daysTotal)) {
            DEBUGFW ;
            //display=tr("More to Search    -----    Click on Continue Search");
            statusA->setText(centerLine(tr("More to Search")));
            //statusB->setText("");
            //statusC->setText(centerLine(tr("Continue or Change Match")));
            statusA->show();
            //statusB->hide();
            //statusC->show();



            continueButton->setEnabled(true);
            startButton->setEnabled(false);
        } else if (daysFound>0) {
            DEBUGFW ;
            statusA->setText(centerLine(tr("End of Search")));
            //statusB->setText("");
            //statusC->setText(centerLine(tr("Change Match")));
            statusA->show();
            //statusB->hide();
            //statusC->show();

            continueButton->setEnabled(false);
            startButton->setEnabled(false);
        } else {
            DEBUGFW ;
            statusA->setText(centerLine(tr("No Matching Criteria")));
            //statusB->setText("");
            //statusC->setText(centerLine(tr("Change Match")));
            statusA->show();
            //statusB->hide();
            //statusC->show();
            continueButton->setEnabled(false);
        }
        
        //status->setText(centerLine(   display  ));
        //status->show();
        displayStatistics();
}


void DailySearchTab::search(QDate date, bool start)
{
    findall(date,start);
};


void    DailySearchTab::on_itemActivated(QTableWidgetItem *item)
{
        if (item->column()!=0) {
            item=guiDisplayTable->item(item->row(),0);
        }
        QDate date = item->data(dateRole).toDate();
        item->setIcon (*icon_on);
        guiDisplayTable->setCurrentItem(item,QItemSelectionModel::Clear);
        daily->LoadDate( date );
        if (nextTab>=0 && nextTab < dailyTabWidget->count())  {
            dailyTabWidget->setCurrentIndex(nextTab);    // 0 = details ; 1=events =2 notes ; 3=bookarks;
        }
}

void    DailySearchTab::on_continueButton_clicked()
{
        search (nextDate , false);
}

void    DailySearchTab::on_startButton_clicked()
{
        firstDate = p_profile->FirstDay(MT_CPAP);
        lastDate = p_profile->LastDay(MT_CPAP);
        daysTotal= 1+firstDate.daysTo(lastDate);
        daysFound=0;
        daysSkipped=0;
        daysSearched=0;

        search (lastDate ,true);

}

void    DailySearchTab::on_intValueChanged(int ) {
        enterInteger->findChild<QLineEdit*>()->deselect();
        criteriaChanged();
}

void    DailySearchTab::on_doubleValueChanged(double ) {
        enterDouble->findChild<QLineEdit*>()->deselect();
        criteriaChanged();
}

void    DailySearchTab::on_textEdited(QString ) {
        criteriaChanged();
}

void    DailySearchTab::on_dailyTabWidgetCurrentChanged(int ) {
        delayedCreateUi();
}

void    DailySearchTab::displayStatistics() {
        QString extra;
        QString space("");
        if (minMaxValid) 
        switch (minMaxMode) {
            case minInteger:
                if (minMaxUnit == time) {
                    extra = QString("%1%2%3").arg(space).arg(tr("Min: ")).arg( formatTime(minMaxInteger));
                } else {
                    extra = QString("%1%2%3").arg(space).arg(tr("Min: ")).arg(minMaxInteger);
                }
                break;
            case maxInteger:
                extra = QString("%1%2%3").arg(space).arg(tr("Max: ")).arg(minMaxInteger);
                break;
            case minDouble:
                extra = QString("%1%2%3").arg(space).arg(tr("Min: ")).arg(minMaxDouble,0,'f',1);
                break;
            case maxDouble:
                extra = QString("%1%2%3").arg(space).arg(tr("Max: ")).arg(minMaxDouble,0,'f',1);
                break;
            default:
                extra="";
                break;
        }
        summaryStatsA->setText(centerLine(QString(tr("Searched %1/%2 days.")).arg(daysSearched).arg(daysTotal) ));
        summaryStatsB->setText(centerLine(QString(tr("Found %1.")).arg(daysFound) ));
        if (extra.size()>0) {
            summaryStatsC->setText(extra);
            summaryStatsC->show();
        } else {
            summaryStatsC->hide();
        }

}

void    DailySearchTab::criteriaChanged() {
        statusA->setText(centerLine(" ----- "));
        statusA->clear();
        statusB->clear();
        statusC->clear();
        summaryStatsA->clear();
        summaryStatsB->clear();
        summaryStatsC->clear();
        startButton->setEnabled( true);
        continueButton->setEnabled(false);
        guiDisplayTable->clear();
}

// inputs  character string.
// outputs cwa centered html string.
// converts \n to <br>
QString DailySearchTab::centerLine(QString line) {
        return QString( "<center>%1</center>").arg(line).replace("\n","<br>");
}

QString DailySearchTab::introductionStr() {
        return centerLine(
        "Finds days that match specified criteria\n"
        "Searches from last day to first day\n"
        "-----\n"
        "Find Days with:"
         );
        //"Searching starts on the lastDay to the firstDay\n"
}

QString DailySearchTab::formatTime (quint32 ms) {
        DEBUGFW Q(ms) ;
        ms += 500; // round to nearest second
        DEBUGFW Q(ms) ;
        quint32 hours = ms / 3600000;
        ms = ms % 3600000;
        DEBUGFW Q(ms) Q(hours) ;
        quint32 minutes = ms / 60000;
        DEBUGFW Q(ms) Q(hours) Q(minutes) ;
        ms = ms % 60000;
        quint32 seconds = ms /1000;
        DEBUGFW Q(ms) Q(hours) Q(minutes) Q(seconds);
        return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds); 
        #if 0
        double seconds = minutes*60;
        if (seconds<100) {
            //display seconds
        } else {
            // display tenths of minutes.
            return QString("%1 minutes").arg(seconds,0,'f',1); 
        }
        #endif
}

QString DailySearchTab::convertRichText2Plain (QString rich) {
        richText.setHtml(rich);
        return richText.toPlainText();
}

EventDataType  DailySearchTab::calculateAhi(Day* day) {
        if (!day) return 0.0;
        double  tmphours=day->hours(MT_CPAP);
        if (tmphours<=0) return 0;
        EventDataType  ahi=day->count(AllAhiChannels);
        if (p_profile->general->calculateRDI()) ahi+=day->count(CPAP_RERA);
        ahi/=tmphours;
        return ahi;
}


#if 0


    quint32 chantype = schema::FLAG | schema::SPAN | schema::MINOR_FLAG;
    if (p_profile->general->showUnknownFlags()) chantype |= schema::UNKNOWN;
    QList<ChannelID> chans = day->getSortedMachineChannels(chantype);

    // Go through all the enabled sessions of the day
    for (QList<Session *>::iterator s=day->begin();s!=day->end();++s) {
        Session * sess = *s;
        if (!sess->enabled()) continue;

        // For each session, go through all the channels
        QHash<ChannelID,QVector<EventList *> >::iterator m;
        for (int c=0; c < chans.size(); ++c) {
            ChannelID code = chans.at(c);
            m = sess->eventlist.find(code);
            if (m == sess->eventlist.end()) continue;

            drift=(sess->type() == MT_CPAP) ? clockdrift : 0;

            // Prepare title for this code, if there are any events
            QTreeWidgetItem *mcr;
            if (mcroot.find(code)==mcroot.end()) {
                int cnt=day->count(code);
                if (!cnt) continue; // If no events than don't bother showing..
                total_events+=cnt;
                QString st=schema::channel[code].fullname();
                if (st.isEmpty())  {
                    st=QString("Fixme %1").arg(code);
                }
                st+=" ";
                if (cnt==1) st+=tr("%1 event").arg(cnt);
                else st+=tr("%1 events").arg(cnt);

                QStringList l(st);
                l.append("");
                mcroot[code]=mcr=new QTreeWidgetItem(root,l);
                mccnt[code]=0;
            } else {
                mcr=mcroot[code];
            }

            // number of digits required for count depends on total for day
            int numDigits = ceil(log10(day->count(code)+1));

            // Now we go through the event list for the *session* (not for the day)
            for (int z=0;z<m.value().size();z++) {
                EventList & ev=*(m.value()[z]);

                for (quint32 o=0;o<ev.count();o++) {
                    qint64 t=ev.time(o)+drift;

                    if ((code == CPAP_CSR) || (code == CPAP_PB)) { // center it in the middle of span
                        t -= float(ev.raw(o) / 2.0) * 1000.0;
                    }
                    QStringList a;
                    QDateTime d=QDateTime::fromMSecsSinceEpoch(t); // Localtime
                    QString s=QString("#%1: %2").arg((int)(++mccnt[code]),(int)numDigits,(int)10,QChar('0')).arg(d.toString("HH:mm:ss"));
                    if (m.value()[z]->raw(o) > 0)
                            s += QString(" (%3)").arg(m.value()[z]->raw(o));

                    a.append(s);
                    QTreeWidgetItem *item=new QTreeWidgetItem(a);
                    item->setData(0,Qt::UserRole,t);
                    mcr->addChild(item);
                }
            }
        }
    }



#endif
