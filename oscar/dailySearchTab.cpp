/* user graph settings Implementation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#define TEST_MACROS_ENABLEDoff
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
#include "dailySearchTab.h"
#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "daily.h"

enum DS_COL { DS_COL_LEFT=0, DS_COL_MID, DS_COL_RIGHT, DS_COL_MAX };
enum DS_ROW{ DS_ROW_CTL=0, DS_ROW_CMD, DS_ROW_LIST, DS_ROW_PROGRESS, DS_ROW_SUMMARY, DS_ROW_HEADER, DS_ROW_DATA };
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
|          Progress      | 
+========================+
|          Summary       |
+========================+
|          RESULTS       |
+========================+
*/


DailySearchTab::DailySearchTab(Daily* daily , QWidget* searchTabWidget  , QTabWidget* dailyTabWidget)  :
                        daily(daily) , parent(daily) , searchTabWidget(searchTabWidget) ,dailyTabWidget(dailyTabWidget)
{
    //DEBUGFW Q(this->font());
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

        controlTable     = new QTableWidget(DS_ROW_MAX,DS_COL_MAX,searchTabWidget);
        commandWidget    = new QWidget(this);
        commandLayout    = new QHBoxLayout();

        summaryWidget    = new QWidget(this);
        summaryLayout    = new QHBoxLayout();

        helpButton       = new QPushButton(this);
        helpText         = new QTextEdit(this);
        matchButton      = new QPushButton(this);
        clearButton      = new QPushButton(this);

        commandList      = new QListWidget(controlTable);
        commandButton    = new QPushButton(commandWidget);

        operationCombo   = new QComboBox(commandWidget);
        operationButton  = new QPushButton(commandWidget);
        selectDouble     = new QDoubleSpinBox(commandWidget);
        selectInteger    = new QSpinBox(commandWidget);
        selectString     = new QLineEdit(commandWidget);
        selectUnits      = new QLabel(commandWidget);

        startButton      = new QPushButton(this);
        statusProgress   = new QLabel(this);
        summaryProgress  = new QLabel(this);
        summaryFound     = new QLabel(this);
        summaryMinMax    = new QLabel(this);
        progressBar      = new QProgressBar(this);

        populateControl();

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


        controlTable->setCellWidget(DS_ROW_CTL,DS_COL_LEFT,matchButton);
        controlTable->setCellWidget(DS_ROW_CTL,DS_COL_MID,clearButton);
        controlTable->setCellWidget(DS_ROW_CTL,DS_COL_RIGHT,startButton);

        controlTable->setCellWidget(DS_ROW_LIST,DS_COL_LEFT,commandList);
        controlTable->setCellWidget(DS_ROW_CMD,DS_COL_LEFT,commandWidget);

        controlTable->setCellWidget( DS_ROW_SUMMARY , 0 ,summaryWidget);
        controlTable->setCellWidget( DS_ROW_PROGRESS , 0 , progressBar);
        
        controlTable->setRowHeight(DS_ROW_LIST,commandList->size().height());
        //controlTable->setRowHeight(DS_ROW_PROGRESS,progressBar->size().height());

        controlTable->setSpan( DS_ROW_CMD ,DS_COL_LEFT,1,3);
        controlTable->setSpan( DS_ROW_LIST ,DS_COL_LEFT,1,3);
        controlTable->setSpan( DS_ROW_SUMMARY ,DS_COL_LEFT,1,3);
        controlTable->setSpan( DS_ROW_PROGRESS ,DS_COL_LEFT,1,3);
        for (int index = DS_ROW_HEADER; index<DS_ROW_MAX;index++) {
            controlTable->setSpan(index,DS_COL_MID,1,2);
        }

        searchTabLayout ->addWidget(helpButton);
        searchTabLayout ->addWidget(helpText);
        searchTabLayout ->addWidget(controlTable);

        // End of UI creatation
        //Setup each BUtton / control item

        QString styleButton=QString(
            "QPushButton { color: black; border: 1px solid black; padding: 5px ;background-color:white; }"
            "QPushButton:disabled { color: #333333; border: 1px solid #333333; background-color: #dddddd;}"
            );

        helpText->setReadOnly(true);
        helpText->setLineWrapMode(QTextEdit::NoWrap);
        QSize size = QFontMetrics(this->font()).size(0, helpString);
        size.rheight() += 35 ; // scrollbar 
        size.rwidth()  += 35 ; // scrollbar 
        helpText->setText(helpString);
        helpText->setMinimumSize(textsize(this->font(),helpString));
        helpText->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

        helpButton->setStyleSheet( styleButton );
        // helpButton->setText(tr("Help"));
        helpMode = true;
        on_helpButton_clicked();

        matchButton->setIcon(*m_icon_configure);
        matchButton->setStyleSheet( styleButton );
        setText(matchButton,tr("Match"));

        clearButton->setStyleSheet( styleButton );
        setText(clearButton,tr("Clear"));

        startButton->setStyleSheet( styleButton );
        //setText(startButton,tr("Start Search"));
        //startButton->setEnabled(false);

        //setText(commandButton,(tr("Select Match")));

        //float height = float(1+commandList->count())*commandListItemHeight ;
        float height = float(commandList->count())*commandListItemHeight ;
        commandList->setMinimumHeight(height);
        commandList->setMinimumWidth(commandListItemMaxWidth);
        setCommandPopupEnabled(false);

        setText(operationButton,"");
        operationButton->setStyleSheet("border:none;");
        operationButton->hide();
        operationCombo->hide();
        operationCombo->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        setOperationPopupEnabled(false);

        selectDouble->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectInteger->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectString->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectUnits->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        setText(selectUnits,"");

        commandButton->setStyleSheet("border: 1px solid black; padding: 5px ;");
        operationButton->setStyleSheet("border: 1px solid black; padding: 5px ;");
        selectUnits->setStyleSheet("border: 1px solid white; padding: 5px ;");
        selectDouble->setButtonSymbols(QAbstractSpinBox::NoButtons);
        selectInteger->setButtonSymbols(QAbstractSpinBox::NoButtons);
        selectDouble->setStyleSheet("border: 1px solid black; padding: 5px ;");
        // clears arrows on spinbox selectDouble->setStyleSheet("border: 1px solid black; padding: 5px ;");
        // clears arrows on spinbox selectInteger->setStyleSheet("border: 1px solid black; padding: 5px ;");
        commandWidget->setStyleSheet("border: 1px solid black; padding: 5px ;");

        progressBar->setValue(0);
        //progressBar->setStyleSheet("border: 0px solid black; padding: 0px ;");
        //progressBar->setStyleSheet("color: black; background-color #666666 ;");

        progressBar->setStyleSheet(
            "QProgressBar{border: 1px solid black; text-align: center;}"
            "QProgressBar::chunk { border: none; background-color: #ccddFF; } ");
        summaryProgress->setStyleSheet("padding:5px;background-color: #ffffff;" );
        summaryFound->setStyleSheet("padding:5px;background-color: #f0f0f0;" );
        summaryMinMax->setStyleSheet("padding:5px;background-color: #ffffff;" );

        controlTable->horizontalHeader()->hide();   // hides numbers above each column
        //controlTable->verticalHeader()->hide();   // hides numbers before each row.
        controlTable->horizontalHeader()->setStretchLastSection(true);
        // get rid of selection coloring
        controlTable->setStyleSheet("QTableView{background-color: white; selection-background-color: white; }");

        float width = 14/*styleSheet:padding+border*/ + QFontMetrics(this->font()).size(Qt::TextSingleLine ,"WWW MMM 99 2222").width();
        controlTable->setColumnWidth(DS_COL_LEFT, width);

        width = 30/*iconWidthPlus*/+QFontMetrics(this->font()).size(Qt::TextSingleLine ,clearButton->text()).width();
        //width = clearButton->width();
        controlTable->setColumnWidth(DS_COL_MID, width);
        controlTable->setShowGrid(false);

        searchTabLayout->setContentsMargins(4, 4, 4, 4);

        //DEBUGFW Q(QWidget::logicalDpiX()) Q(QWidget::logicalDpiY()) Q(QWidget::physicalDpiX()) Q(QWidget::physicalDpiY());
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
    //DEBUGFW Q(scaleX) Q(percentX) Q(width) Q(width*percentX);
    commandListItemMaxWidth = max (commandListItemMaxWidth, (width*percentX));
    commandListItemHeight = QFontMetricsF(this->font()).size(Qt::TextSingleLine , str).height();
    QListWidgetItem* item = new QListWidgetItem(str);
    item->setData(Qt::UserRole,topic);
    return item;
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
        if ( !AppSetting->complianceMode() ) {
            commandList->addItem(calculateMaxSize(tr("Disabled Sessions"),ST_DISABLED_SESSIONS));
        }
        commandList->addItem(calculateMaxSize(tr("Number of Sessions"),ST_SESSIONS_QTY));
        //commandList->insertSeparator(commandList->count());  // separate from events

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
            commandList->addItem(calculateMaxSize(displayName,id));
        }

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

}


