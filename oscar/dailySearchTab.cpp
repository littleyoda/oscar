/* user graph settings Implementation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#define TEST_MACROS_ENABLED
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
#define OT_BOOKMARKS         4
#define OT_BOOKMARKS_STRING  5
#define OT_AHI               6
#define OT_SESSION_LENGTH    7
#define OT_SESSIONS_QTY      8
#define OT_DAILY_USAGE       9
#define OT_BMI               10


//DO NOT CHANGH THESE VALUES - they impact compare operations.
//enums DO NOT WORK because due to switch statements
#define OP_NONE              0
#define OP_LT                1
#define OP_GT                2
#define OP_NE                3
#define OP_EQ                4
#define OP_LE                5
#define OP_GE                6
#define OP_ALL               7
#define OP_CONTAINS          0x100  // No bits set

DailySearchTab::DailySearchTab(Daily* daily , QWidget* searchTabWidget  , QTabWidget* dailyTabWidget)  : 
                        daily(daily) , parent(daily) , searchTabWidget(searchTabWidget) ,dailyTabWidget(dailyTabWidget)
{
    m_icon_selected      = new QIcon(":/icons/checkmark.png");
    m_icon_notSelected   = new QIcon(":/icons/empty_box.png");
    m_icon_configure     = new QIcon(":/icons/cog.png");

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
    daily->connect(selectString,        SIGNAL(textEdited(QString)), this, SLOT(on_textEdited(QString)) );
    daily->connect(selectInteger,       SIGNAL(valueChanged(int)), this, SLOT(on_intValueChanged(int)) );
    daily->connect(selectDouble,        SIGNAL(valueChanged(double)), this, SLOT(on_doubleValueChanged(double)) );
    daily->connect(selectCommandCombo,       SIGNAL(activated(int)), this, SLOT(on_selectCommandCombo_activated(int)   ));
    daily->connect(selectOperationCombo,       SIGNAL(activated(int)), this, SLOT(on_selectOperationCombo_activated(int)   ));
    daily->connect(selectCommandButton, SIGNAL(clicked()), this, SLOT(on_selectCommandButton_clicked()) );
    daily->connect(selectOperationButton,  SIGNAL(clicked()), this, SLOT(on_selectOperationButton_clicked()) );
    daily->connect(selectMatch,         SIGNAL(clicked()), this, SLOT(on_selectMatch_clicked()) );
    daily->connect(startButton,         SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
    daily->connect(helpInfo   ,         SIGNAL(clicked()), this, SLOT(on_helpInfo_clicked()) );
    daily->connect(guiDisplayTable,     SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->connect(guiDisplayTable,     SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->connect(guiDisplayTable,     SIGNAL(itemActivated(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->connect(dailyTabWidget,      SIGNAL(currentChanged(int)), this, SLOT(on_dailyTabWidgetCurrentChanged(int)   ));
}

DailySearchTab::~DailySearchTab() {
    daily->disconnect(selectString,       SIGNAL(textEdited(QString)), this, SLOT(on_textEdited(QString)) );
    daily->disconnect(dailyTabWidget,     SIGNAL(currentChanged(int)), this, SLOT(on_dailyTabWidgetCurrentChanged(int)   ));
    daily->disconnect(selectInteger,      SIGNAL(valueChanged(int)), this, SLOT(on_intValueChanged(int)) );
    daily->disconnect(selectDouble,       SIGNAL(valueChanged(double)), this, SLOT(on_doubleValueChanged(double)) );
    daily->disconnect(selectCommandCombo,    SIGNAL(activated(int)), this, SLOT(on_selectCommandCombo_activated(int)   ));
    daily->disconnect(selectOperationCombo,       SIGNAL(activated(int)), this, SLOT(on_selectOperationCombo_activated(int)   ));
    daily->disconnect(selectCommandButton,   SIGNAL(clicked()), this, SLOT(on_selectCommandButton_clicked()) );
    daily->disconnect(selectOperationButton,  SIGNAL(clicked()), this, SLOT(on_selectOperationButton_clicked()) );
    daily->disconnect(selectMatch,       SIGNAL(clicked()), this, SLOT(on_selectMatch_clicked()) );
    daily->disconnect(helpInfo   ,       SIGNAL(clicked()), this, SLOT(on_helpInfo_clicked()) );
    daily->disconnect(guiDisplayTable,   SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->disconnect(guiDisplayTable,   SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->disconnect(guiDisplayTable,   SIGNAL(itemActivated(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->disconnect(startButton,       SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
    delete m_icon_selected;
    delete m_icon_notSelected;
    delete m_icon_configure ;
};

void    DailySearchTab::createUi() {

        QFont baseFont =*defaultfont;
        baseFont.setPointSize(1+baseFont.pointSize());
        searchTabWidget ->setFont(baseFont);

        searchTabLayout  = new QVBoxLayout(searchTabWidget);
        criteriaLayout   = new QHBoxLayout();
        innerCriteriaFrame = new QFrame(this);
        innerCriteriaLayout = new QHBoxLayout(innerCriteriaFrame);

        searchLayout     = new QHBoxLayout();
        summaryLayout    = new QHBoxLayout();
        searchTabLayout  ->setContentsMargins(4, 4, 4, 4);

        helpInfo        = new QPushButton(this);
        selectMatch     = new QPushButton(this);
        selectUnits     = new QLabel(this);
        selectCommandCombo   = new QComboBox(this);
        selectOperationCombo   = new QComboBox(this);
        selectCommandButton  = new QPushButton(this);
        selectOperationButton = new QPushButton(this);
        startButton     = new QPushButton(this);
        selectDouble     = new QDoubleSpinBox(this);
        selectInteger    = new QSpinBox(this);
        selectString     = new QLineEdit(this);
        statusProgress  = new QLabel(this);
        summaryProgress = new QLabel(this);
        summaryFound    = new QLabel(this);
        summaryMinMax   = new QLabel(this);
        guiDisplayTable = new QTableWidget(this);

        searchTabLayout ->addWidget(helpInfo);

        innerCriteriaLayout  ->addWidget(selectCommandCombo);
        innerCriteriaLayout  ->addWidget(selectCommandButton);
        innerCriteriaLayout  ->addWidget(selectOperationCombo);
        innerCriteriaLayout  ->addWidget(selectOperationButton);
        innerCriteriaLayout  ->addWidget(selectInteger);
        innerCriteriaLayout  ->addWidget(selectString);
        innerCriteriaLayout  ->addWidget(selectDouble);
        innerCriteriaLayout  ->addWidget(selectUnits);
        innerCriteriaLayout  ->insertStretch(-1,5);   // will center match command

        criteriaLayout  ->addWidget(selectMatch);
        criteriaLayout  ->addWidget(innerCriteriaFrame);
        criteriaLayout  ->insertStretch(-1,5);

        searchTabLayout ->addLayout(criteriaLayout);

        searchLayout    ->addWidget(startButton);
        searchLayout    ->addWidget(statusProgress);
        searchTabLayout ->addLayout(searchLayout);

        summaryLayout   ->addWidget(summaryProgress);
        summaryLayout   ->insertStretch(1,5);
        summaryLayout   ->addWidget(summaryFound);
        summaryLayout   ->insertStretch(3,5);
        summaryLayout   ->addWidget(summaryMinMax);
        searchTabLayout ->addLayout(summaryLayout);

        searchTabLayout ->addWidget(guiDisplayTable);
        // End of UI creatation

        // Initialize ui contents 

        QString styleButton=QString("QPushButton { color: black; border: 1px solid black; padding: 5px ; } QPushButton:disabled { color: #606060; border: 1px solid #606060; }" );

        searchTabWidget ->setFont(baseFont);

        helpInfo    ->setText(helpStr());
        helpInfo    ->setFont(baseFont);
        helpInfo    ->setStyleSheet(" padding: 4;border: 1px solid black;");

        selectMatch->setText(tr("Match:"));
        selectMatch->setIcon(*m_icon_configure);
        selectMatch->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectMatch->setStyleSheet( styleButton );


        selectOperationButton->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectOperationButton->setText("");
        selectOperationButton->setStyleSheet("border:none;");
        selectOperationButton->hide();

        selectCommandButton->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectCommandButton->setText(tr("Select Match"));
        selectCommandButton->setStyleSheet("border:none;");

        selectCommandCombo->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectCommandCombo->setFont(baseFont);
        setCommandPopupEnabled(false);
        selectOperationCombo->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectOperationCombo->setFont(baseFont);
        setOperationPopupEnabled(false);

        selectDouble->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectDouble->hide();
        selectInteger->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectInteger->hide();
        selectString->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectString ->hide();

        selectUnits->setText("");
        selectUnits->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

        startButton       ->setStyleSheet( styleButton );
        startButton       ->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        helpInfo          ->setStyleSheet( styleButton );


        statusProgress  ->show();
        summaryProgress ->setFont(baseFont);
        summaryFound    ->setFont(baseFont);
        summaryMinMax   ->setFont(baseFont);
        summaryMinMax   ->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

        summaryProgress ->setStyleSheet("padding:4px;background-color: #ffffff;" );
        summaryFound    ->setStyleSheet("padding:4px;background-color: #f0f0f0;" );
        summaryMinMax   ->setStyleSheet("padding:4px;background-color: #ffffff;" );

        summaryProgress ->show();
        summaryFound    ->show();
        summaryMinMax   ->show();
        searchType = OT_NONE;

        startButton->setText(tr("Start Search"));
        startButton->setEnabled(false);

        guiDisplayTable->setFont(baseFont);
        if (guiDisplayTable->columnCount() <2) guiDisplayTable->setColumnCount(2);
        horizontalHeader0 = new QTableWidgetItem();
        guiDisplayTable->setHorizontalHeaderItem ( 0, horizontalHeader0);

        horizontalHeader1 = new QTableWidgetItem();
        guiDisplayTable->setHorizontalHeaderItem ( 1, horizontalHeader1);

        guiDisplayTable->setObjectName(QString::fromUtf8("guiDisplayTable"));
        guiDisplayTable->setAlternatingRowColors(true);
        guiDisplayTable->setSelectionMode(QAbstractItemView::SingleSelection);
        guiDisplayTable->setAlternatingRowColors(true);
        guiDisplayTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        guiDisplayTable->setSortingEnabled(false);
        guiDisplayTable->horizontalHeader()->setStretchLastSection(true);
        // should make the following based on a real date ei based on locale.
        guiDisplayTable->setColumnWidth(0, 30/*iconWidthPlus*/ + QFontMetrics(baseFont).size(Qt::TextSingleLine , "WWW MMM 99 2222").width());


        horizontalHeader0->setText("DATE\nClick date to Restore");
        horizontalHeader1->setText("INFORMATION\nRestores & Bookmark tab");

        guiDisplayTable->horizontalHeader()->hide();
        //guiDisplayTable->setStyleSheet("QTableWidget::item { padding: 1px }");
}

