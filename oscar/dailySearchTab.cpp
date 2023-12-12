/* user graph settings Implementation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

// Issues no clear of results between runs or between passes.
// automatically open event. in graph view for events TAB

#define TEST_MACROS_ENABLED
#include <test_macros.h>

#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QProgressBar>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPixmap>
#include <QSize>
#include <QChar>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QtGlobal>
#include <QHeaderView>
#include <QCoreApplication>
#include "dailySearchTab.h"
#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "daily.h"
#include "SleepLib/machine_common.h"

enum DS_COL { DS_COL_LEFT=0, DS_COL_RIGHT, DS_COL_MAX };
enum DS_ROW{ DS_ROW_HEADER, DS_ROW_DATA };
#define DS_ROW_MAX         (passDisplayLimit+DS_ROW_DATA)


/* layout  of searchTAB
+========================+
|           HELP         |
+========================+
|       HELPText         |
+========================+
| Match | Clear | start  |
|------------------------|
| control:cmd op value   |
+========================+
|     Progress Bar       | 
+========================+
|          Summary       |
+========================+
|          RESULTS       |
+========================+
*/


DailySearchTab::DailySearchTab(Daily* daily , QWidget* searchTabWidget  , QTabWidget* dailyTabWidget)  :
                        daily(daily) , parent(daily) , searchTabWidget(searchTabWidget) ,dailyTabWidget(dailyTabWidget)
{
    m_icon_selected      = new QIcon(":/icons/checkmark.png");
    m_icon_notSelected   = new QIcon(":/icons/empty_box.png");
    m_icon_configure     = new QIcon(":/icons/cog.png");
    createUi();
    connectUi(true);
}

DailySearchTab::~DailySearchTab() {
    connectUi(false);
    delete m_icon_selected;
    delete m_icon_notSelected;
    delete m_icon_configure ;
};

