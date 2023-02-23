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
#include <QProgressBar>
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


//enums DO NOT WORK because due to switch statements because channelID for events are also used
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
    daily->connect(selectCommandCombo,  SIGNAL(activated(int)), this, SLOT(on_selectCommandCombo_activated(int)   ));
    daily->connect(selectCommandButton, SIGNAL(clicked()), this, SLOT(on_selectCommandButton_clicked()) );
    daily->connect(selectOperationCombo,SIGNAL(activated(int)), this, SLOT(on_selectOperationCombo_activated(int)   ));
    daily->connect(selectOperationButton,SIGNAL(clicked()), this, SLOT(on_selectOperationButton_clicked()) );
    daily->connect(selectMatch,         SIGNAL(clicked()), this, SLOT(on_selectMatch_clicked()) );
    daily->connect(startButton,         SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
    daily->connect(clearButton,         SIGNAL(clicked()), this, SLOT(on_clearButton_clicked()) );
    daily->connect(helpButton   ,       SIGNAL(clicked()), this, SLOT(on_helpButton_clicked()) );
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
    daily->disconnect(selectCommandCombo, SIGNAL(activated(int)), this, SLOT(on_selectCommandCombo_activated(int)   ));
    daily->disconnect(selectCommandButton,SIGNAL(clicked()), this, SLOT(on_selectCommandButton_clicked()) );
    daily->disconnect(selectOperationCombo,SIGNAL(activated(int)), this, SLOT(on_selectOperationCombo_activated(int)   ));
    daily->disconnect(selectOperationButton,SIGNAL(clicked()), this, SLOT(on_selectOperationButton_clicked()) );
    daily->disconnect(selectMatch,       SIGNAL(clicked()), this, SLOT(on_selectMatch_clicked()) );
    daily->disconnect(helpButton   ,     SIGNAL(clicked()), this, SLOT(on_helpButton_clicked()) );
    daily->disconnect(guiDisplayTable,   SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->disconnect(guiDisplayTable,   SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->disconnect(guiDisplayTable,   SIGNAL(itemActivated(QTableWidgetItem*)), this, SLOT(on_dateItemClicked(QTableWidgetItem*)   ));
    daily->disconnect(startButton,       SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
    daily->connect(clearButton,          SIGNAL(clicked()), this, SLOT(on_clearButton_clicked()) );
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

        helpButton        = new QPushButton(this);
        helpInfo        = new QLabel(this);
        selectMatch     = new QPushButton(this);
        selectUnits     = new QLabel(this);
        selectCommandCombo   = new QComboBox(this);
        selectCommandButton  = new QPushButton(this);
        selectOperationCombo   = new QComboBox(this);
        selectOperationButton = new QPushButton(this);
        startButton     = new QPushButton(this);
        clearButton     = new QPushButton(this);
        selectDouble     = new QDoubleSpinBox(this);
        selectInteger    = new QSpinBox(this);
        selectString     = new QLineEdit(this);
        statusProgress  = new QLabel(this);
        summaryProgress = new QLabel(this);
        summaryFound    = new QLabel(this);
        summaryMinMax   = new QLabel(this);
        guiProgressBar  = new QProgressBar(this);
        guiDisplayTable = new QTableWidget(this);

        searchTabLayout ->addWidget(helpButton);
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

        searchLayout    ->addWidget(clearButton);
        searchLayout    ->addWidget(startButton);
        searchLayout    ->insertStretch(2,5);
        searchLayout    ->addWidget(statusProgress);
        searchLayout    ->insertStretch(-1,5);
        searchTabLayout ->addLayout(searchLayout);

        summaryLayout   ->addWidget(summaryProgress);
        summaryLayout   ->insertStretch(1,5);
        summaryLayout   ->addWidget(summaryFound);
        summaryLayout   ->insertStretch(3,5);
        summaryLayout   ->addWidget(summaryMinMax);
        searchTabLayout ->addLayout(summaryLayout);

        searchTabLayout ->addWidget(guiProgressBar);
        searchTabLayout ->addWidget(guiDisplayTable);
        // End of UI creatation

        // Initialize ui contents 

        QString styleButton=QString("QPushButton { color: black; border: 1px solid black; padding: 5px ; } QPushButton:disabled { color: #606060; border: 1px solid #606060; }" );

        searchTabWidget ->setFont(baseFont);

        helpButton    ->setFont(baseFont);
        //helpButton    ->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        //helpButton    ->setStyleSheet(QString("QPushButton:flat {border: none }"));
        //helpButton    ->setStyleSheet(" text-align:left ; padding: 4;border: 1px");

        helpInfo      ->setText(helpStr());
        helpInfo      ->setFont(baseFont);
        helpInfo      ->setStyleSheet(" text-align:left ; padding: 4;border: 1px");
        helpMode      = true;
        on_helpButton_clicked();

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
        //selectDouble->hide();
        selectInteger->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        //selectInteger->hide();
        selectString->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        //selectString ->hide();

        selectUnits->setText("");
        selectUnits->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

        clearButton       ->setStyleSheet( styleButton );
        clearButton       ->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        startButton       ->setStyleSheet( styleButton );
        startButton       ->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        helpButton        ->setStyleSheet( styleButton );


        //statusProgress  ->show();
        summaryProgress ->setFont(baseFont);
        summaryFound    ->setFont(baseFont);
        summaryMinMax   ->setFont(baseFont);
        summaryMinMax   ->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

        summaryProgress ->setStyleSheet("padding:4px;background-color: #ffffff;" );
        summaryFound    ->setStyleSheet("padding:4px;background-color: #f0f0f0;" );
        summaryMinMax   ->setStyleSheet("padding:4px;background-color: #ffffff;" );

        //summaryProgress ->show();
        //summaryFound    ->show();
        //summaryMinMax   ->show();
        searchType = OT_NONE;

        clearButton->setText(tr("Clear"));
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


        horizontalHeader0->setText(tr("DATE\nClick date to Restore"));
        horizontalHeader1->setText("");

        guiDisplayTable->horizontalHeader()->hide();
}

void DailySearchTab::delayedCreateUi() {
        // meed delay to insure days are populated.
        if (createUiFinished) return;
        createUiFinished = true;

        selectCommandCombo->clear();
        selectCommandCombo->addItem(tr("Notes"),OT_NOTES);
        selectCommandCombo->addItem(tr("Notes containing"),OT_NOTES_STRING);
        selectCommandCombo->addItem(tr("Bookmarks"),OT_BOOKMARKS);
        selectCommandCombo->addItem(tr("Bookmarks containing"),OT_BOOKMARKS_STRING);
        selectCommandCombo->addItem(tr("AHI "),OT_AHI);
        selectCommandCombo->addItem(tr("Daily Duration"),OT_DAILY_USAGE);
        selectCommandCombo->addItem(tr("Session Duration" ),OT_SESSION_LENGTH);
        selectCommandCombo->addItem(tr("Disabled Sessions"),OT_DISABLED_SESSIONS);
        selectCommandCombo->addItem(tr("Number of Sessions"),OT_SESSIONS_QTY);
        selectCommandCombo->insertSeparator(selectCommandCombo->count());  // separate from events

        opCodeMap.clear();
        opCodeMap.insert( opCodeStr(OP_LT),OP_LT);
        opCodeMap.insert( opCodeStr(OP_GT),OP_GT);
        opCodeMap.insert( opCodeStr(OP_NE),OP_NE);
        opCodeMap.insert( opCodeStr(OP_LE),OP_LE);
        opCodeMap.insert( opCodeStr(OP_GE),OP_GE);
        opCodeMap.insert( opCodeStr(OP_EQ),OP_EQ);
        opCodeMap.insert( opCodeStr(OP_NE),OP_NE);
        opCodeMap.insert( opCodeStr(OP_CONTAINS),OP_CONTAINS);
        opCodeMap.insert( opCodeStr(OP_WILDCARD),OP_WILDCARD);

        // The order here is the order in the popup box 
        selectOperationCombo->clear();
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
        on_clearButton_clicked();
}

void    DailySearchTab::on_helpButton_clicked() {
        helpMode = !helpMode;
        if (helpMode) {
            helpButton->setText(tr("Click HERE to close help"));
            helpInfo ->setVisible(true);
        } else {
            helpInfo ->setVisible(false);
            helpButton->setText(tr("Help"));
        }
}

QRegExp DailySearchTab::searchPatterToRegex (QString searchPattern) {

        const static QChar bSlash('\\');
        const static QChar asterisk('*');
        const static QChar qMark('?');
        const static QChar emptyChar('\0');
        const QString emptyStr("");
        const QString anyStr(".*");
        const QString singleStr(".");


        searchPattern = searchPattern.simplified();
        //QString wilDebug = searchPattern;
        // 
        // wildcard searches uses  '*' , '?' and '\'
        // '*' will match zero or more characters. '?' will match one character. the escape character '\' always matches the next character.
        //  '\\' will match '\'. '\*' will match '*'. '\?' mach for '?'. otherwise the '\' is ignored

        // The user request will be mapped into a valid QRegularExpression. All RegExp meta characters in the request must be handled.
        // Most of the meta characters will be escapped. backslash, asterisk, question mark will be treated.
        // '\*' -> '\*'   
        // '\?' -> '\?' 
        // '*'  -> '.*'      // really  asterisk followed by asterisk or questionmark -> as a single asterisk.
        // '?'  -> '.' 
        // '.'  -> '\.'  // default for all other  meta characters.
        // '\\' -> '[\\]'

        // QT documentation states regex reserved characetrs are $ () * + . ? [ ] ^ {} |   // seems to be missing / \ -
        // Regular expression reserved characters / \ [ ] () {} | + ^ . $ ? * - 


        static const QString metaClass = QString( "[ / \\\\ \\[ \\]  ( ) { } | + ^ . $ ? * - ]").replace(" ",""); // slash,bSlash,[,],(,),{,},+,^,.,$,?,*,-,|
        static const QRegExp metaCharRegex(metaClass);  
        #if 0
        // Verify search pattern
        if (!metaCharRegex.isValid()) {
            DEBUGFW Q(metaCharRegex.errorString()) Q(metaCharRegex) O("============================================");
            return QRegExp();
        }
        #endif

        // regex meta characetrs. all regex meta character must be acounts to us regular expression to make wildcard work.
        // they will be escaped. except for * and ? which will be treated separately
        searchPattern = searchPattern.simplified();               // remove witespace at ends. and multiple white space to a single space.

        // now handle each meta character requested.
        int pos=0;
        int len=1;
        QString replace;
        QChar metaChar;
        QChar nextChar;
        while (pos < (len = searchPattern.length()) ) {
            pos = searchPattern.indexOf(metaCharRegex,pos);
            if (pos<0) break;
            metaChar = searchPattern.at(pos);
            if (pos+1<len) {
                nextChar = searchPattern.at(pos+1);
            } else {
                nextChar = emptyChar;
            }

            int replaceCount = 1;
            if (metaChar == asterisk ){
                int next=pos+1;
                // discard any successive wildcard type of characters.
                while ( (nextChar == asterisk ) || (nextChar == qMark ) ) {
                    searchPattern.remove(next,1);
                    len = searchPattern.length();
                    if (next>=len) break;
                    nextChar = searchPattern.at(next);
                } 
                replace = anyStr;            // if asterisk then write dot asterisk 
            } else if (metaChar == qMark ) {
                replace = singleStr;
            } else {
                if ((metaChar == bSlash ) ) {
                    if ( ((nextChar == bSlash ) || (nextChar == asterisk ) || (nextChar == qMark ) ) )  {
                        pos+=2; continue;
                    } 
                    replace = emptyStr;         //match next character. same as deleteing the backslash
                } else {
                    // Now have a regex reserved character that needs escaping.
                    // insert an escape '\' before meta characters.
                    replace = QString("\\%1").arg(metaChar);
                }
            }
            searchPattern.replace(pos,replaceCount,replace);
            pos+=replace.length();  // skip over characters added.
        }


        // searchPattern = QString("^.*%1.*$").arg(searchPattern);       // add asterisk to end end points.
        QRegExp convertedRegex =QRegExp(searchPattern,Qt::CaseInsensitive, QRegExp::RegExp);
        // verify convertedRegex to use
        if (!convertedRegex.isValid()) {
            qWarning() << QFileInfo( __FILE__).baseName() <<"[" << __LINE__ << "] " <<  convertedRegex.errorString() << convertedRegex ;
            return QRegExp();
        }
        return convertedRegex;
}

bool    DailySearchTab::compare(QString find , QString target) {
        OpCode opCode = selectOperationOpCode;
        bool ret=false;
        if (opCode==OP_CONTAINS) {
            ret =  target.contains(find,Qt::CaseInsensitive);
        } else if (opCode==OP_WILDCARD) {
            QRegExp regex =  searchPatterToRegex(find);
            ret =  target.contains(regex);
        }
        return ret;
}

bool    DailySearchTab::compare(int aa , int bb) {
        OpCode opCode = selectOperationOpCode;
        if (opCode>=OP_END_NUMERIC) return false;
        int mode=0;
        if (aa <bb ) {
            mode |= OP_LT;
        } else if (aa >bb ) {
            mode |= OP_GT;
        } else {
            mode |= OP_EQ;
        }
        return ( (mode & (int)opCode)!=0);
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
            case displayWhole:
            case opWhole:
                return QString().setNum(value);
            break;
            case displayString:
            case opString:
                return foundString;
            break;
            case invalidValueMode:
            case notUsed:
            break;
        }
        return defaultValue;
}

void    DailySearchTab::on_selectOperationCombo_activated(int index) {
        QString text = selectOperationCombo->itemText(index);
        OpCode opCode = opCodeMap[text];
        if (opCode>OP_INVALID && opCode < OP_END_NUMERIC) {
            selectOperationOpCode = opCode;
            selectOperationButton->setText(opCodeStr(selectOperationOpCode));
        } else if (opCode == OP_CONTAINS || opCode == OP_WILDCARD) {
            selectOperationOpCode = opCode;
            selectOperationButton->setText(opCodeStr(selectOperationOpCode));
        } else {
            // null case;
        }
        setOperationPopupEnabled(false);
        criteriaChanged();


};

void    DailySearchTab::on_selectCommandCombo_activated(int index) {
        // here to select new search criteria
        // must reset all variables and label, button, etc
        on_clearButton_clicked() ;

        valueMode = notUsed;
        selectValue = 0;

        // workaround for combo box alignmnet and sizing.
        // copy selections to a pushbutton. hide combobox and show pushButton. Pushbutton activation can show popup.
        // always hide first before show. allows for best fit
        selectCommandButton->setText(selectCommandCombo->itemText(index));
        setCommandPopupEnabled(false);
        selectOperationOpCode = OP_INVALID;

        // get item selected
        searchType = selectCommandCombo->itemData(index).toInt();
        switch (searchType) {
            case OT_NONE :
                // should never get here.
                horizontalHeader1->setText("");
                nextTab = TW_NONE ;
                setSelectOperation( OP_INVALID ,notUsed);
                break;
            case OT_DISABLED_SESSIONS :
                horizontalHeader1->setText(tr("Number Disabled Session\nJumps to Notes"));
                nextTab = TW_DETAILED ;
                selectInteger->setValue(0);
                setSelectOperation(OP_NO_PARMS,displayWhole);
                break;
            case OT_NOTES :
                horizontalHeader1->setText(tr("Note\nJumps to Notes"));
                nextTab = TW_NOTES ;
                setSelectOperation( OP_NO_PARMS ,displayString);
                break;
            case OT_BOOKMARKS :
                horizontalHeader1->setText(tr("Jumps to Bookmark"));
                nextTab = TW_BOOKMARK ;
                setSelectOperation( OP_NO_PARMS ,displayString);
                break;
            case OT_BOOKMARKS_STRING :
                horizontalHeader1->setText(tr("Jumps to Bookmark"));
                nextTab = TW_BOOKMARK ;
                //setSelectOperation(OP_CONTAINS,opString);
                setSelectOperation(OP_WILDCARD,opString);
                selectString->clear();
                break;
            case OT_NOTES_STRING :
                horizontalHeader1->setText(tr("Note\nJumps to Notes"));
                nextTab = TW_NOTES ;
                //setSelectOperation(OP_CONTAINS,opString);
                setSelectOperation(OP_WILDCARD,opString);
                selectString->clear();
                break;
            case OT_AHI :
                horizontalHeader1->setText(tr("AHI\nJumps to Details"));
                nextTab = TW_DETAILED ;
                setSelectOperation(OP_GT,hundredths);
                selectDouble->setValue(5.0);
                break;
            case OT_SESSION_LENGTH :
                horizontalHeader1->setText(tr("Session Duration\nJumps to Details"));
                nextTab = TW_DETAILED ;
                setSelectOperation(OP_LT,minutesToMs);
                selectDouble->setValue(5.0);
                selectInteger->setValue((int)selectDouble->value()*60000.0);   //convert to ms
                break;
            case OT_SESSIONS_QTY :
                horizontalHeader1->setText(tr("Number of Sessions\nJumps to Details"));
                nextTab = TW_DETAILED ;
                setSelectOperation(OP_GT,opWhole);
                selectInteger->setRange(0,999);
                selectInteger->setValue(1);
                break;
            case OT_DAILY_USAGE :
                horizontalHeader1->setText(tr("Daily Duration\nJumps to Details"));
                nextTab = TW_DETAILED ;
                setSelectOperation(OP_LT,hoursToMs);
                selectDouble->setValue(p_profile->cpap->complianceHours());
                selectInteger->setValue((int)selectDouble->value()*3600000.0);   //convert to ms
                break;
            default:
                // Have an Event 
                horizontalHeader1->setText(tr("Number of events\nJumps to Events"));
                nextTab = TW_EVENTS ;
                setSelectOperation(OP_GT,opWhole);
                selectInteger->setValue(0);
                break;
        }

        criteriaChanged();
        if (selectOperationOpCode == OP_NO_PARMS ) {
            // auto start searching
            startButton->setText(tr("Automatic start"));
            startButtonMode=true;
            on_startButton_clicked();
            return;
        }
        return;
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
        Qt::Alignment alignment=Qt::AlignCenter;
        switch (searchType) {
            case OT_DISABLED_SESSIONS :
                {
                qint32 numDisabled=0;
                QList<Session *> sessions = day->getSessions(MT_CPAP,true);
                for (auto & sess : sessions) {
                    if (!sess->enabled()) {
                        numDisabled ++;
                        found=true;
                    }
                }
                updateValues(numDisabled);
                //if (found) {
                    //QString displayStr= QString("%1/%2").arg(numDisabled).arg(sessions.size());
                    //addItem(date , displayStr,alignment );
                    //return true;
                //}
                }
                break;
            case OT_NOTES :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Journal_Notes)) {
                    QString jcontents = convertRichText2Plain(journal->settings[Journal_Notes].toString());
                    foundString = jcontents.simplified().left(stringDisplayLen).simplified();
                    found=true;
                    alignment=Qt::AlignLeft;
                }
                }
                break;
            case OT_BOOKMARKS :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Bookmark_Notes)) {
                    found=true;
                    QStringList notes = journal->settings[Bookmark_Notes].toStringList();
                    for (   const auto & note : notes) {
                       foundString = note.simplified().left(stringDisplayLen).simplified();
                       alignment=Qt::AlignLeft;
                       break;
                    }
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
                        //if (note.contains(findStr,Qt::CaseInsensitive) ) 
                        if (compare(findStr , note)) 
                        {
                           found=true;
                           foundString = note.simplified().left(stringDisplayLen).simplified();
                           alignment=Qt::AlignLeft;
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
                       foundString = jcontents.simplified().left(stringDisplayLen).simplified();
                       alignment=Qt::AlignLeft;
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
            addItem(date , valueToString(foundValue,"------"),alignment );
            return true;
        }
        return false;
};