void DailySearchTab::delayedCreateUi() {
        // meed delay to insure days are populated.
        if (createUiFinished) return;
        createUiFinished = true;

        selectCommandCombo->clear();
        selectCommandCombo->addItem(tr("Notes"),OT_NOTES);
        selectCommandCombo->addItem(tr("Notes containing"),OT_NOTES_STRING);
        selectCommandCombo->addItem(tr("BookMarks"),OT_BOOKMARKS);
        selectCommandCombo->addItem(tr("BookMarks containing"),OT_BOOKMARKS_STRING);
        selectCommandCombo->addItem(tr("AHI "),OT_AHI);
        selectCommandCombo->addItem(tr("Daily Duration"),OT_DAILY_USAGE);
        selectCommandCombo->addItem(tr("Session Duration" ),OT_SESSION_LENGTH);
        selectCommandCombo->addItem(tr("Disabled Sessions"),OT_DISABLED_SESSIONS);
        selectCommandCombo->addItem(tr("Number of Sessions"),OT_SESSIONS_QTY);
        selectCommandCombo->insertSeparator(selectCommandCombo->count());  // separate from events

        opCodeMap.insert( opCodeStr(OP_LT),OP_LT);
        opCodeMap.insert( opCodeStr(OP_GT),OP_GT);
        opCodeMap.insert( opCodeStr(OP_NE),OP_NE);
        opCodeMap.insert( opCodeStr(OP_LE),OP_LE);
        opCodeMap.insert( opCodeStr(OP_GE),OP_GE);
        opCodeMap.insert( opCodeStr(OP_EQ),OP_EQ);
        opCodeMap.insert( opCodeStr(OP_NE),OP_NE);
        opCodeMap.insert( opCodeStr(OP_CONTAINS),OP_CONTAINS);
        selectOperationCombo->clear();

        // The order here is the order in the popup box 
        selectOperationCombo->addItem(opCodeStr(OP_LT));
        selectOperationCombo->addItem(opCodeStr(OP_GT));
        selectOperationCombo->addItem(opCodeStr(OP_LE));
        selectOperationCombo->addItem(opCodeStr(OP_GE));
        selectOperationCombo->addItem(opCodeStr(OP_EQ));
        selectOperationCombo->addItem(opCodeStr(OP_NE));

        // Now add events
        QDate date = p_profile->LastDay(MT_CPAP);
        if ( !date.isValid()) return;
        Day* day = p_profile->GetDay(date);
        if (!day) return;

        // the following is copied from daily.
        qint32 chans = schema::SPAN | schema::FLAG | schema::MINOR_FLAG;
        if (p_profile->general->showUnknownFlags()) chans |= schema::UNKNOWN;
        QList<ChannelID> available;
        available.append(day->getSortedMachineChannels(chans));
        for (int i=0; i < available.size(); ++i) {
            ChannelID id = available.at(i);
            schema::Channel chan = schema::channel[ id  ];
            // new stuff now
            QString displayName= chan.fullname();
            selectCommandCombo->addItem(displayName,id);
        }
}

