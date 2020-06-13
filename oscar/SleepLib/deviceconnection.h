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
#include <QDateTime>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

class DeviceConnectionManager : public QObject
{
    Q_OBJECT

private:
    DeviceConnectionManager();
    
    class XmlRecorder* m_record;
    class XmlReplay* m_replay;

    QList<class SerialPortInfo> replayAvailablePorts();
    QList<SerialPortInfo> m_serialPorts;
    void reset() {  // clear state
        m_serialPorts.clear();
    }

public:
    static DeviceConnectionManager & getInstance();

    QList<class SerialPortInfo> getAvailablePorts();
    // TODO: method to start a polling thread that maintains the list of ports
    // TODO: emit signal when new port is detected

    void record(class QFile* stream);
    void record(QString & string);
    void replay(class QFile* stream);
    void replay(const QString & string);

};

class DeviceConnection : public QObject
{
    Q_OBJECT
};

// TODO: This class may eventually be internal to a DeviceConnection class,
// but for now it is used to provide support for recording and playback of
// serial port connections before refactoring.
class SerialPortConnection : public DeviceConnection
{
    Q_OBJECT

private:
    QSerialPort m_port;
    QString m_portName;
    void checkResult(bool ok, class ConnectionEvent & event) const;
    void checkResult(qint64 len, class ConnectionEvent & event) const;
    void checkError(class ConnectionEvent & event) const;

private slots:
    void onReadyRead();

signals:
    void readyRead();

public:
    // TODO: temporary methods for legacy compatibility
    SerialPortConnection();
    void setPortName(const QString &name);

    SerialPortConnection(const QString &name);
    virtual ~SerialPortConnection();
    
    bool open(QIODevice::OpenMode mode);
    bool setBaudRate(qint32 baudRate, QSerialPort::Directions directions = QSerialPort::AllDirections);
    bool setDataBits(QSerialPort::DataBits dataBits);
    bool setParity(QSerialPort::Parity parity);
    bool setStopBits(QSerialPort::StopBits stopBits);
    bool setFlowControl(QSerialPort::FlowControl flowControl);
    bool clear(QSerialPort::Directions directions = QSerialPort::AllDirections);
    qint64 bytesAvailable() const;
    qint64 read(char *data, qint64 maxSize);
    qint64 write(const char *data, qint64 maxSize);
    bool flush();
    void close();

};

// TODO: temporary class for legacy compatibility
class SerialPort : public SerialPortConnection
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
    SerialPortInfo();

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
    bool operator==(const SerialPortInfo & other) const;

protected:
    SerialPortInfo(const class QSerialPortInfo & other);
    QHash<QString,QVariant> m_info;

    friend class DeviceConnectionManager;
};

#endif // DEVICECONNECTION_H