void DailySearchTab::search(QDate date)
{
        guiProgressBar->show();
        statusProgress->show();
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
            guiProgressBar->setValue(daysSearched);
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

void DailySearchTab::addItem(QDate date, QString value,Qt::Alignment alignment) {
        int row = passFound;

        QTableWidgetItem *item = new QTableWidgetItem(*m_icon_notSelected,date.toString());
        item->setData(dateRole,date);
        item->setData(valueRole,value);
        item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);

        QTableWidgetItem *item2 = new QTableWidgetItem(*m_icon_notSelected,value);
        item2->setTextAlignment(alignment|Qt::AlignVCenter);
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
            startButton->setText(tr("Continue Search"));
            guiDisplayTable->horizontalHeader()->show();
        } else if (daysFound>0) {
            statusProgress->setText(centerLine(tr("End of Search")));
            statusProgress->show();
            startButton->setEnabled(false);
            guiDisplayTable->horizontalHeader()->show();
        } else {
            statusProgress->setText(centerLine(tr("No Matches")));
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
        if (selectOperationOpCode == OP_CONTAINS ) {
            selectOperationOpCode = OP_WILDCARD;
        } else if (selectOperationOpCode == OP_WILDCARD) {
            selectOperationOpCode = OP_CONTAINS ;
        } else {
            setOperationPopupEnabled(true);
            return;
        }
        selectOperationButton->setText(opCodeStr(selectOperationOpCode));
        criteriaChanged();
};


