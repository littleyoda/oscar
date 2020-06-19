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
    inline void lock() { m_mutex.lock(); }
    inline void unlock() { m_mutex.unlock(); }
protected:
    QFile* m_file;  // nullptr for non-file recordings
    QXmlStreamWriter* m_xml;
    QMutex m_mutex;
    
    void prologue();
    void epilogue();
};

class XmlReplayEvent;

class XmlReplay
{
public:
    XmlReplay(class QFile * file);
    XmlReplay(QXmlStreamReader & xml);
    ~XmlReplay();
    template<class T> inline T* getNextEvent(const QString & id = "");

protected:
    void deserialize(QXmlStreamReader & xml);
    void deserializeEvents(QXmlStreamReader & xml);

    QHash<QString,QHash<QString,QList<XmlReplayEvent*>>> m_eventIndex;
    QHash<QString,QHash<QString,int>> m_indexPosition;
    QList<XmlReplayEvent*> m_events;

    XmlReplayEvent* getNextEvent(const QString & type, const QString & id = "");
    void seekToTime(const QDateTime & time);

    XmlReplayEvent* m_pendingSignal;
    QMutex m_lock;
    inline void lock() { m_lock.lock(); }
    inline void unlock() { m_lock.unlock(); }
    void processPendingSignals(const QObject* target);
    friend class XmlReplayLock;
};

class XmlReplayEvent
{
public:
    XmlReplayEvent();
    virtual ~XmlReplayEvent() = default;
    virtual const QString & tag() const = 0;
    virtual const QString id() const { static const QString none(""); return none; }
    virtual bool randomAccess() const { return false; }

    void record(XmlRecorder* xml);
    friend QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const XmlReplayEvent & event);
    friend QXmlStreamReader & operator>>(QXmlStreamReader & xml, XmlReplayEvent & event);

    typedef XmlReplayEvent* (*FactoryMethod)();
    static bool registerClass(const QString & tag, FactoryMethod factory);
    static XmlReplayEvent* createInstance(const QString & tag);

    void set(const QString & name, const QString & value)
    {
        if (!m_values.contains(name)) {
            m_keys.append(name);
        }
        m_values[name] = value;
    }
    void set(const QString & name, qint64 value)
    {
        set(name, QString::number(value));
    }
    void setData(const char* data, qint64 length)
    {
        Q_ASSERT(usesData() == true);
        QByteArray bytes = QByteArray::fromRawData(data, length);
        m_data = bytes.toHex(' ').toUpper();
    }
    inline QString get(const QString & name) const
    {
        if (!m_values.contains(name)) {
            qWarning().noquote() << *this << "missing attribute:" << name;
        }
        return m_values[name];
    }
    QByteArray getData() const
    {
        Q_ASSERT(usesData() == true);
        return QByteArray::fromHex(m_data.toUtf8());
    }
    inline bool ok() const { return m_values.contains("error") == false; }
    operator QString() const
    {
        QString out;
        QXmlStreamWriter xml(&out);
        xml << *this;
        return out;
    }

    void copyIf(const XmlReplayEvent* other)
    {
        // Leave the proposed event alone if there was no replay event.
        if (other == nullptr) {
            return;
        }
        // Do not copy timestamp.
        m_values = other->m_values;
        m_keys = other->m_keys;
        m_data = other->m_data;
    }

protected:
    static QHash<QString,FactoryMethod> s_factories;

    QDateTime m_time;
    XmlReplayEvent* m_next;

    const char* m_signal;
    inline bool isSignal() const { return m_signal != nullptr; }
    virtual void signal(QObject* target)
    {
        QMetaObject::invokeMethod(target, m_signal, Qt::QueuedConnection);
    }

    QHash<QString,QString> m_values;
    QList<QString> m_keys;
    QString m_data;

    virtual bool usesData() const { return false; }
    virtual void write(QXmlStreamWriter & xml) const
    {
        for (auto key : m_keys) {
            xml.writeAttribute(key, m_values[key]);
        }
        if (!m_data.isEmpty()) {
            Q_ASSERT(usesData() == true);
            xml.writeCharacters(m_data);
        }
    }
    virtual void read(QXmlStreamReader & xml)
    {
        QXmlStreamAttributes attribs = xml.attributes();
        for (auto & attrib : attribs) {
            if (attrib.name() != "time") {    // skip outer timestamp
                set(attrib.name().toString(), attrib.value().toString());
            }
        }
        if (usesData()) {
            m_data = xml.readElementText();
        } else {
            xml.skipCurrentElement();
        }
    }

    friend class XmlReplay;
};
QHash<QString,XmlReplayEvent::FactoryMethod> XmlReplayEvent::s_factories;

