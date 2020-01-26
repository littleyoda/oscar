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
const int viatom_data_version = 2;


/*! \class ViatomLoader
    \brief Unfinished stub for loading Viatom Sleep Ring / Wrist Pulse Oximeter data
*/
class ViatomLoader : public MachineLoader
{
  public:
    ViatomLoader() { m_type = MT_OXIMETER; }
    virtual ~ViatomLoader() { }

    virtual bool Detect(const QString & path);

    virtual int Open(const QString & path);
    Session* ParseFile(const QString & filename);

    static void Register();

    virtual int Version() { return viatom_data_version; }
    virtual const QString &loaderName() { return viatom_class_name; }

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_OXIMETER, 0, viatom_class_name, QObject::tr("Viatom"), QString(), QString(), QString(), QObject::tr("Viatom Software"), QDateTime::currentDateTime(), viatom_data_version);
    }
    
    QStringList getNameFilter();

  //Machine *CreateMachine();

  protected:
    Machine* OpenFile(const QString & filename);
    void SaveSessionToDatabase(Session* session);

    void AddEvent(ChannelID channel, qint64 t, EventDataType value);
    void EndEventList(ChannelID channel, qint64 t);

    Session* m_session;
    qint64 m_step;
    QHash<ChannelID, EventList*> m_importChannels;
    QHash<ChannelID, EventDataType> m_importLastValue;
  private:
};

class ViatomFile
{
public:
    struct Record
    {
        unsigned char spo2;
        unsigned char hr;
        unsigned char oximetry_invalid;
        unsigned char motion;
        unsigned char _unk;
    };
    ViatomFile(QFile & file);
    ~ViatomFile() = default;

    bool ParseHeader();
    QList<Record> ReadData();
    SessionID sessionid() const { return m_sessionid; }
    quint64 timestamp() const { return m_timestamp; }
    int duration() const { return m_duration; }

protected:
    static const int RECORD_SIZE = 5;
    QFile & m_file;
    quint64 m_timestamp;
    int m_duration;
    int m_record_count;
    int m_resolution;
    SessionID m_sessionid;
};

#endif // VIATOMLOADER_H
