/* SleepLib ZEO Loader Header
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GARMINLOADER_H
#define GARMINLOADER_H

#include "SleepLib/machine_loader.h"
#include <QJsonValue>
const QString garmin_class_name = "GARMIN";
const int garmin_data_version = 1;

class GARMINLoader : public MachineLoader
{
public:
  GARMINLoader();
  virtual ~GARMINLoader();

  virtual bool Detect(const QString &path)
  {
    Q_UNUSED(path);
    return false;
  } // bypass autoscanner

  virtual int Open(const QString &path)
  {
    Q_UNUSED(path);
    return 0;
  } // Only for CPAP
  virtual int OpenFile(const QString &filename);
  virtual QStringList getNameFilter() { return QStringList("Garmin Connect Json (*.json)"); }
  static void Register();

  virtual int Version() { return garmin_data_version; }
  virtual const QString &loaderName() { return garmin_class_name; }

  // Machine *CreateMachine();
  virtual MachineInfo newInfo()
  {
    return MachineInfo(MT_SLEEPSTAGE, 0, garmin_class_name, QObject::tr("Garmin"), QString(), QString(), QString(), QObject::tr("Garmin Connect"), QDateTime::currentDateTime(), garmin_data_version);
  }

  void closeFile();

protected:
  Session *getSleepLevels(QJsonArray levels);
  QDateTime getDateTime(QJsonValueRef obj);
  void importValues(ChannelID cid, Machine *mi, QJsonArray heartrates, int minRange, int maxRange, QString attr);

private:
  QFile file;
  Machine *mach;

  void AddEvent(ChannelID channel, qint64 t, EventDataType value, int minRange, int maxRange);
  void EndEventList(ChannelID channel, qint64 t);
  Session *m_session;
  QHash<ChannelID, EventList *> m_importChannels;
  QHash<ChannelID, EventDataType> m_importLastValue;
};

#endif // GARMINLOADER_H
