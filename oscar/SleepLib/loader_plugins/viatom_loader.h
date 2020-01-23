/* SleepLib Viatom Loader Header
 *
 * Copyright (c) 2019 The OSCAR Team (written by dave madden <dhm@mersenne.com>)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef VIATOMLOADER_H
#define VIATOMLOADER_H

#include "SleepLib/machine_loader.h"

const QString viatom_class_name = "Viatom";
const int viatom_data_version = 1;


/*! \class ViatomLoader
    \brief Unfinished stub for loading Viatom Sleep Ring / Wrist Pulse Oximeter data
*/
class ViatomLoader : public MachineLoader
{
  public:
    ViatomLoader() { m_type = MT_MULTI; }
    virtual ~ViatomLoader() { }

    virtual bool Detect(const QString & path);

    virtual int Open(const QString & path);
    virtual int OpenFile(const QString & filename);

	static void Register();

    virtual int Version() { return viatom_data_version; }
    virtual const QString &loaderName() { return viatom_class_name; }

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_POSITION, 0, viatom_class_name, QObject::tr("Viatom"), QString(), QString(), QString(), QObject::tr("Viatom Software"), QDateTime::currentDateTime(), viatom_data_version);
    }

  //Machine *CreateMachine();

  protected:
  private:
};

#endif // VIATOMLOADER_H