void    DailySearchTab::on_helpInfo_clicked() {
        helpMode = !helpMode;
        helpInfo->setText(helpStr());
}

bool    DailySearchTab::compare(int aa , int bb) {
        int request = selectOperationOpCode;
        int mode=0;
        if (aa <bb ) mode |= OP_LT;
        if (aa >bb ) mode |= OP_GT;
        if (aa ==bb ) mode |= OP_EQ;
        return ( (mode & request)!=0);
};

QString DailySearchTab::valueToString(int value, QString defaultValue) {
        switch (valueMode) {
            case hundredths :
                return QString("%1").arg( (double(value)/100.0),0,'f',2);
            break;
            case hoursToMs:
            case minutesToMs:
                return formatTime(value);
            break;
            case whole:
                return QString().setNum(value);
            break;
            case string:
                return foundString;
            break;
            default:
            break;
        }
        return defaultValue;
}

void    DailySearchTab::on_selectOperationCombo_activated(int index) {
        QString text = selectOperationCombo->itemText(index);
        int opCode = opCodeMap[text];
        if (opCode>OP_NONE && opCode < OP_ALL) {
            selectOperationOpCode = opCode;
            selectOperationButton->setText(opCodeStr(selectOperationOpCode));
        }
        setOperationPopupEnabled(false);
        criteriaChanged();
};

