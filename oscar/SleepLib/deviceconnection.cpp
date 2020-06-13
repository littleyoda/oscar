/* Device Connection Class Implementation
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "deviceconnection.h"
#include <QtSerialPort/QSerialPortInfo>
#include <QFile>
#include <QBuffer>
#include <QDateTime>
#include <QDebug>


static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}


// MARK: -
// MARK: XML record/playback base classes

class XmlRecorder
{
public:
    XmlRecorder(class QFile * file);
    XmlRecorder(QString & string);
    ~XmlRecorder();
    inline QXmlStreamWriter & xml() { return *m_xml; }
protected:
    QFile* m_file;  // nullptr for non-file recordings
    QXmlStreamWriter* m_xml;
    
    void prologue();
    void epilogue();
};

class XmlReplay
{
public:
    XmlReplay(class QFile * file);
    XmlReplay(QXmlStreamReader & xml);
    ~XmlReplay();
    template<class T> inline T* getNextEvent();

protected:
    void deserialize(QXmlStreamReader & xml);
    void deserializeEvents(QXmlStreamReader & xml);

    // TODO: maybe the QList should be a QHash on the timestamp?
    // Then indices would be iterators over a sorted list of keys.
    QHash<QString,QList<class XmlReplayEvent*>> m_events;
    QHash<QString,int> m_indices;

    class XmlReplayEvent* getNextEvent(const QString & type);
};

class XmlReplayEvent
{
public:
    XmlReplayEvent();
    virtual ~XmlReplayEvent() = default;
    virtual const QString & tag() const = 0;

    void record(XmlRecorder* xml);
    friend QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const XmlReplayEvent & event);
    friend QXmlStreamReader & operator>>(QXmlStreamReader & xml, XmlReplayEvent & event);

    typedef XmlReplayEvent* (*FactoryMethod)();
    static bool registerClass(const QString & tag, FactoryMethod factory);
    static XmlReplayEvent* createInstance(const QString & tag);

protected:
    static QHash<QString,FactoryMethod> s_factories;

    QDateTime m_time;

    virtual void write(QXmlStreamWriter & /*xml*/) const {}
    virtual void read(QXmlStreamReader & /*xml*/) {}
};
QHash<QString,XmlReplayEvent::FactoryMethod> XmlReplayEvent::s_factories;


XmlRecorder::XmlRecorder(QFile* stream)
    : m_file(stream), m_xml(new QXmlStreamWriter(stream))
{
    prologue();
}

XmlRecorder::XmlRecorder(QString & string)
    : m_file(nullptr), m_xml(new QXmlStreamWriter(&string))
{
    prologue();
}

XmlRecorder::~XmlRecorder()
{
    epilogue();
    delete m_xml;
}

void XmlRecorder::prologue()
{
    Q_ASSERT(m_xml);
    m_xml->setAutoFormatting(true);
    m_xml->setAutoFormattingIndent(2);
    
    m_xml->writeStartElement("xmlreplay");
    m_xml->writeStartElement("events");
}

void XmlRecorder::epilogue()
{
    Q_ASSERT(m_xml);
    m_xml->writeEndElement();  // close events
    // TODO: write out any inline connections
    m_xml->writeEndElement();  // close xmlreplay
}

XmlReplay::XmlReplay(QFile* file)
{
    QXmlStreamReader xml(file);
    deserialize(xml);
}

XmlReplay::XmlReplay(QXmlStreamReader & xml)
{
    deserialize(xml);
}

XmlReplay::~XmlReplay()
{
    for (auto list : m_events.values()) {
        for (auto event : list) {
            delete event;
        }
    }
}

void XmlReplay::deserialize(QXmlStreamReader & xml)
{
    if (xml.readNextStartElement()) {
        if (xml.name() == "xmlreplay") {
            while (xml.readNextStartElement()) {
                if (xml.name() == "events") {
                    deserializeEvents(xml);
                // else TODO: inline connections
                } else {
                    qWarning() << "unexpected payload in replay XML:" << xml.name();
                    xml.skipCurrentElement();
                }
            }
        }
    }
}

