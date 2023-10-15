/* Test macros Implemntation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

/*
These functions will display formatted debug information.
The macro TEST_MACROS_ENABLED will enable these macros to display information
When The macro TEST_MACROS_ENABLED is undefined then these marcos will expand to white space.

When these macos are used then debugging is disabled
When only these macos are used then debugging is disabled.
SO for production code rename TEST_MACROS_ENABLED to TEST_MACROS_ENABLEDoff

###########################################
The the following to source cpp files
to turn on the debug macros for use.

#define TEST_MACROS_ENABLED
#include <test_macros.h>

To turn off the the test macros.
#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>
###########################################


*/

#ifndef TEST_MACROS_ENABLED_ONCE
#define TEST_MACROS_ENABLED_ONCE

#ifdef TEST_MACROS_ENABLED
#include <QRegularExpression>
#include <QFileInfo>

#define DEBUGI   qInfo().noquote() << "INFO;"
#define DEBUGD   qInfo().noquote() << "DEBUG;"
#define DEBUGQ   qDebug().noquote()
#define DEBUGW   qWarning().noquote()
#define DEBUGC   qCritical().noquote()
#define DEBUGF   DEBUGI	<<QString("%1[%2]%3").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)
#define DEBUGFW  DEBUGW	<<QString("%1[%2]%3").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)
#define DEBUGFC  DEBUGC <<QString("%1[%2]%3").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)
#define DEBUGFD  DEBUGD <<QString("%1[%2]%3").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)
#define DEBUGFI  DEBUGI <<QString("%1[%2]%3").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)


#define DEBUGT   DEBUGQ	<<QString("%1 %2[%3]%4").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__)
#define DEBUGTF  DEBUGQ	<<QString("%1 %2[%3]%4").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)
#define DEBUGTFW  DEBUGW	<<QString("%1 %2[%3]%4").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)

                                    // Do nothing
#define Z( EXPRESSION ) 			/* comment out display of variable */
#define ZZ( A ,EXPRESSION ) 		/* comment out display of variable */
                                    // Macros to display variables
#define O( EXPRESSION ) 			<<  EXPRESSION
#define Q( VALUE ) 			        <<  "" #VALUE ":" << VALUE
#define QQ( TEXT , EXPRESSION) 		<<  #TEXT ":" << EXPRESSION
//#define Q( VALUE ) 			        <<  QString("%1:%2").arg( ""  #VALUE).arg(VALUE)
//#define QQ( TEXT , EXPRESSION) 		<<  QString("%1:%2").arg( ""  #TEXT).arg(VALUE)

//#define NAME( SCHEMACODE ) 			<<  schema::channel[ SCHEMACODE  ].label()
#define NAME( SCHEMACODE )           <<  QString(("%1:0x%2")).arg(schema::channel[ SCHEMACODE  ].label()).arg( QString::number(SCHEMACODE,16).toUpper() )
#define FULLNAME( SCHEMACODE ) 		 <<  schema::channel[ SCHEMACODE  ].fullname()


                                    //display the date of an epoch time stamp "qint64"
#define DATE( EPOCH ) 			<<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("dd MMM yyyy")
                                    //display the date and Time of an epoch time stamp "qint64"
#define DATETIME( EPOCH ) 		<<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("dd MMM yyyy hh:mm:ss.zzz")
#define IF( EXPRESSION )    if (EXPRESSION )
#define IFD( EXPA , EXPB )   bool EXPA = EXPB


#ifdef __clang__
    #define COMPILER O(QString("clang++:%1").arg(__clang_version__) );
#elif __GNUC_VERSION__
    #define COMPILER O(QString("GNUC++:%1").arg("GNUC").arg(__GNUC_VERSION__)) ;
#else
    #define COMPILER
#endif



#if 0
//example:  DEBUGF;
//12361: Debug: "gGraphView[572]popoutGraph"

//example:  DEBUGT;
//12645: Debug: "06:00:18.284 gGraphView[622]"

//example:  DEBUGTF;
//12645: Debug: "06:00:18.284 gGraphView[622]popoutGraph"

//example:  DEBUGF Q(name) Q(title) QQ("UNITS",units) Q(height) Q(group);
//00791: Debug: "gGraph[137]gGraph" name: "RespRate" title: "Respiratory Rate" "UNITS": "Rate of breaths per minute" height: 180 group: 0

//example:  DEBUGTF Q(newname);
//12645: Debug: "06:00:18.284 gGraphView[622]popoutGraph" newname:"Pressure - Friday, April 15, 2022"

//example:  DEBUGF NAME(dot.code) Q(dot.type) QQ(y,(int)y) Q(ratioX) O(QLine(left + 1, y, left + 1 + width, y)) Q(legendx) O(dot.value) ;
//92 00917: Debug: "gLineChart[568]paint" "Pressure" dot.type: 4 y: 341 ratioX: 1 QLine(QPoint(91,341),QPoint(464,341)) legendx: 463 12.04
#endif

#else
// Turn debugging off.  macros expands to white space

#define DEBUGI
#define DEBUGD
#define DEBUGQ
#define DEBUGW
#define DEBUGC
#define DEBUGF
#define DEBUGFW
#define DEBUGFC
#define DEBUGFD
#define DEBUGFI
#define DEBUGT
#define DEBUGTF
#define DEBUGTFW

#define Z( XX )
#define ZZ( XX , YY)
#define O( XX )
#define Q( XX )
#define QQ( XX , YY )
#define NAME( id)
#define FULLNAME( id)
#define DATE( XX )
#define DATETIME( XX )
#define COMPILER
#define IF( XX )
#define IFD( XX , YY )

#endif  // TEST_MACROS_ENABLED
#ifdef TEST_ROUTIMES_ENABLED
This is prototype 
class testRoutines : public QWidget
{
Q_OBJECT     
public:
typedef enum {debugDisplay = 0, returnQTextEdit} findClassMode;
QWidget* findQTextEditdisplaywidgets(QWidget* widget,findClassMode mode=findClassMode::debugDisplay,QString name="",int recurseCount=0) {
         return findQTextEditdisplaywidgets(widget,findClassMode::debugDisplay,"",0);
}
private:
QWidget* findQTextEditdisplaywidgets(QWidget* widget,findClassMode mode,QString objectName,int recurseCount) {
        QString indent;
        if (!widget) return nullptr;
        if (mode==debugDisplay) {
            if (recurseCount==0) {
                DEBUGF O("===================================================");
            }
            indent = QString("%1").arg("",(recurseCount*4),QLatin1Char(' '));
        }
        recurseCount++;
        if (recurseCount>10) return nullptr;
        if (mode==debugDisplay) {
            DEBUGF O(indent) O(widget) ;
        } else if (mode==returnQTextEdit){
            if(QTextEdit* te = dynamic_cast<QTextEdit*>(widget)) {
                return widget ;
            };
        }
        const QList<QObject*> list = widget->children();
        for (int i = 0; i < list.size(); ++i) {
            QWidget *next_widget = dynamic_cast<QWidget*>(list.at(i));
            if (!next_widget) continue;
            QWidget* found=findQTextEditdisplaywidgets(next_widget,mode,objectName,recurseCount);
            if (found) return found;
        }
        if (mode==debugDisplay && recurseCount==1) DEBUGF O("===================================================");
        return nullptr;
    }
}
#endif  // TEST_ROUTIMES_ENABLED

#endif  // TEST_MACROS_ENABLED_ONCE