void    DailySearchTab::on_selectCommandCombo_activated(int index) {
        // here to select new search criteria
        // must reset all variables and label, button, etc

        selectDouble->hide();
        selectDouble->setDecimals(3);
        selectInteger->hide();
        selectString->hide();
        selectUnits->hide();
        selectOperationButton->hide();

        valueMode = notUsed;
        selectValue = 0;

        // workaround for combo box alignmnet and sizing.
        // copy selections to a pushbutton. hide combobox and show pushButton. Pushbutton activation can show popup.
        // always hide first before show. allows for best fit
        selectCommandButton->setText(selectCommandCombo->itemText(index));
        setCommandPopupEnabled(false);

        // get item selected
        int item = selectCommandCombo->itemData(index).toInt();
        searchType = item;
        bool hasParameters=true;
        switch (item) {
            case OT_NONE :
                horizontalHeader1->setText("INFORMATION");
                nextTab = TW_NONE ;
                break;
            case OT_DISABLED_SESSIONS :
                horizontalHeader1->setText("Jumps to Details tab");
                nextTab = TW_DETAILED ;
                hasParameters=false;
                break;
            case OT_NOTES :
                horizontalHeader1->setText("Note\nJumps to Notes tab");
                nextTab = TW_NOTES ;
                hasParameters=false;
                valueMode = string;
                break;
            case OT_BOOKMARKS :
                horizontalHeader1->setText("Jumps to Bookmark tab");
                nextTab = TW_BOOKMARK ;
                hasParameters=false;
                break;
            case OT_BOOKMARKS_STRING :
                horizontalHeader1->setText("Jumps to Bookmark tab");
                nextTab = TW_BOOKMARK ;
                selectString->clear();
                selectString->show();
                selectOperationOpCode = OP_CONTAINS;
                valueMode = string;
                break;
            case OT_NOTES_STRING :
                horizontalHeader1->setText("Note\nJumps to Notes tab");
                nextTab = TW_NOTES ;
                selectString->clear();
                selectString->show();
                selectOperationOpCode = OP_CONTAINS;
                valueMode = string;
                break;
            case OT_AHI :
                horizontalHeader1->setText("AHI\nJumps to Details tab");
                nextTab = TW_DETAILED ;
                selectDouble->setRange(0,999);
                selectDouble->setValue(5.0);
                selectDouble->setDecimals(2);
                selectDouble->show();
                selectOperationOpCode = OP_GT;

                // QString.number(calculateAhi()*100.0).toInt();
                valueMode = hundredths;
                break;
            case OT_SESSION_LENGTH :
                horizontalHeader1->setText("Session Duration\nJumps to Details tab");
                nextTab = TW_DETAILED ;
                selectDouble->setRange(0,9999);
                selectDouble->setDecimals(2);
                selectDouble->setValue(5);
                selectDouble->show();

                selectUnits->setText(" Miniutes");
                selectUnits->show();

                selectOperationButton->setText("<");
                selectOperationOpCode = OP_LT;
                selectOperationButton->show();

                valueMode = minutesToMs;
                break;
            case OT_SESSIONS_QTY :
                horizontalHeader1->setText("Number of Sessions\nJumps to Details tab");
                nextTab = TW_DETAILED ;
                selectInteger->setRange(0,999);
                selectInteger->setValue(1);
                selectOperationButton->show();
                selectOperationOpCode = OP_GT;

                valueMode = whole;
                selectInteger->show();
                break;
            case OT_DAILY_USAGE :
                horizontalHeader1->setText("Daily Duration\nJumps to Details tab");
                nextTab = TW_DETAILED ;
                selectDouble->setRange(0,999);
                selectUnits->setText(" Hours");
                selectUnits->show();
                selectDouble->setDecimals(2);
                selectOperationButton->show();
                selectOperationOpCode = OP_LT;
                selectDouble->setValue(p_profile->cpap->complianceHours());
                selectDouble->show();

                valueMode = hoursToMs;
                selectInteger->setValue((int)selectDouble->value()*3600000.0);   //convert to ms
                break;
            default:
                // Have an Event 
                horizontalHeader1->setText("Number of events\nJumps to Events tab");
                nextTab = TW_EVENTS ;
                selectInteger->setRange(0,999);
                selectInteger->setValue(0);
                selectOperationOpCode = OP_GT;
                selectOperationButton->show();
                valueMode = whole;
                selectInteger->show();
                break;
        }

        if (searchType == OT_NONE) {
            statusProgress->show();
            statusProgress->setText(centerLine("Please select a Match"));
            summaryProgress->clear();
            summaryFound->clear();
            summaryMinMax->clear();
            startButton->setEnabled(false);
            return;
        }
        criteriaChanged();
        if (!hasParameters) {
            // auto start searching
            startButton->setText(tr("Automatic start"));
            startButtonMode=true;
            on_startButton_clicked();
            return;
        }
}