void XmlReplay::deserializeEvents(QXmlStreamReader & xml)
{
    while (xml.readNextStartElement()) {
        QString name = xml.name().toString();
        XmlReplayEvent* event = XmlReplayEvent::createInstance(name);
        if (event) {
            xml >> *event;
            auto & events = m_events[name];
            events.append(event);
        } else {
            xml.skipCurrentElement();
        }
    }
}

XmlReplayEvent* XmlReplay::getNextEvent(const QString & type)
{
    XmlReplayEvent* event = nullptr;
    
    if (m_events.contains(type)) {
        auto & events = m_events[type];
        int i = m_indices[type];
        if (i < events.size()) {
            event = events[i];
            // TODO: if we're simulating the original timing, return nullptr if we haven't reached this event's time yet;
            // otherwise:
            m_indices[type] = i + 1;
        }
    }
    return event;
}

template<class T>
T* XmlReplay::getNextEvent()
{
    T* event = dynamic_cast<T*>(getNextEvent(T::TAG));
    return event;
}


// MARK: -
// MARK: XML record/playback event base class

XmlReplayEvent::XmlReplayEvent()
    : m_time(QDateTime::currentDateTime())
{
}

void XmlReplayEvent::record(XmlRecorder* writer)
{
    // Do nothing if we're not recording.
    if (writer != nullptr) {
        writer->xml() << *this;
    }
}

bool XmlReplayEvent::registerClass(const QString & tag, XmlReplayEvent::FactoryMethod factory)
{
    if (s_factories.contains(tag)) {
        qWarning() << "Event class already registered for tag" << tag;
        return false;
    }
    s_factories[tag] = factory;
    return true;
}

XmlReplayEvent* XmlReplayEvent::createInstance(const QString & tag)
{
    XmlReplayEvent* event = nullptr;
    XmlReplayEvent::FactoryMethod factory = s_factories.value(tag);
    if (factory == nullptr) {
        qWarning() << "No event class registered for XML tag" << tag;
    } else {
        event = factory();
    }
    return event;
}

QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const XmlReplayEvent & event)
{
    QDateTime time = event.m_time.toOffsetFromUtc(event.m_time.offsetFromUtc());  // force display of UTC offset
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    // TODO: Can we please deprecate support for Qt older than 5.9?
    QString timestamp = time.toString(Qt::ISODate);
#else
    QString timestamp = time.toString(Qt::ISODateWithMs);
#endif
    xml.writeStartElement(event.tag());
    xml.writeAttribute("time", timestamp);

    event.write(xml);

    xml.writeEndElement();
    return xml;
}

QXmlStreamReader & operator>>(QXmlStreamReader & xml, XmlReplayEvent & event)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == event.tag());

    QDateTime time;
    if (xml.attributes().hasAttribute("time")) {
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
        // TODO: Can we please deprecate support for Qt older than 5.9?
        time = QDateTime::fromString(xml.attributes().value("time").toString(), Qt::ISODate);
#else
        time = QDateTime::fromString(xml.attributes().value("time").toString(), Qt::ISODateWithMs);
#endif
    } else {
        qWarning() << "Missing timestamp in" << xml.name() << "tag, using current time";
        time = QDateTime::currentDateTime();
    }
    
    event.read(xml);
    return xml;
}

template<typename T> QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const QList<T> & list)
{
    for (auto & item : list) {
        xml << item;
    }
    return xml;
}

template<typename T> QXmlStreamReader & operator>>(QXmlStreamReader & xml, QList<T> & list)
{
    list.clear();
    while (xml.readNextStartElement()) {
        T item;
        xml >> item;
        list.append(item);
    }
    return xml;
}

template <typename Derived>
class XmlReplayBase : public XmlReplayEvent
{
public:
    static const QString TAG;
    static const bool registered;
    virtual const QString & tag() const { return TAG; };

    static XmlReplayEvent* createInstance()
    {
        Derived* instance = new Derived();
        return static_cast<XmlReplayEvent*>(instance);
    }
};

#define REGISTER_XMLREPLAYEVENT(tag, type) \
template<> const QString XmlReplayBase<type>::TAG = tag; \
template<> const bool XmlReplayBase<type>::registered = XmlReplayEvent::registerClass(XmlReplayBase<type>::TAG, XmlReplayBase<type>::createInstance);


