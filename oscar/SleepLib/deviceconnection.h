/* Device Connection Class Header
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef DEVICECONNECTION_H
#define DEVICECONNECTION_H

// TODO: This file will eventually abstract serial port or bluetooth (or other)
// connections to devices. For now it just supports serial ports.

#include <QtSerialPort/QSerialPort>
#include <QHash>
#include <QVariant>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

class DeviceConnectionManager : public QObject
{
    Q_OBJECT

private:
    DeviceConnectionManager();
    QXmlStreamWriter* m_record;
    QXmlStreamReader* m_replay;
    void startEvent(const QString & event);
    void endEvent();

public:
    static DeviceConnectionManager & getInstance();

    QList<class SerialPortInfo> getAvailablePorts();
    // TODO: method to start a polling thread that maintains the list of ports
    // TODO: emit signal when new port is detected

    static void Record(QXmlStreamWriter* stream);
    static void Replay(QXmlStreamReader* stream);

};

// TODO: This class may eventually be internal to a DeviceConnection class,
// but for now it is used to provide support for recording and playback of
// serial port connections before refactoring.
class SerialPort : public QSerialPort
{
};

// TODO: This class's functionality will eventually be internal to a
// DeviceConnection class, but for now it is needed to support recording
// and playback of serial port scanning before refactoring.
class SerialPortInfo
{
public:
    static QList<SerialPortInfo> availablePorts();
    SerialPortInfo(const SerialPortInfo & other);
    SerialPortInfo(const QString & data);

    inline QString portName() const { return m_info["portName"].toString(); }
    inline QString systemLocation() const { return m_info["systemLocation"].toString(); }
    inline QString description() const { return m_info["description"].toString(); }
    inline QString manufacturer() const { return m_info["manufacturer"].toString(); }
    inline QString serialNumber() const { return m_info["serialNumber"].toString(); }

    inline quint16 vendorIdentifier() const { return m_info["vendorIdentifier"].toInt(); }
    inline quint16 productIdentifier() const { return m_info["productIdentifier"].toInt(); }

    inline bool hasVendorIdentifier() const { return m_info.contains("vendorIdentifier"); }
    inline bool hasProductIdentifier() const { return m_info.contains("productIdentifier"); }

    inline bool isNull() const { return m_info.isEmpty(); }

    operator QString() const;
    friend QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const SerialPortInfo & info);
    friend QXmlStreamReader & operator>>(QXmlStreamReader & xml, SerialPortInfo & info);

protected:
    SerialPortInfo(const class QSerialPortInfo & other);
    QHash<QString,QVariant> m_info;

    friend class DeviceConnectionManager;
};

#endif // DEVICECONNECTION_H