void    DailySearchTab::createUi() {

        searchTabLayout  = new QVBoxLayout(searchTabWidget);

        resultTable      = new QTableWidget(DS_ROW_MAX,DS_COL_MAX,searchTabWidget);

        helpButton       = new QPushButton(searchTabWidget);
        helpText         = new QTextEdit(searchTabWidget);

        startWidget      = new QWidget(searchTabWidget);
        startLayout      = new QHBoxLayout;
        matchButton      = new QPushButton( startWidget);
        clearButton      = new QPushButton( startWidget);
        startButton      = new QPushButton( startWidget);


        commandWidget    = new QWidget(searchTabWidget);
        commandLayout    = new QHBoxLayout();
        commandButton    = new QPushButton(commandWidget);
        operationCombo   = new QComboBox(commandWidget);
        operationButton  = new QPushButton(commandWidget);
        selectDouble     = new QDoubleSpinBox(commandWidget);
        selectInteger    = new QSpinBox(commandWidget);
        selectString     = new QLineEdit(commandWidget);
        selectUnits      = new QLabel(commandWidget);

        commandList      = new QListWidget(resultTable);

        summaryWidget    = new QWidget(searchTabWidget);
        summaryLayout    = new QHBoxLayout();
        summaryProgress  = new QPushButton(summaryWidget);
        summaryFound     = new QPushButton(summaryWidget);
        summaryMinMax    = new QPushButton(summaryWidget);
        progressBar      = new QProgressBar(searchTabWidget);

        populateControl();

        searchTabLayout->setContentsMargins(0, 0, 0, 0);
        searchTabLayout->addSpacing(2);
        searchTabLayout->setMargin(2);

        startLayout->addWidget(matchButton);
        startLayout->addWidget(clearButton);
        startLayout->addWidget(startButton);
        startLayout->addStretch(0);
        startLayout->addSpacing(2);
        startLayout->setMargin(2);
        startWidget->setLayout(startLayout);

        commandLayout->addWidget(commandButton);
        commandLayout->addWidget(operationCombo);
        commandLayout->addWidget(operationButton);
        commandLayout->addWidget(selectInteger);
        commandLayout->addWidget(selectString);
        commandLayout->addWidget(selectDouble);
        commandLayout->addWidget(selectUnits);

        commandLayout->setMargin(2);
        commandLayout->setSpacing(2);
        commandLayout->addStretch(0);
        commandWidget->setLayout(commandLayout);

        summaryLayout->addWidget(summaryProgress);
        summaryLayout->addWidget(summaryFound);
        summaryLayout->addWidget(summaryMinMax);

        summaryLayout->setMargin(2);
        summaryLayout->setSpacing(2);
        summaryWidget->setLayout(summaryLayout);


        QString styleSheetWidget = QString("border: 1px solid black; padding:none;");
        startWidget->setStyleSheet(styleSheetWidget);

        searchTabLayout ->addWidget(helpButton);
        searchTabLayout ->addWidget(helpText);
        searchTabLayout ->addWidget(startWidget);
        searchTabLayout ->addWidget(commandWidget);
        searchTabLayout ->addWidget(commandList);
        searchTabLayout ->addWidget(progressBar);
        searchTabLayout ->addWidget(summaryWidget);
        searchTabLayout ->addWidget(resultTable);

        // End of UI creatation
        //Setup each BUtton / control item

        QString styleButton=QString(
            "QPushButton { color: black; border: 1px solid black; padding: 5px ;background-color:white; }"
            "QPushButton:disabled { background-color: #EEEEFF;}"
            //"QPushButton:disabled { color: #333333; border: 1px solid #333333; background-color: #dddddd;}"
            );
        QSizePolicy sizePolicyEP = QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        QSizePolicy sizePolicyEE = QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        QSizePolicy sizePolicyEM = QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
        QSizePolicy sizePolicyMM = QSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        QSizePolicy sizePolicyMxMx = QSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum);
        Q_UNUSED (sizePolicyEP);
        Q_UNUSED (sizePolicyMM);
        Q_UNUSED (sizePolicyMxMx);

        helpText->setReadOnly(true);
        helpText->setLineWrapMode(QTextEdit::NoWrap);
        QSize size = QFontMetrics(this->font()).size(0, helpString);
        size.rheight() += 35 ; // scrollbar 
        size.rwidth()  += 35 ; // scrollbar 
        helpText->setText(helpString);
        helpText->setMinimumSize(textsize(this->font(),helpString));
        helpText->setSizePolicy( sizePolicyEE );

        helpButton->setStyleSheet( styleButton );
        // helpButton->setText(tr("Help"));
        helpMode = true;
        on_helpButton_clicked();

        matchButton->setIcon(*m_icon_configure);
        matchButton->setStyleSheet( styleButton );
        clearButton->setStyleSheet( styleButton );
        //startButton->setStyleSheet( styleButton );
        //matchButton->setSizePolicy( sizePolicyEE);
        //clearButton->setSizePolicy( sizePolicyEE);
        //startButton->setSizePolicy( sizePolicyEE);
        //startWidget->setSizePolicy( sizePolicyEM);
        setText(matchButton,tr("Match"));
        setText(clearButton,tr("Clear"));


        commandButton->setStyleSheet("border:none;");

        //float height = float(1+commandList->count())*commandListItemHeight ;
        float height = float(commandList->count())*commandListItemHeight ;
        commandList->setMinimumHeight(height);
        commandList->setMinimumWidth(commandListItemMaxWidth);
        setCommandPopupEnabled(false);

        setText(operationButton,"");
        operationButton->setStyleSheet("border:none;");
        operationButton->hide();
        operationCombo->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        setOperationPopupEnabled(false);

        selectDouble->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectInteger->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectString->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectUnits->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        setText(selectUnits,"");

        progressBar->setStyleSheet(
            "QProgressBar{border: 1px solid black; text-align: center;}"
            "QProgressBar::chunk { border: none; background-color: #ccddFF; } ");


        //QString styleLabel=QString( "QLabel { color: black; border: 1px solid black; padding: 5px ;background-color:white; }");
        summaryProgress->setStyleSheet( styleButton );
        summaryFound->setStyleSheet( styleButton );
        summaryMinMax->setStyleSheet( styleButton );
        summaryProgress->setSizePolicy( sizePolicyEM ) ;
        summaryFound->setSizePolicy( sizePolicyEM ) ;
        summaryMinMax->setSizePolicy( sizePolicyEM ) ;

        resultTable->horizontalHeader()->hide();   // hides numbers above each column
        resultTable->horizontalHeader()->setStretchLastSection(true);
        // get rid of selection coloring
        resultTable->setStyleSheet("QTableView{background-color: white; selection-background-color: white; }");

        float width = 14/*styleSheet:padding+border*/ + QFontMetrics(this->font()).size(Qt::TextSingleLine ,"WWW MMM 99 2222").width();
        resultTable->setColumnWidth(DS_COL_LEFT, width);

        width = 30/*iconWidthPlus*/+QFontMetrics(this->font()).size(Qt::TextSingleLine ,clearButton->text()).width();
        //width = clearButton->width();
        resultTable->setShowGrid(false);


        setResult(DS_ROW_HEADER,0,QDate(),tr("DATE\nJumps to Date"));
        on_clearButton_clicked();
}