// MARK: -
// MARK: Device connection manager

inline DeviceConnectionManager & DeviceConnectionManager::getInstance()
{
    static DeviceConnectionManager instance;
    return instance;
}

DeviceConnectionManager::DeviceConnectionManager()
    : m_record(nullptr), m_replay(nullptr)
{
}

void DeviceConnectionManager::record(QFile* stream)
{
    if (m_record) {
        delete m_record;
    }
    if (stream) {
        m_record = new XmlRecorder(stream);
    } else {
        // nullptr turns off recording
        m_record = nullptr;
    }
}

void DeviceConnectionManager::record(QString & string)
{
    if (m_record) {
        delete m_record;
    }
    m_record = new XmlRecorder(string);
}

void DeviceConnectionManager::replay(const QString & string)
{
    QXmlStreamReader xml(string);
    reset();
    if (m_replay) {
        delete m_replay;
    }
    m_replay = new XmlReplay(xml);
}

void DeviceConnectionManager::replay(QFile* file)
{
    reset();
    if (m_replay) {
        delete m_replay;
    }
    if (file) {
        m_replay = new XmlReplay(file);
    } else {
        // nullptr turns off replay
        m_replay = nullptr;
    }
}


// MARK: -
// MARK: Device manager events

class GetAvailablePortsEvent : public XmlReplayBase<GetAvailablePortsEvent>
{
public:
    QList<SerialPortInfo> m_ports;

protected:
    virtual void write(QXmlStreamWriter & xml) const
    {
        xml << m_ports;
    }
    virtual void read(QXmlStreamReader & xml)
    {
        xml >> m_ports;
    }
};
REGISTER_XMLREPLAYEVENT("getAvailablePorts", GetAvailablePortsEvent);


QList<SerialPortInfo> DeviceConnectionManager::getAvailablePorts()
{
    GetAvailablePortsEvent event;

    if (!m_replay) {
        for (auto & info : QSerialPortInfo::availablePorts()) {
            event.m_ports.append(SerialPortInfo(info));
        }
    } else {
        auto replayEvent = m_replay->getNextEvent<GetAvailablePortsEvent>();
        if (replayEvent) {
            event.m_ports = replayEvent->m_ports;
        } else {
            // If there are no replay events available, reuse the most recent state.
            event.m_ports = m_serialPorts;
        }
    }
    m_serialPorts = event.m_ports;

    event.record(m_record);
    return event.m_ports;
}


// MARK: -
// MARK: Serial port info

SerialPortInfo::SerialPortInfo(const QSerialPortInfo & other)
{
    if (other.isNull() == false) {
        m_info["portName"] = other.portName();
        m_info["systemLocation"] = other.systemLocation();
        m_info["description"] = other.description();
        m_info["manufacturer"] = other.manufacturer();
        m_info["serialNumber"] = other.serialNumber();
        if (other.hasVendorIdentifier()) {
            m_info["vendorIdentifier"] = other.vendorIdentifier();
        }
        if (other.hasProductIdentifier()) {
            m_info["productIdentifier"] = other.productIdentifier();
        }
    }
}

SerialPortInfo::SerialPortInfo(const SerialPortInfo & other)
    : m_info(other.m_info)
{
}

SerialPortInfo::SerialPortInfo(const QString & data)
{
    QXmlStreamReader xml(data);
    xml.readNextStartElement();
    xml >> *this;
}

SerialPortInfo::SerialPortInfo()
{
}

// TODO: This is a temporary wrapper until we begin refactoring.
QList<SerialPortInfo> SerialPortInfo::availablePorts()
{
    return DeviceConnectionManager::getInstance().getAvailablePorts();
}

QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const SerialPortInfo & info)
{
    xml.writeStartElement("serial");
    if (info.isNull() == false) {
        xml.writeAttribute("portName", info.portName());
        xml.writeAttribute("systemLocation", info.systemLocation());
        xml.writeAttribute("description", info.description());
        xml.writeAttribute("manufacturer", info.manufacturer());
        xml.writeAttribute("serialNumber", info.serialNumber());
        if (info.hasVendorIdentifier()) {
            xml.writeAttribute("vendorIdentifier", hex(info.vendorIdentifier()));
        }
        if (info.hasProductIdentifier()) {
            xml.writeAttribute("productIdentifier", hex(info.productIdentifier()));
        }
    }
    xml.writeEndElement();
    return xml;
}

QXmlStreamReader & operator>>(QXmlStreamReader & xml, SerialPortInfo & info)
{
    if (xml.atEnd() == false && xml.isStartElement() && xml.name() == "serial") {
        for (auto & attribute : xml.attributes()) {
            QString name = attribute.name().toString();
            QString value = attribute.value().toString();
            if (name == "vendorIdentifier" || name == "productIdentifier") {
                bool ok;
                quint16 id = value.toUInt(&ok, 0);
                if (ok) {
                    info.m_info[name] = id;
                } else {
                    qWarning() << "invalid" << name << "value" << value;
                }
            } else {
                info.m_info[name] = value;
            }
        }
    } else {
        qWarning() << "no <serial> tag";
    }
    xml.skipCurrentElement();
    return xml;
}

SerialPortInfo::operator QString() const
{
    QString out;
    QXmlStreamWriter xml(&out);
    xml << *this;
    return out;
}

bool SerialPortInfo::operator==(const SerialPortInfo & other) const
{
    return m_info == other.m_info;
}


// MARK: -
// MARK: Serial port connection

// TODO: log these to XML stream

class ConnectionEvent
{
public:
    ConnectionEvent(const QString & tag)
        : m_tag(tag)
    {
    }
    void set(const QString & name, const QString & value)
    {
        m_values[name] = value;
        m_keys.append(name);
    }
    void set(const QString & name, qint64 value)
    {
        set(name, QString::number(value));
    }
    void setData(const char* data, qint64 length)
    {
        QStringList bytes;
        for (qint64 i = 0; i < length; i++) {
            bytes.append(QString("%1").arg((unsigned char) data[i], 2, 16, QChar('0')).toUpper());
        }
        m_data = bytes.join(QChar(' '));
    }
    inline bool ok() const { return m_values.contains("error") == false; }
    operator QString() const;

protected:
    const QString m_tag;
    QHash<QString,QString> m_values;
    QList<QString> m_keys;
    QString m_data;
    friend QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const ConnectionEvent & event);
};

class SetValueEvent : public ConnectionEvent
{
public:
    SetValueEvent(const QString & name, int value)
        : ConnectionEvent("set")
    {
        set(name, value);
    }
};

ConnectionEvent::operator QString() const
{
    QString out;
    QXmlStreamWriter xml(&out);
    xml << *this;
    return out;
}

QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const ConnectionEvent & event)
{
    xml.writeStartElement(event.m_tag);
    for (auto key : event.m_keys) {
        xml.writeAttribute(key, event.m_values[key]);
    }
    if (!event.m_data.isEmpty()) {
        xml.writeCharacters(event.m_data);
    }
    xml.writeEndElement();
    return xml;
}