void    DailySearchTab::on_selectMatch_clicked() {
        setCommandPopupEnabled(true);
}

void    DailySearchTab::on_selectCommandButton_clicked()
{
        setCommandPopupEnabled(true);
}

void    DailySearchTab::setOperationPopupEnabled(bool on) {
        //if (selectOperationOpCode<OP_INVALID || selectOperationOpCode >= OP_END_NUMERIC) return;
        if (on) {
            selectOperationCombo->setEnabled(true);
            selectOperationCombo->showPopup();
            selectOperationButton->show();
        } else {
            selectOperationCombo->hidePopup();
            selectOperationCombo->setEnabled(false);
            selectOperationCombo->hide();
            selectOperationButton->show();
        }

}

void    DailySearchTab::setSelectOperation(OpCode opCode,ValueMode mode)  {
        valueMode = mode;
        selectOperationOpCode = opCode;
        selectOperationButton->setText(opCodeStr(selectOperationOpCode));
        setOperationPopupEnabled(false);

        if (opCode > OP_INVALID && opCode <OP_END_NUMERIC)  {
            // Have numbers
            selectDouble->setDecimals(2);
            selectDouble->setRange(0,999);
            selectDouble->setDecimals(2);
            selectInteger->setRange(0,999);
        }
        switch (valueMode) {
            case hundredths :
                selectDouble->show();
            break;
            case hoursToMs:
                selectUnits->setText(" Hours");
                selectUnits->show();
                selectDouble->show();
                break;
            case minutesToMs:
                selectUnits->setText(" Minutes");
                selectUnits->show();
                selectDouble->setRange(0,9999);
                selectDouble->show();
                break;
            case opWhole:
                selectInteger->show();
            case displayWhole:
            break;
            case opString:
                selectOperationButton->show();
                selectString ->show();
            break;
            case    displayString:
                selectString ->hide();
                break;
            case invalidValueMode:
            case notUsed:
                break;
        }

}