void DailySearchTab::updateValues(qint32 value) {
        foundValue = value;
        if (!minMaxValid ) {
            minMaxValid = true;
            minInteger = value;
            maxInteger = value;
        } else if (  value < minInteger ) {
            minInteger = value;
        } else if (  value > maxInteger ) {
            maxInteger = value;
        }
}


bool DailySearchTab::find(QDate& date,Day* day)
{
        if (!day) return false;
        bool found=false;
        QString extra="---";
        switch (searchType) {
            case OT_DISABLED_SESSIONS :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP,true);
                for (auto & sess : sessions) {
                    if (!sess->enabled()) {
                        found=true;
                    }
                }
                }
                break;
            case OT_NOTES :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Journal_Notes)) {
                    QString jcontents = convertRichText2Plain(journal->settings[Journal_Notes].toString());
                    foundString = jcontents.trimmed().left(40);
                    found=true;
                }
                }
                break;
            case OT_BOOKMARKS :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Bookmark_Start)) {
                    found=true;
                }
                }
                break;
            case OT_BOOKMARKS_STRING :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Bookmark_Notes)) {
                    QStringList notes = journal->settings[Bookmark_Notes].toStringList();
                    QString findStr = selectString->text();
                    for (   const auto & note : notes) {
                        if (note.contains(findStr,Qt::CaseInsensitive) ) {
                           found=true;
                           foundString = note.trimmed().left(40);
                           break;
                        }
                    }
                }
                }
                break;
            case OT_NOTES_STRING :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Journal_Notes)) {
                    QString jcontents = convertRichText2Plain(journal->settings[Journal_Notes].toString());
                    QString findStr = selectString->text();
                    if (jcontents.contains(findStr,Qt::CaseInsensitive) ) {
                       found=true;
                       foundString = jcontents.trimmed().left(40);
                    }
                }
                }
                break;
            case OT_AHI :   
                {
                EventDataType dahi =calculateAhi(day);
                dahi += 0.005;  
                dahi *= 100.0;  
                int ahi = (int)dahi;
                updateValues(ahi);
                found = compare (ahi , selectValue);
                }
                break;
            case OT_SESSION_LENGTH :
                {
                bool valid=false;
                qint64 value=0;
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                for (auto & sess : sessions) {
                    //qint64 msF = sess->realFirst();
                    //qint64 msL = sess->realLast();
                    //DEBUGFW O(day->date()) QQ("sessionLength",ms) Q(selectValue); //  Q(msF) Q(msL) ;
                    //found a session with negative length. Session.cpp has real end time before realstart time.
                    qint64 ms = sess->length();
                    updateValues(ms);
                    if (compare (ms , selectValue) ) {
                        found =true;
                    }
                    if (!valid) {
                        valid=true;
                        value=ms;
                    } else if (compare (ms , value) ) {
                        value=ms;
                    }
                }
                // use best / lowest  daily value that meets criteria
                if (valid) updateValues(value);
                }
                break;
            case OT_SESSIONS_QTY :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                qint32 size = sessions.size();
                updateValues(size);
                found=compare (size , selectValue);
                }
                break;
            case OT_DAILY_USAGE :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                qint64 sum = 0  ;
                for (auto & sess : sessions) {
                    sum += sess->length();
                }
                updateValues(sum);
                found=compare (sum , selectValue);
                }
                break;
            default : 
                {
                qint32 count = day->count(searchType);
                if (count<=0) break;
                updateValues(count);
                found=compare (count , selectValue);
                }
                break;
            case OT_NONE :
                return false;
                break;
        }
        if (found) {
            addItem(date , valueToString(foundValue,"------") );
            return true;
        }
        return false;
};

