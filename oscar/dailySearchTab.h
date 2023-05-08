/* search  GUI Headers
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SEARCHDAILY_H
#define SEARCHDAILY_H

#include <QDate>
#include <QTextDocument>
#include <QListWidgetItem>
#include <QList>
#include <QWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QMap>
#include <QTextEdit>
#include "SleepLib/common.h"

class GPushButton;
class QWidget ;
class QDialog ;
class QComboBox ;
class QListWidget ;
class QProgressBar ;
class QHBoxLayout ;
class QVBoxLayout ;
class QLabel ;
class QDoubleSpinBox ;
class QSpinBox ;
class QLineEdit ;
class QTableWidget ;
class QTableWidgetItem ;
class QSizeF ;

class Day;  //forward declaration.
class Daily;  //forward declaration.

class DailySearchTab : public QWidget
{
	Q_OBJECT
public:
    DailySearchTab ( Daily* daily , QWidget* ,  QTabWidget* ) ;
    ~DailySearchTab();

private:

    // these values are hard coded. because dynamic translation might not do the proper assignment.
    // Dynamic code is commented out using c preprocess #if #endif
    const int          TW_NONE       = -1;
    const int          TW_DETAILED   =  0;
    const int          TW_EVENTS     =  1;
    const int          TW_NOTES      =  2;
    const int          TW_BOOKMARK   =  3;
    const int          TW_SEARCH     =  4;

    const int     dateRole = Qt::UserRole;
    const int     valueRole = 1+Qt::UserRole;
    const int     passDisplayLimit = 30;
    const int     stringDisplayLen = 80;

enum ValueMode { invalidValueMode , notUsed , minutesToMs  , hoursToMs , hundredths , opWhole , displayWhole , opString , displayString};

enum SearchTopic { ST_NONE = 0 , ST_DAYS_SKIPPED = 1 , ST_DISABLED_SESSIONS = 2 , ST_NOTES = 3 , ST_NOTES_STRING , ST_BOOKMARKS , ST_BOOKMARKS_STRING , ST_AHI , ST_SESSION_LENGTH , ST_SESSIONS_QTY , ST_DAILY_USAGE, ST_EVENT };

enum OpCode {
    //DO NOT CHANGE NUMERIC OP CODES because THESE VALUES impact compare operations.
    // start of fixed codes
    OP_INVALID , OP_LT , OP_GT , OP_NE , OP_EQ , OP_LE , OP_GE , OP_END_NUMERIC ,
    // end of fixed codes
    OP_CONTAINS , OP_WILDCARD , OP_NO_PARMS };


    Daily*        daily;
    QWidget*      parent;
    QWidget*      searchTabWidget;
    QTabWidget*   dailyTabWidget;
    QVBoxLayout*  searchTabLayout;

    QTableWidget* controlTable;

    // Command command Widget
    QWidget*      commandWidget;
    QHBoxLayout*  commandLayout;

    QPushButton*  helpButton;
    QTextEdit*    helpText;

    QProgressBar* guiProgressBar;

    // control Widget
    QPushButton*  matchButton;
    QPushButton*  clearButton;

    QWidget*      summaryWidget;
    QHBoxLayout*  summaryLayout;

    // Command Widget
    QListWidget*  commandList;
    // command Widget
    QPushButton*  commandButton;
    QComboBox*    operationCombo;
    QPushButton*  operationButton;
    QLabel*       selectUnits;
    QDoubleSpinBox* selectDouble;
    QSpinBox*     selectInteger;
    QLineEdit*    selectString;

    // Trigger  Widget
    QPushButton*  startButton;
    QLabel*       statusProgress;

    QLabel*       summaryProgress;
    QLabel*       summaryFound;
    QLabel*       summaryMinMax;

    QIcon*        m_icon_selected;
    QIcon*        m_icon_notSelected;
    QIcon*        m_icon_configure;

    QMap <QString,OpCode> opCodeMap;
    QString     opCodeStr(OpCode);
    OpCode      operationOpCode = OP_INVALID;


    bool        helpMode=false;
    QString     helpString = helpStr();
    void        createUi();
    void        populateControl();
    QSize       setText(QPushButton*,QString);
    QSize       setText(QLabel*,QString);
    QSize       textsize(QFont font ,QString text);

    void        search(QDate date);
    void        find(QDate&);
    void        criteriaChanged();
    void        endOfPass();
    void        displayStatistics();
    void        setResult(int row,int column,QDate date,QString value);


    void        addItem(QDate date, QString value, Qt::Alignment alignment);
    void        setCommandPopupEnabled(bool );
    void        setOperationPopupEnabled(bool );
    void        setOperation( );
    void        hideResults(bool);
    void        connectUi(bool);


    QString     helpStr();
    QString     centerLine(QString line);
    QString     formatTime (qint32) ;
    QString     convertRichText2Plain (QString rich);
    QRegExp     searchPatterToRegex (QString wildcard);
    QListWidgetItem*     calculateMaxSize(QString str,int topic);
    float       commandListItemMaxWidth = 0;
    float       commandListItemHeight = 0;

    EventDataType calculateAhi(Day* day);
    bool        compare(int,int );
    bool        compare(QString aa , QString bb);

    bool        createUiFinished=false;
    bool        startButtonMode=true;
    bool        commandPopupEnabled=false;
    SearchTopic searchTopic;
    int         nextTab;
    int         channelId;

    QDate       earliestDate ;
    QDate       latestDate ;
    QDate       nextDate;

    //
    int         daysTotal;
    int         daysSkipped;
    int         daysProcessed;
    int         daysFound;
    int         passFound;

    void        setoperation(OpCode opCode,ValueMode mode) ;

    ValueMode   valueMode;
    qint32     selectValue=0;

    bool        minMaxValid;
    qint32     minInteger;
    qint32     maxInteger;
    void        updateValues(qint32);

    QString     valueToString(int value, QString empty = "");
    qint32     foundValue;
    QString     foundString;

    double      maxDouble;
    double      minDouble;

    QTextDocument richText;


public slots:
private slots:
    void on_startButton_clicked();
    void on_clearButton_clicked();
    void on_matchButton_clicked();
    void on_helpButton_clicked();

    void on_commandButton_clicked();
    void on_operationButton_clicked();

    void on_commandList_activated(QListWidgetItem* item);
    void on_operationCombo_activated(int index);

    void on_intValueChanged(int);
    void on_doubleValueChanged(double);
    void on_textEdited(QString);

    void on_activated(GPushButton*);
};


class GPushButton : public QPushButton
{
	Q_OBJECT
public:
    GPushButton (int,int,QDate,DailySearchTab* parent); 
    virtual ~GPushButton(); 
    int row() { return _row;};
    int column() { return _column;};
    QDate date() { return _date;};
    void setDate(QDate date) {_date=date;};
private:
    const DailySearchTab* _parent;
    const int _row;
    const int _column;
    QDate _date;
signals:
    void activated(GPushButton*);
public slots:
    void on_clicked();
};

#endif // SEARCHDAILY_H