void    DailySearchTab::on_clearButton_clicked() 
{
        // make these button text back to start.
        selectCommandButton->setText(tr("Select Match"));
        startButton->setText(tr("Start Search"));

        // hide widgets
        //Reset Select area
        selectCommandCombo->hide();
        setCommandPopupEnabled(false);
        selectCommandButton->show();

        selectOperationCombo->hide();
        setOperationPopupEnabled(false);
        selectOperationButton->hide();

        selectDouble->hide();
        selectInteger->hide();
        selectString->hide();
        selectUnits->hide();


        //Reset Start area
        startButtonMode=true;
        startButton->setEnabled( false);
        statusProgress->hide();

        // reset summary line
        summaryProgress->hide();
        summaryFound->hide();
        summaryMinMax->hide();


        guiProgressBar->hide();
        // clear display table && hide
        guiDisplayTable->horizontalHeader()->hide();
        for (int index=0; index<guiDisplayTable->rowCount();index++) {
            guiDisplayTable->setRowHidden(index,true);
        }
        guiDisplayTable->horizontalHeader()->hide();

}

void    DailySearchTab::on_startButton_clicked()
{
        if (startButtonMode) {
            // have start mode
            // must set up search from the latest date and go to the first date.

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
        summaryProgress->show();

        // display days searched
        QString skip= daysSkipped==0?"":QString(tr(" Skip:%1")).arg(daysSkipped);
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
        summaryProgress->show();
        summaryFound->show();
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
            case displayWhole:
            case opWhole:
                selectValue = selectInteger->value();;
            break;
            case opString:
            case displayString:
            case invalidValueMode:
            case notUsed:
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

        //initialize progress bar.
        guiProgressBar->hide();
        guiProgressBar->setMinimum(0);
        guiProgressBar->setMaximum(daysTotal);
        guiProgressBar->setTextVisible(true);
        //guiProgressBar->setTextVisible(false);
        guiProgressBar->setMaximumHeight(15);
        guiProgressBar->reset();
}

// inputs  character string.
// outputs cwa centered html string.
// converts \n to <br>
QString DailySearchTab::centerLine(QString line) {
        return QString( "<center>%1</center>").arg(line).replace("\n","<br>");
}

QString DailySearchTab::helpStr() {
        return (tr (
"Finds days that match specified criteria.\t Searches from last day to first day\n"
"\n"
"Click on the Match Button to start.\t\t Next choose the match topic to run\n"
"\n"
"Different topics use different operations \t numberic, character, or none. \n"
"Numberic  Operations:\t >. >=, <, <=, ==, !=.\n"
"Character Operations:\t '*?' for wildcard \t" u8"\u2208" " for Normal matching\n"
"Click on the operation to change\n"
"Any White Space will match any white space in the target.\n"
"Space is always ignored at the start or end.\n"
"\n"
"Wildcards use 3 characters: '*' asterisk, '?' Question Mark '\\' baclspace.\n"
"'*' matches any number of characters.\t '?' matches a single character. \n;"
"'\\' the next character is matched.\t Allowing '*' and '?' to be matched \n"
"'\\*' matchs '*' \t '\\?' matches '?' \t '\\\\' matches '\\' \n"
"\n"
"Result Table\n"
"Column One: Date of match. Clicking loads the date and checkbox marked.\n"
"Column two: Information. Clicking loads the date, checkbox marked, jumps to a tab.\n"
) );
}

QString DailySearchTab::formatTime (qint32 ms) {
        qint32 hours = ms / 3600000;
        ms = ms % 3600000;
        qint32 minutes = ms / 60000;
        ms = ms % 60000;
        qint32 seconds = ms /1000;
        return QString(tr("%1h %2m %3s")).arg(hours).arg(minutes).arg(seconds); 
}

QString DailySearchTab::convertRichText2Plain (QString rich) {
        richText.setHtml(rich);
        QString line=richText.toPlainText().simplified();
        return line.replace(QRegExp("[\\s\\r\\n]+")," ").simplified();
}

QString DailySearchTab::opCodeStr(OpCode opCode) {
        switch (opCode) {
            case OP_GT : return "> ";
            case OP_GE : return ">=";
            case OP_LT : return "< ";
            case OP_LE : return "<=";
            case OP_EQ : return "==";
            case OP_NE : return "!=";
            case OP_CONTAINS : return QChar(0x2208);    // or use 0x220B
            case OP_WILDCARD : return "*?";
            default:
            break;
        }
        return QString();
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