void    DailySearchTab::on_helpButton_clicked() {
        helpMode = !helpMode;
        if (helpMode) {
            //DEBUGFW Q(textsize(helpText->font(),helpString)) Q(controlTable->size());
            controlTable->setMinimumSize(QSize(50,200)+textsize(helpText->font(),helpString));
            controlTable->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::MinimumExpanding);
            //setText(helpButton,tr("Click HERE to close Help"));
            helpButton->setText(tr("Click HERE to close Help"));
            helpText ->show();
        } else {
            controlTable->setMinimumWidth(250);
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
                break;
            case ST_SESSION_LENGTH :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Session Duration\nJumps to Date's Details"));
                nextTab = TW_DETAILED ;
                setoperation(OP_LT,minutesToMs);
                setText(selectUnits,tr(" Minutes"));
                selectDouble->setValue(5.0);
                selectInteger->setValue((int)selectDouble->value()*60000.0);   //convert to ms
                break;
            case ST_SESSIONS_QTY :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Number of Sessions\nJumps to Date's Details"));
                nextTab = TW_DETAILED ;
                setoperation(OP_GT,opWhole);
                setText(selectUnits,tr(" Sessions"));
                selectInteger->setRange(0,999);
                selectInteger->setValue(2);
                break;
            case ST_DAILY_USAGE :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Daily Duration\nJumps to Date's Details"));
                nextTab = TW_DETAILED ;
                setoperation(OP_LT,hoursToMs);
                setText(selectUnits,tr(" Hours"));
                selectDouble->setValue(p_profile->cpap->complianceHours());
                selectInteger->setValue((int)selectDouble->value()*3600000.0);   //convert to ms
                break;
            case ST_EVENT:
                // Have an Event
                setResult(DS_ROW_HEADER,1,QDate(),tr("Number of events\nJumps to Date's Events"));
                nextTab = TW_EVENTS ;
                setoperation(OP_GT,opWhole);
                setText(selectUnits,tr(" Events"));
                selectInteger->setValue(0);
                break;
        }
        criteriaChanged();
        if (operationOpCode == OP_NO_PARMS ) {
            operationButton->hide();
            operationCombo->hide();
            // auto start searching
            setText(startButton,tr("Automatic start"));
            startButtonMode=true;
            on_startButton_clicked();
            return;
        }
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

    QWidget* header = controlTable->cellWidget(row,column);
    GPushButton* item;
    if (header == nullptr) {
        item = new GPushButton(row,column,date,this);
        //item->setStyleSheet("QPushButton {text-align: left;vertical-align:top;}");
        item->setStyleSheet(
            "QPushButton { text-align: left;color: black; border: 1px solid black; padding: 5px ;background-color:white; }" );
        controlTable->setCellWidget(row,column,item);
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
        controlTable->setRowHeight(DS_ROW_HEADER,8/*margins*/+size.height());
    } else {
        item->setIcon(*m_icon_notSelected);
        if (column == 0) {
            setText(item,date.toString());
        } else {
            setText(item,text);
        }
    }
    if ( row == DS_ROW_DATA )  {
        controlTable->setRowHidden(DS_ROW_HEADER,false);
    }
    controlTable->setRowHidden(row,false);
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