void DailySearchTab::connectUi(bool doConnect) {
    if (doConnect) {
        daily->connect(startButton,     SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
        daily->connect(clearButton,     SIGNAL(clicked()), this, SLOT(on_clearButton_clicked()) );
        daily->connect(matchButton,     SIGNAL(clicked()), this, SLOT(on_matchButton_clicked()) );
        daily->connect(helpButton   ,   SIGNAL(clicked()), this, SLOT(on_helpButton_clicked()) );

        daily->connect(commandButton,   SIGNAL(clicked()), this, SLOT(on_commandButton_clicked()) );
        daily->connect(operationButton, SIGNAL(clicked()), this, SLOT(on_operationButton_clicked()) );
        
        daily->connect(commandList,     SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(on_commandList_activated(QListWidgetItem*)   ));
        daily->connect(commandList,     SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(on_commandList_activated(QListWidgetItem*)   ));
        daily->connect(operationCombo,  SIGNAL(activated(int)), this, SLOT(on_operationCombo_activated(int)   ));

        daily->connect(selectInteger,   SIGNAL(valueChanged(int)), this, SLOT(on_intValueChanged(int)) );
        daily->connect(selectDouble,    SIGNAL(valueChanged(double)), this, SLOT(on_doubleValueChanged(double)) );
        daily->connect(selectString,    SIGNAL(textEdited(QString)), this, SLOT(on_textEdited(QString)) );

    } else {
        daily->disconnect(startButton,     SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
        daily->disconnect(clearButton,     SIGNAL(clicked()), this, SLOT(on_clearButton_clicked()) );
        daily->disconnect(matchButton,     SIGNAL(clicked()), this, SLOT(on_matchButton_clicked()) );
        daily->disconnect(helpButton   ,   SIGNAL(clicked()), this, SLOT(on_helpButton_clicked()) );

        daily->disconnect(commandButton,   SIGNAL(clicked()), this, SLOT(on_commandButton_clicked()) );
        daily->disconnect(operationButton, SIGNAL(clicked()), this, SLOT(on_operationButton_clicked()) );
        
        daily->disconnect(commandList,     SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(on_commandList_activated(QListWidgetItem*)   ));
        daily->disconnect(commandList,     SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(on_commandList_activated(QListWidgetItem*)   ));
        daily->disconnect(operationCombo,  SIGNAL(activated(int)), this, SLOT(on_operationCombo_activated(int)   ));

        daily->disconnect(selectInteger,   SIGNAL(valueChanged(int)), this, SLOT(on_intValueChanged(int)) );
        daily->disconnect(selectDouble,    SIGNAL(valueChanged(double)), this, SLOT(on_doubleValueChanged(double)) );
        daily->disconnect(selectString,    SIGNAL(textEdited(QString)), this, SLOT(on_textEdited(QString)) );

    }
}

QListWidgetItem* DailySearchTab::calculateMaxSize(QString str,int topic) {  //calculate max size of strings
    float scaleX= (float)(QWidget::logicalDpiX()*100.0)/(float)(QWidget::physicalDpiX());
    float percentX = scaleX/100.0;
    float width = QFontMetricsF(this->font()).size(Qt::TextSingleLine , str).width();
    width += 30 ; // account for scrollbar width;
    commandListItemMaxWidth = max (commandListItemMaxWidth, (width*percentX));
    commandListItemHeight = QFontMetricsF(this->font()).size(Qt::TextSingleLine , str).height();
    QListWidgetItem* item = new QListWidgetItem(str);
    item->setData(Qt::UserRole,topic);
    return item;
}

void DailySearchTab::updateEvents(ChannelID id,QString fullname) {
    if (commandEventList.contains(fullname)) return;
    commandEventList.insert(fullname);
    commandList->addItem(calculateMaxSize(fullname,id));
}

void DailySearchTab::populateControl() {
        commandList->clear(); 
        commandList->addItem(calculateMaxSize(tr("Notes"),ST_NOTES));
        commandList->addItem(calculateMaxSize(tr("Notes containing"),ST_NOTES_STRING));
        commandList->addItem(calculateMaxSize(tr("Bookmarks"),ST_BOOKMARKS));
        commandList->addItem(calculateMaxSize(tr("Bookmarks containing"),ST_BOOKMARKS_STRING));
        commandList->addItem(calculateMaxSize(tr("AHI "),ST_AHI));
        commandList->addItem(calculateMaxSize(tr("Daily Duration"),ST_DAILY_USAGE));
        commandList->addItem(calculateMaxSize(tr("Session Duration" ),ST_SESSION_LENGTH));
        commandList->addItem(calculateMaxSize(tr("Days Skipped"),ST_DAYS_SKIPPED));
        commandList->addItem(calculateMaxSize(tr("Apnea Length"),ST_APNEA_LENGTH));
        if ( !p_profile->cpap->clinicalMode() ) {
            commandList->addItem(calculateMaxSize(tr("Disabled Sessions"),ST_DISABLED_SESSIONS));
        }
        commandList->addItem(calculateMaxSize(tr("Number of Sessions"),ST_SESSIONS_QTY));
        //commandList->insertSeparator(commandList->count());  // separate from events

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
        operationCombo->clear();
        operationCombo->addItem(opCodeStr(OP_LT));
        operationCombo->addItem(opCodeStr(OP_GT));
        operationCombo->addItem(opCodeStr(OP_LE));
        operationCombo->addItem(opCodeStr(OP_GE));
        operationCombo->addItem(opCodeStr(OP_EQ));
        operationCombo->addItem(opCodeStr(OP_NE));

        // Now add events
        QDate date = p_profile->LastDay(MT_CPAP);
        if ( !date.isValid()) return;
        Day* day = p_profile->GetDay(date);
        if (!day) return;

        qint32 chans = schema::SPAN | schema::FLAG | schema::MINOR_FLAG;
        if (p_profile->general->showUnknownFlags()) chans |= schema::UNKNOWN;
        QList<ChannelID> available;
        available.append(day->getSortedMachineChannels(chans));
        for (int i=0; i < available.size(); ++i) {
            ChannelID id = available.at(i);
            schema::Channel chan = schema::channel[ id  ];
            updateEvents(id,chan.fullname());
        }

}


void    DailySearchTab::on_helpButton_clicked() {
        helpMode = !helpMode;
        if (helpMode) {
            resultTable->setMinimumSize(QSize(50,200)+textsize(helpText->font(),helpString));
            resultTable->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::MinimumExpanding);
            //setText(helpButton,tr("Click HERE to close Help"));
            helpButton->setText(tr("Click HERE to close Help"));
            helpText ->show();
        } else {
            resultTable->setMinimumWidth(250);
            helpText ->hide();
            //setText(helpButton,tr("Help"));
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
        OpCode opCode = operationOpCode;
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
        OpCode opCode = operationOpCode;
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
}

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
            case secondsDisplayString:
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

void    DailySearchTab::on_operationCombo_activated(int index) {
        QString text = operationCombo->itemText(index);
        OpCode opCode = opCodeMap[text];
        if (opCode>OP_INVALID && opCode < OP_END_NUMERIC) {
            operationOpCode = opCode;
            setText(operationButton,opCodeStr(operationOpCode));
        } else if (opCode == OP_CONTAINS || opCode == OP_WILDCARD) {
            operationOpCode = opCode;
            setText(operationButton,opCodeStr(operationOpCode));
        } else {
            // null case;
        }
        setOperationPopupEnabled(false);
        criteriaChanged();
};

QSize DailySearchTab::setText(QLabel* label ,QString text) {
        QSize size = textsize(label->font(),text);
        int width = size.width();
        width += 20 ; //margings
        label->setMinimumWidth(width);
        label->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        //label->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
        label->setText(text);
        return size;
}

QSize DailySearchTab::setText(QPushButton* but ,QString text) {
        QSize size = textsize(but->font(),text);
        int width = size.width();
        width += but->iconSize().width();
        width += 4 ; //margings
        but->setMinimumWidth(width);
        but->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        //but->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
        but->setText(text);
        return size;
}

void    DailySearchTab::on_commandList_activated(QListWidgetItem* item) {
        // here to select new search criteria
        // must reset all variables and label, button, etc
        on_clearButton_clicked() ;

        valueMode = notUsed;
        selectValue = 0;

        // workaround for combo box alignmnet and sizing.
        // copy selections to a pushbutton. hide combobox and show pushButton. Pushbutton activation can show popup.
        // always hide first before show. allows for best fit
        setText(commandButton, item->text());

        setCommandPopupEnabled(false);
        operationOpCode = OP_INVALID;

        // get item selected
        int itemTopic = item->data(Qt::UserRole).toInt();
        if (itemTopic>=ST_EVENT) {
            channelId = itemTopic;
            searchTopic = ST_EVENT;
        } else {
            searchTopic = (SearchTopic)itemTopic;
        }
        switch (searchTopic) {
            case ST_NONE :
                // should never get here.
                setResult(DS_ROW_HEADER,1,QDate(),"");
                nextTab = TW_NONE ;
                setoperation( OP_INVALID ,notUsed);
                break;
            case ST_DAYS_SKIPPED :
                setResult(DS_ROW_HEADER,1,QDate(),tr("No Data\nJumps to Date's Details "));
                nextTab = TW_DETAILED ;
                setoperation(OP_NO_PARMS,notUsed);
                break;
            case ST_DISABLED_SESSIONS :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Number Disabled Session\nJumps to Date's Details "));
                nextTab = TW_DETAILED ;
                selectInteger->setValue(0);
                setoperation(OP_NO_PARMS,displayWhole);
                break;
            case ST_NOTES :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Note\nJumps to Date's Notes"));
                nextTab = TW_NOTES ;
                setoperation( OP_NO_PARMS ,displayString);
                break;
            case ST_BOOKMARKS :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Bookmark\nJumps to Date's Bookmark"));
                nextTab = TW_BOOKMARK ;
                setoperation( OP_NO_PARMS ,displayString);
                break;
            case ST_BOOKMARKS_STRING :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Bookmark\nJumps to Date's Bookmark"));
                nextTab = TW_BOOKMARK ;
                //setoperation(OP_CONTAINS,opString);
                setoperation(OP_WILDCARD,opString);
                selectString->clear();
                break;
            case ST_NOTES_STRING :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Note\nJumps to Date's Notes"));
                nextTab = TW_NOTES ;
                //setoperation(OP_CONTAINS,opString);
                setoperation(OP_WILDCARD,opString);
                selectString->clear();
                break;
            case ST_AHI :
                setResult(DS_ROW_HEADER,1,QDate(),tr("AHI\nJumps to Date's Details"));
                nextTab = TW_DETAILED ;
                setoperation(OP_GT,hundredths);
                setText(selectUnits,tr(" EventsPerHour"));
                selectDouble->setValue(5.0);
                selectDouble->setSingleStep(0.1);
                break;
            case ST_APNEA_LENGTH :
                DaysWithFileErrors = 0;
                setResult(DS_ROW_HEADER,1,QDate(),tr("Set of Apnea:Length\nJumps to Date's Events"));
                nextTab = TW_EVENTS ;
                setoperation(OP_GE,secondsDisplayString);
                selectInteger->setRange(0,999);
                selectInteger->setValue(25);
                setText(selectUnits,tr(" Seconds"));
                break;
            case ST_SESSION_LENGTH :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Session Duration\nJumps to Date's Details"));
                nextTab = TW_DETAILED ;
                setoperation(OP_LT,minutesToMs);
                selectDouble->setValue(5.0);
                setText(selectUnits,tr(" Minutes"));
                selectDouble->setSingleStep(0.1);
                selectInteger->setValue((int)selectDouble->value()*60000.0);   //convert to ms
                break;
            case ST_SESSIONS_QTY :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Number of Sessions\nJumps to Date's Details"));
                nextTab = TW_DETAILED ;
                setoperation(OP_GT,opWhole);
                selectInteger->setRange(0,999);
                selectInteger->setValue(2);
                setText(selectUnits,tr(" Sessions"));
                break;
            case ST_DAILY_USAGE :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Daily Duration\nJumps to Date's Details"));
                nextTab = TW_DETAILED ;
                setoperation(OP_LT,hoursToMs);
                selectDouble->setValue(p_profile->cpap->complianceHours());
                selectDouble->setSingleStep(0.1);
                selectInteger->setValue((int)selectDouble->value()*3600000.0);   //convert to ms
                setText(selectUnits,tr(" Hours"));
                break;
            case ST_EVENT:
                // Have an Event
                setResult(DS_ROW_HEADER,1,QDate(),tr("Number of events\nJumps to Date's Events"));
                nextTab = TW_EVENTS ;
                setoperation(OP_GT,opWhole);
                selectInteger->setValue(0);
                setText(selectUnits,tr(" Events"));
                break;
        }
        criteriaChanged();
        if (operationOpCode == OP_NO_PARMS ) {
            // auto start searching
            setText(startButton,tr("Automatic start"));
            setColor(startButton,red);
            startButtonMode=true;
            on_startButton_clicked();
            return;
        }
        setColor(startButton,green);
        return;
}

void DailySearchTab::setResult(int row,int column,QDate date,QString text) {
    if(column<0 || column>1) {
        DEBUGTFW O("Column out of range ERROR") Q(row) Q(column) Q(date) Q(text);
        return;
    } else if ( row < DS_ROW_HEADER || row >= DS_ROW_MAX)  {
        DEBUGTFW O("Row out of range ERROR") Q(row) Q(column) Q(date) Q(text);
        return;
    } 

    QWidget* header = resultTable->cellWidget(row,column);
    GPushButton* item;
    if (header == nullptr) {
        item = new GPushButton(row,column,date,this);
        //item->setStyleSheet("QPushButton {text-align: left;vertical-align:top;}");
        item->setStyleSheet(
            "QPushButton { text-align: left;color: black; border: 1px solid black; padding: 5px ;background-color:white; }" );
        resultTable->setCellWidget(row,column,item);
    } else {
        item = dynamic_cast<GPushButton *>(header);
        if (item == nullptr) {
            DEBUGFW Q(header) Q(item) Q(row) Q(column) Q(text) QQ("error","=======================");
            return;
        }
        item->setDate(date);
    }
    if (row == DS_ROW_HEADER) {
        QSize size=setText(item,text);
        resultTable->setRowHeight(DS_ROW_HEADER,8/*margins*/+size.height());
    } else {
        item->setIcon(*m_icon_notSelected);
        if (column == 0) {
            setText(item,date.toString());
        } else {
            setText(item,text);
        }
    }
    if ( row == DS_ROW_DATA )  {
        resultTable->setRowHidden(DS_ROW_HEADER,false);
    }
    resultTable->setRowHidden(row,false);
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


void DailySearchTab::find(QDate& date) {
        QCoreApplication::processEvents();
        bool found=false;
        Qt::Alignment alignment=Qt::AlignCenter;
        Day* day = p_profile->GetDay(date);
        if ( (!day) && (searchTopic != ST_DAYS_SKIPPED)) { daysSkipped++; return;};
        switch (searchTopic) {
            case ST_DAYS_SKIPPED :
                found=!day;
                break;
            case ST_DISABLED_SESSIONS :
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
                }
                break;
            case ST_NOTES :
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
            case ST_BOOKMARKS :
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
            case ST_BOOKMARKS_STRING :
                {
                Session* journal=daily->GetJournalSession(date);
                if (journal && journal->settings.contains(Bookmark_Notes)) {
                    QStringList notes = journal->settings[Bookmark_Notes].toStringList();
                    QString findStr = selectString->text();
                    for (   const auto & note : notes) {
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
            case ST_NOTES_STRING :
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
            case ST_AHI :
                {
                EventDataType dahi =calculateAhi(day);
                dahi += 0.005;
                dahi *= 100.0;
                int ahi = (int)dahi;
                updateValues(ahi);
                found = compare (ahi , selectValue);
                }
                break;
            case ST_APNEA_LENGTH :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                QMap<ChannelID,int> values;
                // find possible channeld to use
                QList<ChannelID> apneaLikeChannels(ahiChannels.begin(),ahiChannels.end());
                #if 1
                if (p_profile->cpap->userEventFlagging()) {
                    apneaLikeChannels.push_back(CPAP_UserFlag1);
                    apneaLikeChannels.push_back(CPAP_UserFlag2);
                }
                #endif
                apneaLikeChannels.push_back(CPAP_RERA);

                QList<ChannelID> chans;
                for( auto code : apneaLikeChannels) {
                    if (day->count(code)) { chans.push_back(code); }
                }
                bool errorFound = false;
                QString result;
                if (!chans.isEmpty()) {
                    for (Session* sess : sessions ) {
                        if (!sess->enabled()) continue;
                        auto keys = sess->eventlist.keys();
                        if (keys.size() <= 0) {
                            bool ok = sess->LoadSummary(false);
                            bool ok1 = sess->OpenEvents(false);
                            keys = sess->eventlist.keys();
                            if ((keys.size() <= 0) || !ok || !ok1 ) {
                                if ((keys.size() <= 0) || !ok || !ok1 ) {
                                    errorFound |= true;
                                    DEBUGFC O(day->date()) O(keys.size()) Q(daysSkipped) O("NO KEYS STILL")  ;
                                    // skip this channel
                                    continue;
                                }
                            }
                        }
                        for( ChannelID code : chans) {
                            auto evlist = sess->eventlist.find(code);
                            if (evlist == sess->eventlist.end()) {
                                continue;
                            }
                            // Now we go through the event list for the *session* (not for the day)
                            for (int z=0;z<evlist.value().size();z++) {
                                EventList & ev=*(evlist.value()[z]);
                                for (quint32 o=0;o<ev.count();o++) {
                                    int sec = evlist.value()[z]->raw(o);
                                    if (compare (sec , selectValue)) {
                                        // save value in map
                                        auto it = values.find(code);
                                        if (it == values.end() ) {
                                            values.insert(code,sec);
                                        } else {
                                            // save max or min value in map
                                            int saved_sec = it.value(); 
                                            // save highest or lowest value.
                                            if (compare (sec ,saved_sec) ) {
                                                *it = sec;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                for ( auto it= values.begin() ; it != values.end() ; it++) {
                    ChannelID code = it.key();
                    int value = it.value();
                    result += QString("%1:%2 ").arg(schema::channel[ code  ].label()).arg(value);
                }
                if (errorFound) {
                    daysSkipped++; 
                    return;
                };
                found = !result.isEmpty();
                if (found) {
                    foundString = result;
                    alignment=Qt::AlignLeft;
                } 
                }
                break;
            case ST_SESSION_LENGTH :
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
            case ST_SESSIONS_QTY :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                qint32 size = sessions.size();
                updateValues(size);
                found=compare (size , selectValue);
                }
                break;
            case ST_DAILY_USAGE :
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
            case ST_EVENT :
                {
                qint32 count = day->count(channelId);
                updateValues(count);
                found=compare (count , selectValue);
                }
                break;
            case ST_NONE :
                break;
        }
        if (found) {
            addItem(date , valueToString(foundValue,"------"),alignment );
            passFound++;
            daysFound++;
        }
        return ;
};

void DailySearchTab::search(QDate date) {
        if (!date.isValid()) {
            qWarning() << "DailySearchTab::find invalid date." << date;
            return;
        }
        hideResults(false);
        foundString.clear();
        passFound=0;
        while (date >= earliestDate) {
            nextDate = date;
            if (passFound >= passDisplayLimit)  break; 

            find(date);
            progressBar->setValue(++daysProcessed);
            date=date.addDays(-1);
        }
        endOfPass();
        return ;
};

void DailySearchTab::addItem(QDate date, QString value,Qt::Alignment alignment) {
        int row = passFound;
        Q_UNUSED(alignment);
        setResult(DS_ROW_DATA+row,0,date,value);
        setResult(DS_ROW_DATA+row,1,date,value);
}

void    DailySearchTab::endOfPass() {
        startButtonMode=false;      // display Continue;
        QString display;
        if (DaysWithFileErrors) {
        }
        if ((passFound >= passDisplayLimit) && (daysProcessed<daysTotal)) {
            startButton->setEnabled(true);
            setText(startButton,(tr("Continue Search")));
            setColor(startButton,green);
        } else if (daysFound>0) {
            startButton->setEnabled(false);
            setText(startButton,tr("End of Search"));
            setColor(startButton,red);
        } else {
            startButton->setEnabled(false);
            setText(startButton,tr("No Matches"));
            setColor(startButton,red);
        }

        displayStatistics();
}

void    DailySearchTab::hideCommand(bool showButton) {
        //operationCombo->hide();
        //operationCombo->clear();
        operationButton->hide();
        selectDouble->hide();
        selectInteger->hide();
        selectString->hide();
        selectUnits->hide();
        commandWidget->setVisible(showButton); 
        commandButton->setVisible(showButton);
};

void    DailySearchTab::setCommandPopupEnabled(bool on) {
        if (on) {
            commandPopupEnabled=true;
            hideCommand();
            commandList->show();
            hideResults(true);
        } else {
            commandPopupEnabled=false;
            commandList->hide();
            hideCommand(true/*show button*/);
        }
}

void    DailySearchTab::on_operationButton_clicked() {
        if (operationOpCode == OP_CONTAINS ) {
            operationOpCode = OP_WILDCARD;
        } else if (operationOpCode == OP_WILDCARD) {
            operationOpCode = OP_CONTAINS ;
        } else {
            setOperationPopupEnabled(true);
            return;
        }
        QString text=opCodeStr(operationOpCode);
        setText(operationButton,text);
        criteriaChanged();
};


void    DailySearchTab::on_matchButton_clicked() {
        setColor(startButton,grey);
        setCommandPopupEnabled(!commandPopupEnabled);
}

void    DailySearchTab::on_commandButton_clicked() {
        setCommandPopupEnabled(true);
}

void    DailySearchTab::setOperationPopupEnabled(bool on) {
        //if (operationOpCode<OP_INVALID || operationOpCode >= OP_END_NUMERIC) return;
        if (on) {
            operationButton->hide();
            operationCombo->show();
            //operationCombo->setEnabled(true);
            operationCombo->showPopup();
        } else {
            operationCombo->hidePopup();
            //operationCombo->setEnabled(false);
            operationCombo->hide();
            operationButton->show();
        }
}

void    DailySearchTab::setoperation(OpCode opCode,ValueMode mode)  {
        valueMode = mode;
        operationOpCode = opCode;
        setText(operationButton,opCodeStr(operationOpCode));
        setOperationPopupEnabled(false);

        if (opCode > OP_INVALID && opCode <OP_END_NUMERIC)  {
            // Have numbers
            selectDouble->setDecimals(2);
            selectDouble->setRange(0,999);
            selectDouble->setDecimals(2);
            selectInteger->setRange(0,999);
            selectDouble->setSingleStep(0.1);
        }
        switch (valueMode) {
            case hundredths :
                selectUnits->show();
                selectDouble->show();
            break;
            case hoursToMs:
                setText(selectUnits,tr(" Hours"));
                selectUnits->show();
                selectDouble->show();
                break;
            case secondsDisplayString:
                setText(selectUnits,tr(" Seconds"));
                selectUnits->show();
                selectInteger->show();
                break;
            case minutesToMs:
                setText(selectUnits,tr(" Minutes"));
                selectUnits->show();
                selectDouble->setRange(0,9999);
                selectDouble->show();
                break;
            case opWhole:
                selectUnits->show();
                selectInteger->show();
                break;
            case displayWhole:
                selectInteger->hide();
                break;
            case opString:
                operationButton->show();
                selectString ->show();
                break;
            case displayString:
                selectString ->hide();
                break;
            case invalidValueMode:
            case notUsed:
                break;
        }

}

void    DailySearchTab::hideResults(bool hide) {
        if (hide) {
            for (int index = DS_ROW_HEADER; index<DS_ROW_MAX;index++) {
                resultTable->setRowHidden(index,true);
            }
        }
        progressBar->setMinimumHeight(matchButton->height());
        progressBar->setVisible(!hide);
        summaryWidget->setVisible(!hide);
}

QSize   DailySearchTab::textsize(QFont font ,QString text) {
        return QFontMetrics(font).size(0 , text);
}

void    DailySearchTab::on_clearButton_clicked()
{
        DaysWithFileErrors = 0 ;
        searchTopic = ST_NONE;
        // make these button text back to start.
        startButton->setText(tr("Start Search"));
        setColor(startButton,grey);
        startButtonMode=true;
        startButton->setEnabled( false);

        // hide widgets
        //Reset Select area
        commandList->hide();
        setCommandPopupEnabled(false);
        setText(commandButton,"");
        commandButton->show();

        operationCombo->hide();
        setOperationPopupEnabled(false);
        operationButton->hide();
        selectDouble->hide();
        selectInteger->hide();
        selectString->hide();
        selectUnits->hide();

        hideResults(true);

        // show these widgets;
        //controlWidget->show();
}

void    DailySearchTab::on_startButton_clicked() {
        hideResults(false);
        setColor(startButton,grey);
        if (startButtonMode) {
            search (latestDate );
            startButtonMode=false;
        } else {
            search (nextDate );
        }
}

void    DailySearchTab::on_intValueChanged(int ) {
        selectInteger->findChild<QLineEdit*>()->deselect();
        criteriaChanged();
}

void    DailySearchTab::on_doubleValueChanged(double ) {
        selectDouble->findChild<QLineEdit*>()->deselect();
        criteriaChanged();
}

void    DailySearchTab::on_textEdited(QString ) {
        criteriaChanged();
}

void    DailySearchTab::setColor(QPushButton* button,QString color)  {
        #if 0
        QPalette pal = button->palette(); 
        pal.setColor(QPalette::Button, color);
        button->setPalette(pal);
        button->setAutoFillBackground(true);
        #else 
        QString style=QString(
            "QPushButton { color: black; border: 1px solid black; padding: 5px ;background-color:%1; }"
            ).arg(color);
            //"QPushButton:disabled { background-color:#EEEEEE ;}"
        button->setStyleSheet(style);
        #endif
        QCoreApplication::processEvents();
}

void    DailySearchTab::displayStatistics() {
        QString extra;
        summaryProgress->show();

        // display days searched
        QString skip= daysSkipped==0?"":QString(tr(" Skip:%1")).arg(daysSkipped);
        setText(summaryProgress,centerLine(QString(tr("%1/%2%3 days")).arg(daysProcessed).arg(daysTotal).arg(skip) ));

        // display days found
        setText(summaryFound,centerLine(QString(tr("Found %1 ")).arg(daysFound) ));

        // display associated value
        extra ="";
        if (minMaxValid) {
            extra = QString("%1 / %2").arg(valueToString(minInteger)).arg(valueToString(maxInteger));
        }
        if (extra.size()>0) {
            setText(summaryMinMax,extra);
            summaryMinMax->show();
        } else {
            if (DaysWithFileErrors) {
                QString msg = tr("File errors:%1");
                setText(summaryMinMax, QString(msg).arg(DaysWithFileErrors)); 
                summaryMinMax->show();
            } else {
                summaryMinMax->hide();
            }
        }
        summaryProgress->show();
        summaryFound->show();
}

void    DailySearchTab::criteriaChanged() {
        // setup before start button

        if (valueMode != notUsed ) {
            setText(operationButton,opCodeStr(operationOpCode));
            operationButton->show();
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
            case secondsDisplayString:
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

        commandList->hide();
        commandButton->show();

        setText(startButton,tr("Start Search"));
        setColor(startButton,green);
        startButtonMode=true;
        startButton->setEnabled( true);

        hideResults(true);

        minMaxValid = false;
        minInteger = 0;
        maxInteger = 0;
        minDouble = 0.0;
        maxDouble = 0.0;
        earliestDate = p_profile->FirstDay(MT_CPAP);
        latestDate = p_profile->LastDay(MT_CPAP);
        daysTotal= 1+earliestDate.daysTo(latestDate);
        daysFound=0;
        daysSkipped=0;
        daysProcessed=0;
        startButtonMode=true;

        //initialize progress bar.

        progressBar->setMinimum(0);
        progressBar->setMaximum(daysTotal);
        progressBar->setTextVisible(true);
        progressBar->setMinimumHeight(commandListItemHeight);
        progressBar->setMaximumHeight(commandListItemHeight);
        progressBar->reset();
}

// inputs  character string.
// outputs cwa centered html string.
// converts \n to <br>
QString DailySearchTab::centerLine(QString line) {
        return line;
        return QString( "<center>%1</center>").arg(line).replace("\n","<br>");
}

QString DailySearchTab::helpStr() 
{
    QStringList str; str
    <<tr("Finds days that match specified criteria.") <<"\n"
    <<tr("  Searches from last day to first day.") <<"\n"
    <<tr("  Skips Days with no graphing data.") <<"\n"
    <<"\n"
    <<tr("First click on Match Button then select topic.") <<"\n"
    <<tr("  Then click on the operation to modify it.") <<"\n"
    <<tr("  or update the value") <<"\n"
    <<"\n"
    <<tr("Topics without operations will automatically start.") <<"\n" 
    <<"\n"
    <<tr("Compare Operations: numberic or character. ") <<"\n"
    <<tr("  Numberic  Operations: ") <<"   >  ,  >=  ,  <  ,  <=  ,  ==  ,  != " <<"\n"
    <<tr("  Character Operations: ") <<"   ==  ,  *? " <<"\n" 
    <<"\n"
    <<tr("Summary Line") <<"\n"
    <<tr("  Left:Summary - Number of Day searched") <<"\n"
    <<tr("  Center:Number of Items Found") <<"\n"
    <<tr("  Right:Minimum/Maximum for item searched") <<"\n"
    <<"\n"
    <<tr("Result Table") <<"\n"
    <<tr("  Column One: Date of match. Click selects date.") <<"\n"
    <<tr("  Column two: Information. Click selects date.") <<"\n"
    <<tr("    Then Jumps the appropiate tab.") <<"\n" 
    <<"\n"
    <<tr("Wildcard Pattern Matching:") <<" *? " <<"\n"
    <<tr("  Wildcards use 3 characters:") <<"\n"
    <<tr("  Asterisk") <<" *  " <<"   "
    <<tr("  Question Mark") <<" ? " <<"   "
    <<tr("  Backslash.") <<" \\ " <<"\n"
    <<tr("  Asterisk matches any number of characters.") <<"\n"
    <<tr("  Question Mark matches a single character.") <<"\n"
    <<tr("  Backslash matches next character.") <<"\n\n"
    ;
    return str.join("");
}

QString DailySearchTab::formatTime (qint32 ms) {
        qint32 hours = ms / 3600000;
        ms = ms % 3600000;
        qint32 minutes = ms / 60000;
        ms = ms % 60000;
        qint32 seconds = ms /1000;
        //return QString(tr("%1h %2m %3s")).arg(hours).arg(minutes).arg(seconds);
        return QString("%1:%2:%3").arg(hours).arg(minutes,2,10,QLatin1Char('0')).arg(seconds,2,10,QLatin1Char('0'));
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
            case OP_CONTAINS : return "==";    // or use 0x220B
            case OP_WILDCARD : return "*?";
            case OP_INVALID:
            case OP_END_NUMERIC:
            case OP_NO_PARMS:
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

void    DailySearchTab::on_activated(GPushButton* item ) {
        int row=item->row();
        int col=item->column();
        if (row<DS_ROW_DATA) return;
        if (row>=DS_ROW_MAX) return;
        row-=DS_ROW_DATA;

        item->setIcon (*m_icon_selected);
        daily->LoadDate( item->date() );

        if ((col!=0) &&  nextTab>=0 && nextTab < dailyTabWidget->count())  {
            dailyTabWidget->setCurrentIndex(nextTab);    // 0 = details ; 1=events =2 notes ; 3=bookarks;
        }
}

GPushButton::GPushButton (int row,int column,QDate date,DailySearchTab* parent , ChannelID code, quint32 offset) : 
    QPushButton(parent), _parent(parent), _row(row), _column(column), _date(date), _code(code) , _offset(offset)
{
        connect(this, SIGNAL(clicked()), this, SLOT(on_clicked()));
        connect(this, SIGNAL(activated(GPushButton*)), _parent, SLOT(on_activated(GPushButton*)));
};

GPushButton::~GPushButton()
{
        //the following disconnects trigger a crash during exit  or profile change. - anytime daily is destroyed.
        //disconnect(this, SIGNAL(clicked()), this, SLOT(on_clicked()));
        //disconnect(this, SIGNAL(activated(GPushButton*)), _parent, SLOT(on_activated(GPushButton*)));
};

void GPushButton::on_clicked() {
        emit activated(this);
};
