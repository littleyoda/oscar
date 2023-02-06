/* search  GUI Headers
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SEARCHDAILY_H
#define SEARCHDAILY_H

#include <QDate>
#include <QTextDocument>
#include <QList>
#include <QWidget>
#include <QTabWidget>
#include "SleepLib/common.h"

class QWidget ;
class QHBoxLayout ;
class QVBoxLayout ;
class QPushButton ;
class QLabel ;
class QComboBox ;
class QDoubleSpinBox ;
class QSpinBox ;
class QLineEdit ;
class QTableWidget ;
class QTableWidgetItem ;

class Day;  //forward declaration.
class Daily;  //forward declaration.

class DailySearchTab : public QWidget
{
	Q_OBJECT
public:
    DailySearchTab ( Daily* daily , QWidget* , QTabWidget* ) ;
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

    Daily*        daily;
    QWidget*      parent;
    QWidget*      searchTabWidget;
    QTabWidget*   dailyTabWidget;
    QVBoxLayout*  searchTabLayout;
    QHBoxLayout*  criteriaLayout;
    QHBoxLayout*  searchLayout;
    QHBoxLayout*  statusLayout;
    QHBoxLayout*  summaryLayout;
    QLabel*       criteriaOperation;
    QLabel*       introduction;
    QComboBox*    selectCommand;
    QLabel*       selectLabel;
    QLabel*       statusA;
    QLabel*       statusB;
    QLabel*       statusC;
    QLabel*       summaryStatsA;
    QLabel*       summaryStatsB;
    QLabel*       summaryStatsC;
    QDoubleSpinBox* enterDouble;
    QSpinBox*     enterInteger;
    QLineEdit*    enterString;
    QPushButton*  startButton;
    QPushButton*  continueButton;
    QTableWidget* guiDisplayTable;
    QIcon*        icon_on;
    QIcon*        icon_off;

    void        createUi();
    void        delayedCreateUi();

    void        search(QDate date, bool star);
    void        findall(QDate date, bool start);
    bool        find(QDate& , Day* day);
    EventDataType calculateAhi(Day* day);

    void        selectAligment(bool withParameters);
    void        displayStatistics();
    void        addItem(QDate date, QString value);
    void        criteriaChanged();
    void        endOfPass();
    QString     introductionStr();

    QString     centerLine(QString line);
    QString     formatTime (quint32) ;
    QString     convertRichText2Plain (QString rich);
    
    bool        createUiFinished=false;
    int         searchType;
    int         nextTab;

    QDate       firstDate ;
    QDate       lastDate ;
    QDate       nextDate;

    // 
    int         daysTotal;
    int         daysSkipped;
    int         daysSearched;
    int         daysFound;
    int         passFound;

    enum minMax {none=0,minDouble,maxDouble,minInteger,maxInteger};
    enum minMaxUnit {noUnit=0,time=1};
    bool        minMaxValid;
    minMaxUnit  minMaxUnit;
    minMax      minMaxMode;
    quint32     minMaxInteger;
    double      minMaxDouble;

    QTextDocument richText;

public slots:
private slots:
    void on_itemActivated(QTableWidgetItem *item);
    void on_startButton_clicked();
    void on_continueButton_clicked();
    void on_selectCommand_activated(int);
    void on_dailyTabWidgetCurrentChanged(int);
    void on_intValueChanged(int);
    void on_doubleValueChanged(double);
    void on_textEdited(QString);
};

#endif // SEARCHDAILY_H