class XmlReplayLock
{
public:
    XmlReplayLock(const QObject* obj, XmlReplay* replay)
        : m_target(obj), m_replay(replay)
    {
        if (m_replay) {
            m_replay->lock();
        }
    }
    ~XmlReplayLock()
    {
        if (m_replay) {
            m_replay->processPendingSignals(m_target);
            m_replay->unlock();
        }
    }

protected:
    const QObject* m_target;
    XmlReplay* m_replay;
};

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
    : m_pendingSignal(nullptr)
{
    QXmlStreamReader xml(file);
    deserialize(xml);
}

XmlReplay::XmlReplay(QXmlStreamReader & xml)
    : m_pendingSignal(nullptr)
{
    deserialize(xml);
}

XmlReplay::~XmlReplay()
{
    for (auto event : m_events) {
        delete event;
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
        QString type = xml.name().toString();
        XmlReplayEvent* event = XmlReplayEvent::createInstance(type);
        if (event) {
            xml >> *event;

            // Add to list
            if (m_events.isEmpty() == false) {
                m_events.last()->m_next = event;
            }
            m_events.append(event);

            // Add to index
            const QString & id = event->id();
            auto & events = m_eventIndex[type][id];
            events.append(event);
        } else {
            xml.skipCurrentElement();
        }
    }
}

void XmlReplay::processPendingSignals(const QObject* target)
{
    if (m_pendingSignal) {
        XmlReplayEvent* pending = m_pendingSignal;
        m_pendingSignal = nullptr;

        // Dequeue the signal from the index; this may update m_pendingSignal.
        XmlReplayEvent* dequeued = getNextEvent(pending->tag(), pending->id());
        if (dequeued != pending) {
            qWarning() << "triggered signal doesn't match dequeued signal!" << pending << dequeued;
        }
        
        // It is safe to re-cast this as non-const because signals are deferred
        // and cannot alter the underlying target until the const method holding
        // the lock releases it at function exit.
        pending->signal(const_cast<QObject*>(target));

        /*
        XmlReplayEvent* next = m_pendingSignal->m_next;
        if (next && next->isSignal() == false) {
            next = nullptr;
        }
        if (next) {
            qDebug() << "UNTESTED: multiple signal events in a row:" << m_pendingSignal->tag() << next->tag();
        }
        m_pendingSignal = next;
        */
    }
}

void XmlReplay::seekToTime(const QDateTime & time)
{
    for (auto & type : m_eventIndex.keys()) {
        for (auto & key : m_eventIndex[type].keys()) {
            auto & events = m_eventIndex[type][key];
            int pos;
            for (pos = 0; pos < events.size(); pos++) {
                auto & event = events.at(pos);
                // Random-access events should always start searching from position 0.
                if (event->randomAccess() || event->m_time >= time) {
                    break;
                }
            }
            // If pos == events.size(), that means there are no more events of this type
            // after the given time.
            m_indexPosition[type][key] = pos;
        }
    }
}

XmlReplayEvent* XmlReplay::getNextEvent(const QString & type, const QString & id)
{
    XmlReplayEvent* event = nullptr;
    
    if (m_lock.tryLock()) {
        qWarning() << "XML replay" << type << "object not locked by event handler!";
        m_lock.unlock();
    }

    if (m_eventIndex.contains(type)) {
        auto & ids = m_eventIndex[type];
        if (ids.contains(id)) {
            auto & events = ids[id];

            // Start at the position identified by the previous random-access event.
            int pos = m_indexPosition[type][id];
            if (pos < events.size()) {
                event = events.at(pos);
                // TODO: if we're simulating the original timing, return nullptr if we haven't reached this event's time yet;
                // otherwise:
                events.removeAt(pos);
            }
        }
    }
    
    if (event && event->randomAccess()) {
        seekToTime(event->m_time);
    }

    if (event && event->m_next && event->m_next->isSignal()) {
        Q_ASSERT(m_pendingSignal == nullptr);  // if this ever fails, we may need m_pendingSignal to be a list
        m_pendingSignal = event->m_next;
    }

    return event;
}