void DailySearchTab::search(QDate date)
{
        guiDisplayTable->clearContents();
        for (int index=0; index<guiDisplayTable->rowCount();index++) {
            guiDisplayTable->setRowHidden(index,true);
        }
        foundString.clear();
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
                qWarning() << "DailySearchTab::search invalid date." << date;
                break;
            }
            date=date.addDays(-1);
        }
        endOfPass();
        return ;

};

void DailySearchTab::addItem(QDate date, QString value) {
        int row = passFound;

        QTableWidgetItem *item = new QTableWidgetItem(*m_icon_notSelected,date.toString());
        item->setData(dateRole,date);
        item->setData(valueRole,value);
        item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);

        QTableWidgetItem *item2 = new QTableWidgetItem(*m_icon_notSelected,value);
        item2->setTextAlignment(Qt::AlignCenter);
        item2->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
        if (guiDisplayTable->rowCount()<(row+1)) {
            guiDisplayTable->insertRow(row);
        }

        guiDisplayTable->setItem(row,0,item);
        guiDisplayTable->setItem(row,1,item2);
        guiDisplayTable->setRowHidden(row,false);
}

void    DailySearchTab::endOfPass() {
        startButtonMode=false;      // display Continue;
        QString display;
        if ((passFound >= passDisplayLimit) && (daysSearched<daysTotal)) {
            statusProgress->setText(centerLine(tr("More to Search")));
            statusProgress->show();
            startButton->setEnabled(true);
            startButton->setText("Continue Search");
            guiDisplayTable->horizontalHeader()->show();
        } else if (daysFound>0) {
            statusProgress->setText(centerLine(tr("End of Search")));
            statusProgress->show();
            startButton->setEnabled(false);
            guiDisplayTable->horizontalHeader()->show();
        } else {
            statusProgress->setText(centerLine(tr("No Matching Criteria")));
            statusProgress->show();
            startButton->setEnabled(false);
            guiDisplayTable->horizontalHeader()->hide();
        }

        displayStatistics();
}