void DailySearchTab::find(QDate& date)
{
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

void DailySearchTab::search(QDate date)
{
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
        if ((passFound >= passDisplayLimit) && (daysProcessed<daysTotal)) {
            //setText(statusProgress,centerLine(tr("More to Search")));
            //statusProgress->show();
            startButton->setEnabled(true);
            setText(startButton,(tr("Continue Search")));
        } else if (daysFound>0) {
            //setText(statusProgress,centerLine(tr("End of Search")));
            //statusProgress->show();
            startButton->setEnabled(false);
            setText(startButton,tr("End of Search"));
        } else {
            //setText(statusProgress,centerLine(tr("No Matches")));
            //statusProgress->show();
            startButton->setEnabled(false);
            //setText(startButton,(tr("End of Search")));
            setText(startButton,tr("No Matches"));
        }

        displayStatistics();
}


void    DailySearchTab::setCommandPopupEnabled(bool on) {
        DEBUGFW;
        if (on) {
            commandPopupEnabled=true;
            controlTable->setRowHidden(DS_ROW_CMD,true);
            controlTable->setRowHidden(DS_ROW_LIST,false);
            hideResults(true);
        } else {
            commandPopupEnabled=false;
            controlTable->setRowHidden(DS_ROW_LIST,true);
            controlTable->setRowHidden(DS_ROW_CMD,false);
        }
}

void    DailySearchTab::on_operationButton_clicked() {
        DEBUGFW;
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
        DEBUGFW;
        setCommandPopupEnabled(!commandPopupEnabled);
}

void    DailySearchTab::on_commandButton_clicked()
{
        DEBUGFW;
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
        }
        switch (valueMode) {
            case hundredths :
                selectUnits->show();
                selectDouble->show();
            break;
            case hoursToMs:
                selectUnits->show();
                selectDouble->show();
                break;
            case minutesToMs:
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
        controlTable->setRowHidden(DS_ROW_SUMMARY,hide);
        controlTable->setRowHidden(DS_ROW_PROGRESS,hide);
        if (hide) {
            for (int index = DS_ROW_HEADER; index<DS_ROW_MAX;index++) {
                controlTable->setRowHidden(index,true);
            }
        }
}

QSize   DailySearchTab::textsize(QFont font ,QString text) {
        return QFontMetrics(font).size(0 , text);
}

void    DailySearchTab::on_clearButton_clicked()
{
        DEBUGFW;
        searchTopic = ST_NONE;
        // make these button text back to start.
        startButton->setText(tr("Start Search"));
        startButtonMode=true;
        startButton->setEnabled( false);

        // hide widgets
        //Reset Select area
        commandList->hide();
        setCommandPopupEnabled(false);
        setText(commandButton,(tr("Select Match")));
        commandButton->show();

        operationCombo->hide();
        setOperationPopupEnabled(false);
        operationCombo->hide();
        operationButton->hide();
        selectDouble->hide();
        selectInteger->hide();
        selectString->hide();
        selectUnits->hide();

        hideResults(true);

        // show these widgets;
        //controlWidget->show();
}

void    DailySearchTab::on_startButton_clicked()
{
        DEBUGFW;
        hideResults(false);
        startButton->setEnabled(false);
        setText(startButton,tr("Searchng"));
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

void    DailySearchTab::displayStatistics() {
        QString extra;
        summaryProgress->show();

        // display days searched
        QString skip= daysSkipped==0?"":QString(tr(" Skip:%1")).arg(daysSkipped);
        setText(summaryProgress,centerLine(QString(tr("%1/%2%3 days.")).arg(daysProcessed).arg(daysTotal).arg(skip) ));

        // display days found
        setText(summaryFound,centerLine(QString(tr("Found %1.")).arg(daysFound) ));

        // display associated value
        extra ="";
        if (minMaxValid) {
            extra = QString("%1 / %2").arg(valueToString(minInteger)).arg(valueToString(maxInteger));
        }
        if (extra.size()>0) {
            setText(summaryMinMax,extra);
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
        startButtonMode=true;
        startButton->setEnabled( true);

        setText(statusProgress,centerLine(" ----- "));
        statusProgress->clear();
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
        //progressBar->setMinimumHeight(commandListItemHeight);
        //progressBar->setMaximumHeight(commandListItemHeight);
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
    QStringList str;
    str.append(tr("Finds days that match specified criteria."));
    str.append("\n");
    str.append(tr("  Searches from last day to first day."));
    str.append("\n");
    str.append("\n");
    str.append(tr("First click on Match Button then select topic."));
    str.append("\n");
    str.append(tr("  Then click on the operation to modify it."));
    str.append("\n");
    str.append(tr("  or update the value"));
    str.append("\n");
    str.append(tr("Topics without operations will automatically start."));
    str.append("\n");
    str.append("\n");
    str.append(tr("Compare Operations: numberic or character. "));
    str.append("\n");
    str.append(tr("  Numberic  Operations: "));
    str.append("   >  ,  >=  ,  <  ,  <=  ,  ==  ,  != ");
    str.append("\n");
    str.append(tr("  Character Operations: "));
    str.append("   ==  ,  *? ");
    str.append("\n");
    str.append("\n");
    str.append(tr("Summary Line"));
    str.append("\n");
    str.append(tr("  Left:Summary - Number of Day searched"));
    str.append("\n");
    str.append(tr("  Center:Number of Items Found"));
    str.append("\n");
    str.append(tr("  Right:Minimum/Maximum for item searched"));
    str.append("\n");
    str.append(tr("Result Table"));
    str.append("\n");
    str.append(tr("  Column One: Date of match. Click selects date."));
    str.append("\n");
    str.append(tr("  Column two: Information. Click selects date."));
    str.append("\n");
    str.append(tr("    Then Jumps the appropiate tab."));
    str.append("\n");
    str.append("\n");
    str.append(tr("Wildcard Pattern Matching:"));
    str.append(" *? ");
    str.append("\n");
    str.append(tr("  Wildcards use 3 characters:"));
    str.append("\n");
    str.append(tr("  Asterisk"));
    str.append(" *  ");
    str.append("   ");
    str.append(tr(" Question Mark"));
    str.append(" ? ");
    str.append("   ");
    str.append(tr(" Backslash."));
    str.append(" \\ ");
    str.append("\n");
    str.append(tr("  Asterisk matches any number of characters."));
    str.append("\n");
    str.append(tr("  Question Mark matches a single character."));
    str.append("\n");
    str.append(tr("  Backslash matches next character."));
    str.append("\n");
    QString result =str.join("");
    return result;
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
            case OP_CONTAINS : return QChar(0x2208);    // or use 0x220B
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
        // DEBUGFW Q(row) Q(item->column()) Q(item->date()) Q(item->text());
        if (row<DS_ROW_DATA) return;
        if (row>=DS_ROW_MAX) return;
        row-=DS_ROW_DATA;

        item->setIcon (*m_icon_selected);
        daily->LoadDate( item->date() );
        if ((col!=0) &&  nextTab>=0 && nextTab < dailyTabWidget->count())  {
            dailyTabWidget->setCurrentIndex(nextTab);    // 0 = details ; 1=events =2 notes ; 3=bookarks;
        }
}

GPushButton::GPushButton (int row,int column,QDate date,DailySearchTab* parent) : QPushButton(parent), _parent(parent), _row(row), _column(column), _date(date)
{
        connect(this, SIGNAL(clicked()), this, SLOT(on_clicked()));
        connect(this, SIGNAL(activated(GPushButton*)), _parent, SLOT(on_activated(GPushButton*)));
};

GPushButton::~GPushButton()
{
        //these disconnects trigger a crash during exit  or profile change. - anytime daily is destroyed.
        //disconnect(this, SIGNAL(clicked()), this, SLOT(on_clicked()));
        //disconnect(this, SIGNAL(activated(GPushButton*)), _parent, SLOT(on_activated(GPushButton*)));
};
void GPushButton::on_clicked() {
        emit activated(this);
};