template<class T>
T* XmlReplay::getNextEvent(const QString & id)
{
    T* event = dynamic_cast<T*>(getNextEvent(T::TAG, id));
    return event;
}


// MARK: -
// MARK: XML record/playback event base class

XmlReplayEvent::XmlReplayEvent()
    : m_time(QDateTime::currentDateTime()), m_next(nullptr), m_signal(nullptr)
{
}

void XmlReplayEvent::record(XmlRecorder* writer)
{
    // Do nothing if we're not recording.
    if (writer != nullptr) {
        writer->lock();
        writer->xml() << *this;
        writer->unlock();
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
    event.m_time = time;
    
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

// We use this extra CRTP templating so that concrete event subclasses require as little code as possible.
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

DeviceConnection* DeviceConnectionManager::openConnection(const QString & type, const QString & name)
{
    if (!s_factories.contains(type)) {
        qWarning() << "Unknown device connection type:" << type;
        return nullptr;
    }
    if (m_connections.contains(name)) {
        qWarning() << "connection to" << name << "already open";
        return nullptr;
    }

    DeviceConnection* conn = s_factories[type](name, m_record, m_replay);
    if (conn) {
        if (conn->open()) {
            m_connections[name] = conn;
        } else {
            qWarning().noquote() << "unable to open" << type << "connection to" << name;
            delete conn;
            conn = nullptr;
        }
    } else {
        qWarning() << "unable to create" << type << "connection to" << name;
    }
    // TODO: record event
    return conn;
}

void DeviceConnectionManager::connectionClosed(DeviceConnection* conn)
{
    Q_ASSERT(conn);
    const QString & type = conn->type();
    const QString & name = conn->name();

    if (m_connections.contains(name)) {
        if (m_connections[name] == conn) {
            m_connections.remove(name);
        } else {
            qWarning() << "connection to" << name << "not created by openConnection!";
        }
    } else {
        qWarning() << type << "connection to" << name << "missing";
    }
    // TODO: record event
}

// Temporary convenience function for code that still supports only serial ports.
SerialPortConnection* DeviceConnectionManager::openSerialPortConnection(const QString & portName)
{
    return dynamic_cast<SerialPortConnection*>(getInstance().openConnection(SerialPortConnection::TYPE, portName));
}


QHash<QString,DeviceConnection::FactoryMethod> DeviceConnectionManager::s_factories;

bool DeviceConnectionManager::registerClass(const QString & type, DeviceConnection::FactoryMethod factory)
{
    if (s_factories.contains(type)) {
        qWarning() << "Connection class already registered for type" << type;
        return false;
    }
    s_factories[type] = factory;
    return true;
}

#define REGISTER_DEVICECONNECTION(type, T) \
const QString T::TYPE = type; \
const bool T::registered = DeviceConnectionManager::registerClass(T::TYPE, T::createInstance); \
DeviceConnection* T::createInstance(const QString & name, XmlRecorder* record, XmlReplay* replay) { return static_cast<DeviceConnection*>(new T(name, record, replay)); }

// MARK: -
// MARK: Device manager events

class GetAvailableSerialPortsEvent : public XmlReplayBase<GetAvailableSerialPortsEvent>
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
REGISTER_XMLREPLAYEVENT("getAvailableSerialPorts", GetAvailableSerialPortsEvent);


QList<SerialPortInfo> DeviceConnectionManager::getAvailableSerialPorts()
{
    XmlReplayLock lock(this, m_replay);
    GetAvailableSerialPortsEvent event;

    if (!m_replay) {
        for (auto & info : QSerialPortInfo::availablePorts()) {
            event.m_ports.append(SerialPortInfo(info));
        }
    } else {
        auto replayEvent = m_replay->getNextEvent<GetAvailableSerialPortsEvent>();
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
    return DeviceConnectionManager::getInstance().getAvailableSerialPorts();
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
// MARK: Device connection base class

DeviceConnection::DeviceConnection(const QString & name, XmlRecorder* record, XmlReplay* replay)
    : m_name(name), m_record(record), m_replay(replay), m_opened(false)
{
}

DeviceConnection::~DeviceConnection()
{
}

class SetValueEvent : public XmlReplayBase<SetValueEvent>
{
public:
    SetValueEvent() {}
    SetValueEvent(const QString & name, int value)
    {
        set(name, value);
    }
    virtual const QString id() const { return m_keys.first(); }
};
REGISTER_XMLREPLAYEVENT("set", SetValueEvent);

class GetValueEvent : public XmlReplayBase<GetValueEvent>
{
public:
    GetValueEvent() {}
    GetValueEvent(const QString & id)
    {
        set(id, 0);
    }
    virtual const QString id() const { return m_keys.first(); }
    void setValue(qint64 value)
    {
        if (m_keys.isEmpty()) {
            qWarning() << "setValue: get event missing key";
            return;
        }
        set(m_keys.first(), value);
    }
    QString value() const
    {
        if (m_keys.isEmpty()) {
            qWarning() << "getValue: get event missing key";
            return 0;
        }
        return get(m_keys.first());
    }
};
REGISTER_XMLREPLAYEVENT("get", GetValueEvent);

class OpenConnectionEvent : public XmlReplayBase<OpenConnectionEvent>
{
public:
    OpenConnectionEvent() {}
    OpenConnectionEvent(const QString & type, const QString & name)
    {
        set("type", type);
        set("name", name);
    }
    virtual const QString id() const { return m_values["name"]; }
};
REGISTER_XMLREPLAYEVENT("openConnection", OpenConnectionEvent);

class CloseConnectionEvent : public XmlReplayBase<CloseConnectionEvent>
{
public:
    CloseConnectionEvent() {}
    CloseConnectionEvent(const QString & type, const QString & name)
    {
        set("type", type);
        set("name", name);
    }
    virtual const QString id() const { return m_values["name"]; }
};
REGISTER_XMLREPLAYEVENT("closeConnection", CloseConnectionEvent);

class ClearConnectionEvent : public XmlReplayBase<ClearConnectionEvent>
{
};
REGISTER_XMLREPLAYEVENT("clear", ClearConnectionEvent);

class FlushConnectionEvent : public XmlReplayBase<FlushConnectionEvent>
{
};
REGISTER_XMLREPLAYEVENT("flush", FlushConnectionEvent);

class ReceiveDataEvent : public XmlReplayBase<ReceiveDataEvent>
{
    virtual bool usesData() const { return true; }
};
REGISTER_XMLREPLAYEVENT("rx", ReceiveDataEvent);

class TransmitDataEvent : public XmlReplayBase<TransmitDataEvent>
{
    virtual bool usesData() const { return true; }
public:
    virtual const QString id() const { return m_data; }
    virtual bool randomAccess() const { return true; }
};
REGISTER_XMLREPLAYEVENT("tx", TransmitDataEvent);

class ReadyReadEvent : public XmlReplayBase<ReadyReadEvent>
{
public:
    ReadyReadEvent() { m_signal = "onReadyRead"; }
};
REGISTER_XMLREPLAYEVENT("readyRead", ReadyReadEvent);


// MARK: -
// MARK: Serial port connection

REGISTER_DEVICECONNECTION("serial", SerialPortConnection);

SerialPortConnection::SerialPortConnection(const QString & name, XmlRecorder* record, XmlReplay* replay)
    : DeviceConnection(name, record, replay)
{
    connect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

SerialPortConnection::~SerialPortConnection()
{
    if (m_opened) {
        close();
        DeviceConnectionManager::getInstance().connectionClosed(this);
    }
    disconnect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

bool SerialPortConnection::open()
{
    if (m_opened) {
        qWarning() << "serial connection to" << m_name << "already opened";
        return false;
    }
    XmlReplayLock lock(this, m_replay);
    OpenConnectionEvent event("serial", m_name);

    if (!m_replay) {
        m_port.setPortName(m_name);
        checkResult(m_port.open(QSerialPort::ReadWrite), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<OpenConnectionEvent>(m_name);
        if (replayEvent) {
            event.copyIf(replayEvent);
        } else {
            event.set("error", QSerialPort::DeviceNotFoundError);
        }
    }

    event.record(m_record);
    m_opened = event.ok();
    
    // TODO: if OK, open a connection substream for connection events

    return event.ok();
}

bool SerialPortConnection::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("baudRate", baudRate);
    event.set("directions", directions);

    if (!m_replay) {
        checkResult(m_port.setBaudRate(baudRate, directions), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::setDataBits(QSerialPort::DataBits dataBits)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("setDataBits", dataBits);

    if (!m_replay) {
        checkResult(m_port.setDataBits(dataBits), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::setParity(QSerialPort::Parity parity)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("setParity", parity);

    if (!m_replay) {
        checkResult(m_port.setParity(parity), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::setStopBits(QSerialPort::StopBits stopBits)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("setStopBits", stopBits);

    if (!m_replay) {
        checkResult(m_port.setStopBits(stopBits), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::setFlowControl(QSerialPort::FlowControl flowControl)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("setFlowControl", flowControl);

    if (!m_replay) {
        checkResult(m_port.setFlowControl(flowControl), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::clear(QSerialPort::Directions directions)
{
    XmlReplayLock lock(this, m_replay);
    ClearConnectionEvent event;
    event.set("directions", directions);

    if (!m_replay) {
        checkResult(m_port.clear(directions), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<ClearConnectionEvent>();
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

qint64 SerialPortConnection::bytesAvailable() const
{
    XmlReplayLock lock(this, m_replay);
    GetValueEvent event("bytesAvailable");
    qint64 result;
    
    if (!m_replay) {
        result = m_port.bytesAvailable();
        event.setValue(result);
        checkResult(result, event);
    } else {
        auto replayEvent = m_replay->getNextEvent<GetValueEvent>(event.id());
        event.copyIf(replayEvent);

        bool ok;
        result = event.value().toLong(&ok);
        if (!ok) {
            qWarning() << event.tag() << event.id() << "has bad value";
        }
    }
    event.record(m_record);

    return result;
}

qint64 SerialPortConnection::read(char *data, qint64 maxSize)
{
    XmlReplayLock lock(this, m_replay);
    qint64 len;
    ReceiveDataEvent event;

    if (!m_replay) {
        len = m_port.read(data, maxSize);
        if (len > 0) {
            event.setData(data, len);
        }
        event.set("len", len);
        if (len != maxSize) {
            event.set("req", maxSize);
        }
        checkResult(len, event);
    } else {
        auto replayEvent = m_replay->getNextEvent<ReceiveDataEvent>();
        event.copyIf(replayEvent);
        if (!replayEvent) {
            qWarning() << "reading data past replay";
            event.set("len", -1);
            event.set("error", QSerialPort::ReadError);
        }

        bool ok;
        len = event.get("len").toLong(&ok);
        if (ok) {
            if (event.ok()) {
                if (len != maxSize) {
                    qWarning() << "replay of" << len << "bytes but" << maxSize << "requested";
                }
                if (len > maxSize) {
                    len = maxSize;
                }
                QByteArray replayData = event.getData();
                memcpy(data, replayData, len);
            }
        } else {
            qWarning() << event << "has bad len";
            len = -1;
        }
    }
    event.record(m_record);

    return len;
}

qint64 SerialPortConnection::write(const char *data, qint64 maxSize)
{
    XmlReplayLock lock(this, m_replay);
    qint64 len;
    TransmitDataEvent event;
    event.setData(data, maxSize);
    
    if (!m_replay) {
        len = m_port.write(data, maxSize);
        event.set("len", len);
        if (len != maxSize) {
            event.set("req", maxSize);
        }
        checkResult(len, event);
    } else {
        auto replayEvent = m_replay->getNextEvent<TransmitDataEvent>(event.id());
        event.copyIf(replayEvent);
        if (!replayEvent) {
            qWarning() << "writing data past replay";
            event.set("len", -1);
            event.set("error", QSerialPort::ReadError);
        }

        bool ok;
        len = event.get("len").toLong(&ok);
        if (!ok) {
            qWarning() << event << "has bad len";
            len = -1;
        }
    }
    event.record(m_record);

    return len;
}

bool SerialPortConnection::flush()
{
    XmlReplayLock lock(this, m_replay);
    FlushConnectionEvent event;

    if (!m_replay) {
        checkResult(m_port.flush(), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<FlushConnectionEvent>();
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

void SerialPortConnection::close()
{
    XmlReplayLock lock(this, m_replay);
    CloseConnectionEvent event("serial", m_name);

    // TODO: the separate connection stream will have an enclosing "connection" tag with these
    // attributes. The main device connection manager stream will log this openConnection/
    // closeConnection pair. We'll also need to include a loader ID and stream version number
    // in the "connection" tag, so that if we ever have to change a loader's download code,
    // the older replays will still work as expected.

    // TODO: close event substream first

    if (!m_replay) {
        m_port.close();
        checkError(event);
    } else {
        auto replayEvent = m_replay->getNextEvent<CloseConnectionEvent>(m_name);
        if (replayEvent) {
            event.copyIf(replayEvent);
        } else {
            event.set("error", QSerialPort::ResourceError);
        }
    }

    event.record(m_record);
}

void SerialPortConnection::onReadyRead()
{
    {
        // Wait until the replay signaler (if any) has released its lock.
        XmlReplayLock lock(this, m_replay);
    
        // This needs to be recorded before the signal below, since the slot may trigger more events.
        ReadyReadEvent event;
        event.record(m_record);
        
        // Unlocking will queue any subsequent signals.
    }

    // Because clients typically leave this as Qt::AutoConnection, the below emit may
    // execute immediately in this thread, so we have to release the lock before sending
    // the signal.

    // Unlike client-called events, We don't need to handle replay differently here,
    // because the replay will signal this slot just like the serial port.
    emit readyRead();
}

void SerialPortConnection::checkResult(bool ok, XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (ok && error == QSerialPort::NoError) return;
    event.set("error", error);
    if (ok) event.set("ok", ok);  // we don't expect to see this, but we should know if it happens
}

void SerialPortConnection::checkResult(qint64 len, XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (len < 0 || error != QSerialPort::NoError) {
        event.set("error", error);
    }
}

void SerialPortConnection::checkError(XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (error != QSerialPort::NoError) {
        event.set("error", error);
    }
}


// MARK: -
// MARK: SerialPort legacy class

SerialPort::SerialPort()
    : m_conn(nullptr)
{
}

SerialPort::~SerialPort()
{
    if (m_conn) {
        close();
    }
}

void SerialPort::setPortName(const QString &name)
{
    m_portName = name;
}

bool SerialPort::open(QIODevice::OpenMode mode)
{
    Q_ASSERT(!m_conn);
    Q_ASSERT(mode == QSerialPort::ReadWrite);
    m_conn = DeviceConnectionManager::openSerialPortConnection(m_portName);
    if (m_conn) {
        connect(m_conn, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    }
    return m_conn != nullptr;
}

bool SerialPort::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_ASSERT(m_conn);
    return m_conn->setBaudRate(baudRate, directions);
}

bool SerialPort::setDataBits(QSerialPort::DataBits dataBits)
{
    Q_ASSERT(m_conn);
    return m_conn->setDataBits(dataBits);
}

bool SerialPort::setParity(QSerialPort::Parity parity)
{
    Q_ASSERT(m_conn);
    return m_conn->setParity(parity);
}

bool SerialPort::setStopBits(QSerialPort::StopBits stopBits)
{
    Q_ASSERT(m_conn);
    return m_conn->setStopBits(stopBits);
}

bool SerialPort::setFlowControl(QSerialPort::FlowControl flowControl)
{
    Q_ASSERT(m_conn);
    return m_conn->setFlowControl(flowControl);
}

bool SerialPort::clear(QSerialPort::Directions directions)
{
    Q_ASSERT(m_conn);
    return m_conn->clear(directions);
}

qint64 SerialPort::bytesAvailable() const
{
    Q_ASSERT(m_conn);
    return m_conn->bytesAvailable();
}

qint64 SerialPort::read(char *data, qint64 maxSize)
{
    Q_ASSERT(m_conn);
    return m_conn->read(data, maxSize);
}

qint64 SerialPort::write(const char *data, qint64 maxSize)
{
    Q_ASSERT(m_conn);
    return m_conn->write(data, maxSize);
}

bool SerialPort::flush()
{
    Q_ASSERT(m_conn);
    return m_conn->flush();
}

void SerialPort::close()
{
    Q_ASSERT(m_conn);
    disconnect(m_conn, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    delete m_conn;  // this will close the connection
    m_conn = nullptr;
}

void SerialPort::onReadyRead()
{
    emit readyRead();
}