void    DailySearchTab::on_dateItemClicked(QTableWidgetItem *item)
{
        // a date is clicked
        // load new date
        // change tab 
        int row = item->row();
        int col = item->column();
        guiDisplayTable->setCurrentItem(item,QItemSelectionModel::Clear);
        item->setIcon (*m_icon_selected);
        item=guiDisplayTable->item(row,col);
        if (col!=0) {
            item = guiDisplayTable->item(item->row(),0);
        }
        QDate date = item->data(dateRole).toDate();
        daily->LoadDate( date );
        if ((col!=0) &&  nextTab>=0 && nextTab < dailyTabWidget->count())  {
            dailyTabWidget->setCurrentIndex(nextTab);    // 0 = details ; 1=events =2 notes ; 3=bookarks;
        }
}

void    DailySearchTab::setOperationPopupEnabled(bool on) {
        if (selectOperationOpCode<OP_NONE || selectOperationOpCode >= OP_ALL) return;
        if (on) {
            selectOperationButton->show();
            selectOperationCombo->setEnabled(true);
            selectOperationCombo->showPopup();
        } else {
            selectOperationCombo->hidePopup();
            selectOperationCombo->setEnabled(false);
            selectOperationCombo->hide();
            selectOperationButton->show();
        }

}

void    DailySearchTab::setCommandPopupEnabled(bool on) {
        if (on) {
            selectCommandButton->show();
            selectCommandCombo->setEnabled(true);
            selectCommandCombo->showPopup();
        } else {
            selectCommandCombo->hidePopup();
            selectCommandCombo->setEnabled(false);
            selectCommandCombo->hide();
            selectCommandButton->show();
        }
}

void    DailySearchTab::on_selectOperationButton_clicked() {
        setOperationPopupEnabled(true);
};


void    DailySearchTab::on_selectMatch_clicked() {
        setCommandPopupEnabled(true);
}

void    DailySearchTab::on_selectCommandButton_clicked()
{
        setCommandPopupEnabled(true);
}

void    DailySearchTab::on_startButton_clicked()
{
        if (startButtonMode) {
            // have start mode

            // must set up search from the latest date and go to the first date.
            // set up variables for multiple passes.
            //startButton->setText("Continue Search");
            search (lastDate );
            startButtonMode=false;
        } else {
            // have continue search mode;
            search (nextDate );
        }
}

void    DailySearchTab::on_intValueChanged(int ) {
        //Turn off highlighting by deslecting edit capabilities
        selectInteger->findChild<QLineEdit*>()->deselect();
        criteriaChanged();
}

void    DailySearchTab::on_doubleValueChanged(double ) {
        //Turn off highlighting by deslecting edit capabilities
        selectDouble->findChild<QLineEdit*>()->deselect();
        criteriaChanged();
}

void    DailySearchTab::on_textEdited(QString ) {
        criteriaChanged();
}

void    DailySearchTab::on_dailyTabWidgetCurrentChanged(int ) {
        // Any time a tab is changed - then the day information should be valid.
        // so finish updating the ui display.
        delayedCreateUi();
}

