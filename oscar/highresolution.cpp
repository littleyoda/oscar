/* Hi Resolution Implementation
 *
 * Copyright (c) 2023 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QMessageBox>
#include <QDebug>
#include <QCheckBox>
#include <QFileDialog>
#include "SleepLib/profiles.h"
#include "highresolution.h"


namespace HighResolution {

    const QString HI_RES_FILENAME("hiResolutionMode.txt");
    HiResolutionMode hiResolutionNextSessionMode = HRM_INVALID;
    HiResolutionMode hiResolutionOperaingMode = HRM_INVALID;

    int readMode( QString filename) {
        QFile file ( filename );
        int mode=0;
        if (file.open(QFile::ReadOnly)) {
            QTextStream in(&file);
            in >> mode;
            file.close();
        }
        return mode;
    }

    void writeMode( QString filename, int data , QString description) {
        QFile file ( filename );
        if (file.open(QFile::WriteOnly|QFile::Text)) {
            QTextStream out(&file);
            out << data << " " << description << "\n" ;
            file.close();
        }
    }

    HiResolutionMode setHiResolutionMode(HiResolutionMode value) {
        QString filename = GetAppData() + HI_RES_FILENAME;
        if (value == HRM_ENABLED ) {
            writeMode( filename , HRM_ENABLED ,"HiResolutionMode Enabled");
            return HRM_ENABLED;
        } else {
            writeMode( filename , HRM_DISABLED , "HiResolutionMode Disabled");
        }
        return HRM_DISABLED;
    }

    HiResolutionMode  getHiResolutionMode() {
        QString filename = GetAppData() + HI_RES_FILENAME;
        int hiResMode= readMode( filename );
        return (hiResMode == HRM_ENABLED ) ? HRM_ENABLED : HRM_DISABLED ;
    }

    // this function is used to control the text name of the high resolution preference checkbox.
    void checkBox(bool set,QCheckBox* button) {
        if (set) {
            hiResolutionNextSessionMode = button->isChecked()? HRM_ENABLED : HRM_DISABLED ;
            setHiResolutionMode(hiResolutionNextSessionMode);
            if ( hiResolutionOperaingMode != hiResolutionNextSessionMode ) {
                QMessageBox::information(nullptr,
                    STR_MessageBox_Information,
                    QObject::tr("High Resolution Mode change will take effect when OSCAR is restarted."));
            }
            return;
        }
        QString label;
        if (hiResolutionNextSessionMode == HRM_ENABLED ) {
            if ( hiResolutionOperaingMode == hiResolutionNextSessionMode ) {
                label = QObject::tr("High Resolution Mode is Enabled");
            } else {
                label = QObject::tr("The High Resolution Mode will be Enabled after Oscar is restarted.");
            }
            button->setChecked(true);
        } else {
            if ( hiResolutionOperaingMode == hiResolutionNextSessionMode ) {
                label = QObject::tr("High Resolution Mode is Disabled");
            } else {
                label = QObject::tr("High Resolution Mode will be Disabled after Oscar is restarted.");
            }
            button->setChecked(false);
        }
        button->setText(label);
    }

    // These functions are for main.cpp
    void init() {
        hiResolutionOperaingMode = getHiResolutionMode();
    };

    void init(HiResolutionMode mode) {
        hiResolutionOperaingMode = mode;
    };

    bool isEnabled() {
        hiResolutionNextSessionMode = hiResolutionOperaingMode ;
        return hiResolutionOperaingMode >= HRM_ENABLED;
    };

    void display(bool actuallyEnabled) {
        bool shouldBeEnabled= (hiResolutionOperaingMode >= HRM_ENABLED) ;
        if (shouldBeEnabled != actuallyEnabled) {
            DEBUGFC O("RESULT MISMATCH") Q(hiResolutionOperaingMode) Q(shouldBeEnabled) Q(actuallyEnabled);
        
        }
        if (actuallyEnabled) {
            qDebug() << "High Resolution Mode is Enabled";
        } else {
            qDebug() << "High Resolution Mode is Disabled";
        }
    }
}
