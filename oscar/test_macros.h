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

#define DEBUGL  qDebug()	<<QString("%1[%2]").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__)
//example:  
//12361: Debug: "gGraphView[572]"


#define DEBUGF  qDebug()	<<QString("%1[%2]%3").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)
//example:  
//12361: Debug: "gGraphView[572]popoutGraph"


#define DEBUGT  qDebug()	<<QString("%1 %2[%3]%4").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__)
//example:  
//12645: Debug: "06:00:18.284 gGraphView[622]" 

#define DEBUGTF qDebug()	<<QString("%1 %2[%3]%4").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)
//example:  
//12645: Debug: "06:00:18.284 gGraphView[622]popoutGraph" 

// Macros to display variables
#define O( EXPRESSION ) 			<<  EXPRESSION

// return values from functions donr't work - QQ macro instead
#define Q( VALUE ) 			<<  "" #VALUE ":" << VALUE
//example:

// 
#define QQ( TEXT , EXPRESSION) 		<<  #TEXT ":" << EXPRESSION
//example:

#define NAME( SCHEMECODE ) 			<<  schema::channel[ SCHEMECODE  ].label()
//example:

#define FULLNAME( SCHEMECODE ) 		<<  schema::channel[ SCHEMEcODE  ].fullname()
//example:

//display the date of an epoch time stamp "qint64"
#define DATE( EPOCH ) 			<<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("dd MMM yyyy")

//display the date and Time of an epoch time stamp "qint64"
#define DATETIME( EPOCH ) 		<<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("dd MMM yyyy hh:mm:ss.zzz")

/*
sample Lines.

code
DEBUGF Q(name) Q(title) QQ("UNITS",units) Q(height) Q(group);
output
00791: Debug: "gGraph[137]gGraph" name: "RespRate" title: "Respiratory Rate" "UNITS": "Rate of breaths per minute" height: 180 group: 0


DEBUGTF Q(newname);
12645: Debug: "06:00:18.284 gGraphView[622]popoutGraph" newname: "Pressure - Friday, April 15, 2022"


DEBUGF NAME(dot.code) Q(dot.type) QQ(y,(int)y) Q(ratioX) O(QLine(left + 1, y, left + 1 + width, y)) Q(legendx) O(dot.value) ;
92 00917: Debug: "gLineChart[568]paint" "Pressure" dot.type: 4 y: 341 ratioX: 1 QLine(QPoint(91,341),QPoint(464,341)) legendx: 463 12.04


*/

#else
// Turn debugging off.  macros expands to white space

#define DEBUGL
#define DEBUGF
#define DEBUGT
#define DEBUGTF

#define O( XX )
#define Q( XX )
#define QQ( XX , YY )
#define NAME( id)
#define FULLNAME( id)
#define DATE( XX )
#define DATETIME( XX )

#endif
#endif

