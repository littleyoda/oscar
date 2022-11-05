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

#define DEBUGQ  qDebug().noquote()
#define DEBUGL  DEBUGQ	<<QString("%1[%2]").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__)
#define DEBUGF  DEBUGQ	<<QString("%1[%2]%3").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)
#define DEBUGT  DEBUGQ	<<QString("%1 %2[%3]%4").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__)
#define DEBUGTF DEBUGQ	<<QString("%1 %2[%3]%4").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)

                                    // Do nothing
#define Z( EXPRESSION ) 			/* comment out display of variable */
#define ZZ( A ,EXPRESSION ) 		/* comment out display of variable */
                                    // Macros to display variables
#define O( EXPRESSION ) 			<<  EXPRESSION
#define Q( VALUE ) 			        <<  "" #VALUE ":" << VALUE
#define QQ( TEXT , EXPRESSION) 		<<  #TEXT ":" << EXPRESSION
//#define Q( VALUE ) 			        <<  QString("%1:%2").arg( ""  #VALUE).arg(VALUE)
//#define QQ( TEXT , EXPRESSION) 		<<  QString("%1:%2").arg( ""  #TEXT).arg(VALUE)

#define NAME( SCHEMACODE ) 			<<  schema::channel[ SCHEMACODE  ].label()
#define FULLNAME( SCHEMACODE ) 		<<  schema::channel[ SCHEMAcODE  ].fullname()

                                    //display the date of an epoch time stamp "qint64"
#define DATE( EPOCH ) 			<<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("dd MMM yyyy")
                                    //display the date and Time of an epoch time stamp "qint64"
#define DATETIME( EPOCH ) 		<<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("dd MMM yyyy hh:mm:ss.zzz")


#ifdef __clang__
    #define COMPILER O(QString("clang++:%1").arg(__clang_version__) );
#elif __GNUC_VERSION__
    #define COMPILER O(QString("GNUC++:%1").arg("GNUC").arg(__GNUC_VERSION__)) ;
#else
    #define COMPILER 
#endif



#if 0
//example:  DEBUGL;
//12361: Debug: "gGraphView[572]"

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

#define DEBUGQ 
#define DEBUGL
#define DEBUGF
#define DEBUGT
#define DEBUGTF

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

#endif
#endif

