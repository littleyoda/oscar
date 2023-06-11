/* SleepLib Resvent Loader Implementation
 *
 * Copyright (c) 2019-2023 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef RESVENT_LOADER_H
#define RESVENT_LOADER_H

#include <QVector>
#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation.
//
const int resvent_data_version = 1;
//
//********************************************************************************************

const QString resvent_class_name = "Resvent/Hoffrichter";

/*! \class ResventLoader
    \brief Importer for Resvent iBreezer and Hoffrichter Point 3
    */
class ResventLoader : public CPAPLoader
{
    Q_OBJECT
public:
    ResventLoader();
    virtual ~ResventLoader();

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Look up machine model information of ResMed file structure stored at path
    virtual MachineInfo PeekInfo(const QString & path);

    //! \brief Scans for ResMed SD folder structure signature, and loads any new data if found
    virtual int Open(const QString &);

    //! \brief Returns the version number of this Resvent loader
    virtual int Version() { return resvent_data_version; }

    //! \brief Returns the Machine class name of this loader. ("Resvent")
    virtual const QString &loaderName() { return resvent_class_name; }

    //! \brief Register the ResmedLoader with the list of other machine loaders
    static void Register();

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, resvent_class_name, QObject::tr("Resvent/Hoffrichter"), QString(), QString(), QString(), QObject::tr("iBreeze/Point3"), QDateTime::currentDateTime(), resvent_data_version);
    }

    virtual void initChannels();


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Now for some CPAPLoader overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual QString PresReliefLabel() { return QObject::tr("EPR: "); }

    virtual ChannelID PresReliefMode();
    virtual ChannelID PresReliefLevel();
    virtual ChannelID CPAPModeChannel();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
};

#endif // RESVENT_LOADER_H