SerialPortConnection::SerialPortConnection(const QString & name)
    : m_portName(name)
{
    connect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

// TODO: temporary method for legacy compatibility
SerialPortConnection::SerialPortConnection()
{
    connect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

SerialPortConnection::~SerialPortConnection()
{
    disconnect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

// TODO: temporary method for legacy compatibility
void SerialPortConnection::setPortName(const QString &name)
{
    Q_ASSERT(m_portName.isEmpty());
    m_portName = name;
}

// TODO: This will eventually be open(), the constructor will be given the name, and the mode will always be ReadWrite
bool SerialPortConnection::open(QIODevice::OpenMode mode)
{
    Q_ASSERT(mode == QSerialPort::ReadWrite);
    ConnectionEvent event("openConnection");
    event.set("type", "serial");
    event.set("port", m_portName);

    m_port.setPortName(m_portName);
    checkResult(m_port.open(mode), event);

    // TODO: send this event back to manager to log
    qDebug().noquote() << event;

    return event.ok();
}

bool SerialPortConnection::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    SetValueEvent event("baudRate", baudRate);
    event.set("directions", directions);

    checkResult(m_port.setBaudRate(baudRate, directions), event);
    qDebug().noquote() << event;

    return event.ok();
}

bool SerialPortConnection::setDataBits(QSerialPort::DataBits dataBits)
{
    SetValueEvent event("setDataBits", dataBits);

    checkResult(m_port.setDataBits(dataBits), event);
    qDebug().noquote() << event;

    return event.ok();
}

bool SerialPortConnection::setParity(QSerialPort::Parity parity)
{
    SetValueEvent event("setParity", parity);

    checkResult(m_port.setParity(parity), event);
    qDebug().noquote() << event;

    return event.ok();
}

bool SerialPortConnection::setStopBits(QSerialPort::StopBits stopBits)
{
    SetValueEvent event("setStopBits", stopBits);

    checkResult(m_port.setStopBits(stopBits), event);
    qDebug().noquote() << event;

    return event.ok();
}

bool SerialPortConnection::setFlowControl(QSerialPort::FlowControl flowControl)
{
    SetValueEvent event("setFlowControl", flowControl);

    checkResult(m_port.setFlowControl(flowControl), event);
    qDebug().noquote() << event;

    return event.ok();
}

bool SerialPortConnection::clear(QSerialPort::Directions directions)
{
    ConnectionEvent event("clear");
    event.set("directions", directions);

    checkResult(m_port.clear(directions), event);
    qDebug().noquote() << event;

    return event.ok();
}

qint64 SerialPortConnection::bytesAvailable() const
{
    ConnectionEvent event("get");
    
    qint64 result = m_port.bytesAvailable();
    event.set("bytesAvailable", result);
    checkResult(result, event);
    qDebug().noquote() << event;

    return result;
}

qint64 SerialPortConnection::read(char *data, qint64 maxSize)
{
    ConnectionEvent event("rx");

    qint64 len = m_port.read(data, maxSize);
    if (len > 0) {
        event.setData(data, len);
    }
    event.set("len", len);
    if (len != maxSize) {
        event.set("req", maxSize);
    }
    checkResult(len, event);
    qDebug().noquote() << event;

    return len;
}

qint64 SerialPortConnection::write(const char *data, qint64 maxSize)
{
    ConnectionEvent event("tx");
    
    event.setData(data, maxSize);
    qint64 len = m_port.write(data, maxSize);
    event.set("len", len);
    if (len != maxSize) {
        event.set("req", maxSize);
    }
    checkResult(len, event);
    qDebug().noquote() << event;

    return len;
}

bool SerialPortConnection::flush()
{
    ConnectionEvent event("flush");

    checkResult(m_port.flush(), event);
    qDebug().noquote() << event;

    return event.ok();
}

void SerialPortConnection::close()
{
    ConnectionEvent event("closeConnection");
    event.set("type", "serial");
    event.set("port", m_portName);

    // TODO: the separate connection stream will have an enclosing "connection" tag with these
    // attributes. The main device connection manager stream will log this openConnection/
    // closeConnection pair. We'll also need to include a loader ID and stream version number
    // in the "connection" tag, so that if we ever have to change a loader's download code,
    // the older replays will still work as expected.

    m_port.close();
    checkError(event);

    qDebug().noquote() << event;
}

void SerialPortConnection::onReadyRead()
{
    ConnectionEvent event("readyRead");

    // TODO: Most of the playback API reponds to the caller. How do we replay port-driven events?
    qDebug().noquote() << event;

    emit readyRead();
}

void SerialPortConnection::checkResult(bool ok, ConnectionEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (ok && error == QSerialPort::NoError) return;
    event.set("error", error);
    if (ok) event.set("ok", ok);  // we don't expect to see this, but we should know if it happens
}

void SerialPortConnection::checkResult(qint64 len, ConnectionEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (len < 0 || error != QSerialPort::NoError) {
        event.set("error", error);
    }
}

void SerialPortConnection::checkError(ConnectionEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (error != QSerialPort::NoError) {
        event.set("error", error);
    }
}