void    DailySearchTab::displayStatistics() {
        QString extra;
        // display days searched
        QString skip= daysSkipped==0?"":QString(" (Skip:%1)").arg(daysSkipped);
        summaryProgress->setText(centerLine(QString(tr("Searched %1/%2%3 days.")).arg(daysSearched).arg(daysTotal).arg(skip) ));

        // display days found
        summaryFound->setText(centerLine(QString(tr("Found %1.")).arg(daysFound) ));

        // display associated value 
        extra ="";
        if (minMaxValid) {
            extra = QString("%1/%2").arg(valueToString(minInteger)).arg(valueToString(maxInteger));
        }
        if (extra.size()>0) {
            summaryMinMax->setText(extra);
            summaryMinMax->show();
        } else {
            summaryMinMax->hide();
        }
}

void    DailySearchTab::criteriaChanged() {
        // setup before start button

        if (valueMode != notUsed ) {
            selectOperationButton->setText(opCodeStr(selectOperationOpCode));
            selectOperationButton->show();
        }
        switch (valueMode) {
            case hundredths :
                selectValue = (int)(selectDouble->value()*100.0);   //convert to hundreths of AHI.
            break;
            case minutesToMs:
                selectValue = (int)(selectDouble->value()*60000.0);   //convert to ms
            break;
            case hoursToMs:
                selectValue = (int)(selectDouble->value()*3600000.0);   //convert to ms
            break;
            case whole:
                selectValue = selectInteger->value();;
            break;
            default:
            break;
        }

        selectCommandCombo->hide();
        selectCommandButton->show();

        startButton->setText(tr("Start Search"));
        startButtonMode=true;
        startButton->setEnabled( true);

        statusProgress->setText(centerLine(" ----- "));
        statusProgress->clear();

        summaryProgress->clear();
        summaryFound->clear();
        summaryMinMax->clear();
        for (int index=0; index<guiDisplayTable->rowCount();index++) {
            guiDisplayTable->setRowHidden(index,true);
        }
        guiDisplayTable->horizontalHeader()->hide();

        minMaxValid = false;
        minInteger = 0;
        maxInteger = 0;
        minDouble = 0.0;
        maxDouble = 0.0;
        firstDate = p_profile->FirstDay(MT_CPAP);
        lastDate = p_profile->LastDay(MT_CPAP);
        daysTotal= 1+firstDate.daysTo(lastDate);
        daysFound=0;
        daysSkipped=0;
        daysSearched=0;
        startButtonMode=true;
}

// inputs  character string.
// outputs cwa centered html string.
// converts \n to <br>
QString DailySearchTab::centerLine(QString line) {
        return QString( "<center>%1</center>").arg(line).replace("\n","<br>");
}

QString DailySearchTab::helpStr() {
    if (helpMode) {
        return tr(
            "Click HERE to close help\n"
            "\n"
            "Finds days that match specified criteria\n"
            "Searches from last day to first day\n"
            "\n"
            "Click on the Match Button to configure the search criteria\n"
            "Different operations are supported. click on the compare operator.\n"
            "\n"
            "Search Results\n"
            "Minimum/Maximum values are display on the summary row\n"
            "Click date column will restores date\n"
            "Click right column will restores date and jump to a tab"
        );
    }
    return tr("Help Information");
}

QString DailySearchTab::formatTime (qint32 ms) {
        qint32 hours = ms / 3600000;
        ms = ms % 3600000;
        qint32 minutes = ms / 60000;
        ms = ms % 60000;
        qint32 seconds = ms /1000;
        return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds); 
}

QString DailySearchTab::convertRichText2Plain (QString rich) {
        richText.setHtml(rich);
        return richText.toPlainText();
}

QString DailySearchTab::opCodeStr(int opCode) {
//selectOperationButton->setText(QChar(0x2208));   // use either 0x220B or 0x2208
        switch (opCode) {
            case OP_GT : return "> ";
            case OP_GE : return ">=";
            case OP_LT : return "< ";
            case OP_LE : return "<=";
            case OP_EQ : return "==";
            case OP_NE : return "!=";
            case OP_CONTAINS : return QChar(0x2208);
        }
        return "";
};

EventDataType  DailySearchTab::calculateAhi(Day* day) {
        if (!day) return 0.0;
        // copied from daily.cpp
        double  hours=day->hours(MT_CPAP);
        if (hours<=0) return 0;
        EventDataType  ahi=day->count(AllAhiChannels);
        if (p_profile->general->calculateRDI()) ahi+=day->count(CPAP_RERA);
        ahi/=hours;
        return ahi;
}

