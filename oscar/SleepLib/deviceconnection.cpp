/* Device Connection Manager
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "deviceconnection.h"
#include "version.h"
#include <QtSerialPort/QSerialPortInfo>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>


static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}


// MARK: -
// MARK: XML record/playback base classes

/*
 * XML recording base class
 *
 * While this can be used on its own via the public constructors, it is
 * typically used as a base class for a subclasses that handle specific
 * events.
 *
 * A single instance of this class can write a linear sequence of events to
 * XML, either to a string (for testing) or to a file (for production use).
 *
 * Sometimes, however, there is need for certain sequences to be treated as
 * separate, either due to multithreading (such recording as multiple
 * simultaneous connections), or in order to treat a certain excerpt (such
 * as data download that we might wish to archive) separately.
 *
 * These sequences are handled as "substreams" of the parent stream. The
 * parent stream will typically record a substream's open/close or start/
 * stop along with its ID. The substream will be written to a separate XML
 * stream identified by that ID. Substreams are implemented as subclasses of
 * this base class.
 *
 * TODO: At the moment, only file-based substreams are supported. In theory
 * it should be possible to cache string-based substreams and then insert
 * them inline into the parent after the substream-close event is recorded.
 */
class XmlRecorder
{
public:
    static const QString TAG;  // default tag if no subclass

    XmlRecorder(class QFile * file, const QString & tag = XmlRecorder::TAG);  // record XML to the given file
    XmlRecorder(QString & string, const QString & tag = XmlRecorder::TAG);    // record XML to the given string
    virtual ~XmlRecorder();  // write the epilogue and close the recorder
    XmlRecorder* closeSubstream();  // convenience function to close out a substream and return its parent
    inline QXmlStreamWriter & xml() { return *m_xml; }
    inline void lock() { m_mutex.lock(); }
    inline void unlock() { m_mutex.unlock(); }
protected:
    XmlRecorder(XmlRecorder* parent, const QString & id, const QString & tag);  // constructor used by substreams
    QXmlStreamWriter* addSubstream(XmlRecorder* child, const QString & id);     // initialize a child substream, used by above constructor
    const QString m_tag;      // opening/closing tag for this instance
    QFile* m_file;            // nullptr for non-file recordings
    QXmlStreamWriter* m_xml;  // XML output stream
    QMutex m_mutex;           // force one thread at a time to write to m_xml
    XmlRecorder* m_parent;    // parent instance of a substream
    
    virtual void prologue();
    virtual void epilogue();
};
const QString XmlRecorder::TAG = "xmlreplay";

class XmlReplayEvent;

/*
 * XML replay base class
 *
 * A single instance of this class caches events from a previously recorded
 * XML stream, either from a string (for testing) or from a file (for
 * production use).
 *
 * Unlike recording, the replay need not be strictly linear. In fact, the
 * implementation is designed to allow for limited reordering during replay,
 * so that minor changes to code should result in sensible replay until a
 * new recording can be made.
 *
 * There are two aspects to this reordering:
 *
 * First, events can be retrieved (and consumed) in any order, being
 * retrieved by type and ID (and then in order within that type and ID).
 *
 * Second, events that are flagged as random-access (see randomAccess below)
 * will cause the above retrieval to subsequently begin searching on or
 * after the random-access event's timestamp (except for other random-access
 * events, which are always searched from the beginning.)
 *
 * This allow non-stateful events to be replayed arbitrarily, and for
 * stateful events (such as commands sent to a device) to be approximated
 * (where subsequent data received matches the command sent).
 *
 * Furthermore, when events are triggered in the same order as they were
 * during recordering, the above reordering will have no effect, and the
 * original ordering will be replayed identically.
 *
 * See XmlRecorder above for a discussion of substreams.
 */
class XmlReplay
{
public:
    XmlReplay(class QFile * file, const QString & tag = XmlRecorder::TAG);      // replay XML from the given file
    XmlReplay(QXmlStreamReader & xml, const QString & tag = XmlRecorder::TAG);  // replay XML from the given stream
    virtual ~XmlReplay();
    XmlReplay* closeSubstream();  // convenience function to close out a substream and return its parent
    template<class T> inline T* getNextEvent(const QString & id = "");  // typesafe accessor to retrieve and consume the next matching event


protected:
    XmlReplay(XmlReplay* parent, const QString & id, const QString & tag = XmlRecorder::TAG);  // constructor used by substreams
    QXmlStreamReader* findSubstream(XmlReplay* child, const QString & id);                     // initialize a child substream, used by above constructor

    void deserialize(QXmlStreamReader & xml);
    void deserializeEvents(QXmlStreamReader & xml);
    const QString m_tag;  // opening/closing tag for this instance
    QFile* m_file;        // nullptr for non-file replay

    QHash<QString,QHash<QString,QList<XmlReplayEvent*>>> m_eventIndex;  // type and ID-based index into the events, see discussion of reordering above
    QHash<QString,QHash<QString,int>> m_indexPosition;                  // positions at which to begin searching the index, updated by random-access events
    QList<XmlReplayEvent*> m_events;                                    // linear list of all events in their original order

    XmlReplayEvent* getNextEvent(const QString & type, const QString & id = "");
    void seekToTime(const QDateTime & time);

    XmlReplayEvent* m_pendingSignal;  // the signal (if any) that should be replayed as soon as the current event has been processed
    QMutex m_lock;                    // prevent signals from being dispatched while an event is being processed, see XmlReplayLock below
    inline void lock() { m_lock.lock(); }
    inline void unlock() { m_lock.unlock(); }
    void processPendingSignals(const QObject* target);
    friend class XmlReplayLock;

    XmlReplay* m_parent;  // parent instance of a substream
};


/*
 * XML replay event base class
 *
 * This class is used to represent a replayable event. An event is created
 * when performing any replayable action, and then recorded (via record())
 * when appropriate. During replay, an event is retrieved from the XmlReplay
 * instance and its previously recorded result should be returned instead of
 * performing the original action.
 *
 * Subclasses are created as subclasses of the XmlReplayBase template (see
 * below), which handles their factory method and tag registration.
 *
 * Subclasses that should be retrieved by ID as well as type will need to
 * override id() to return the ID to use for indexing.
 *
 * Subclasses that represent signal events (rather than API calls) will need
 * to set their m_signal string to the name of the signal to be emitted, and
 * additionally override signal() if they need to pass parameters with the
 * signal.
 *
 * Subclasses that represent random-access events (see discussion above)
 * will need to override randomAccess() to return true.
 *
 * Subclasses whose XML contains raw hexadecimal data will need to override
 * usesData() to return true. Subclasses whose XML contains other data
 * (such as complex data types) will instead need to override read() and
 * write().
 */
class XmlReplayEvent
{
public:
    XmlReplayEvent();
    virtual ~XmlReplayEvent() = default;
    
    //! \brief Return the XML tag used for this event. Automatically overridden for subclasses by template.
    virtual const QString & tag() const = 0;
    
    //! \brief Return the ID for this event, if applicable. Subclasses should override this if their events should be retrieved by ID.
    virtual const QString id() const { static const QString none(""); return none; }
    
    //! \brief True if this event represents a "random-access" event that should cause subsequent event searches to start after this event's timestamp. Subclasses that represent such a state change should override this method.
    virtual bool randomAccess() const { return false; }

    //! \brief Record this event to the given XML recorder, doing nothing if the recorder is null.
    void record(XmlRecorder* xml) const;

    // Serialize this event to an XML stream.
    friend QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const XmlReplayEvent & event);

    // Deserialize this event's contents from an XML stream. The instance is first created via createInstance() based on the tag.
    friend QXmlStreamReader & operator>>(QXmlStreamReader & xml, XmlReplayEvent & event);
    
    // Write the tag's attributes and contents.
    void writeTag(QXmlStreamWriter & xml) const;

    // Event subclass registration and instance creation
    typedef XmlReplayEvent* (*FactoryMethod)();
    static bool registerClass(const QString & tag, FactoryMethod factory);
    static XmlReplayEvent* createInstance(const QString & tag);

    //! \brief Add the given key/value to the event. This will be written as an XML attribute in the order it added.
    void set(const QString & name, const QString & value);
    //! \brief Add the given key/integer to the event. This will be written as an XML attribute in the order it added.
    void set(const QString & name, qint64 value);
    //! \brief Add the raw data to the event. This will be written in hexadecimal as content of the event's XML tag.
    void setData(const char* data, qint64 length);
    //! \brief Get the value for the given key.
    QString get(const QString & name) const;
    //! \brief Get the raw data for this event.
    QByteArray getData() const;
    //! \brief True if there are no errors in this event, or false if the "error" attribute is set.
    inline bool ok() const { return m_values.contains("error") == false; }
    //! \brief Return a string of this event as an XML tag.
    operator QString() const;

    //! \brief Copy the result from the retrieved replay event (if any) into the current event.
    void copyIf(const XmlReplayEvent* other);
protected:
    //! \brief Copy the timestamp as well as the results. This is necessary for replaying substreams that use the timestamp as part of their ID.
    void copy(const XmlReplayEvent & other);

protected:
    static QHash<QString,FactoryMethod> s_factories;  // registered subclass factory methods, arranged by XML tag

    QDateTime m_time;        // timestamp of event
    XmlReplayEvent* m_next;  // next recorded event, used during replay to trigger signals that automatically fire after an event is processed

    const char* m_signal;    // name of the signal to be emitted for this event, if any
    inline bool isSignal() const { return m_signal != nullptr; }

    //! \brief Send a signal to the target object. Subclasses may override this to send signal arguments.
    virtual void signal(QObject* target);

    QHash<QString,QString> m_values;  // hash of key/value pairs for this event, written as attributes of the XML tag
    QList<QString> m_keys;            // list of keys so that attributes will be written in the order they were set
    QString m_data;                   // hexademical string representing this event's raw data, written as contents of the XML tag

    //! \brief Returns whether this event contains raw data. Defaults to false, so subclasses that use raw data must override this.
    virtual bool usesData() const { return false; }

    //! \brief Write any attributes or content needed specific to event. Subclasses may override this to support complex data types.
    virtual void write(QXmlStreamWriter & xml) const;
    //! \brief Read any attributes or content specific to this event. Subclasses may override this to support complex data types.
    virtual void read(QXmlStreamReader & xml);

    friend class XmlReplay;
};
QHash<QString,XmlReplayEvent::FactoryMethod> XmlReplayEvent::s_factories;

void XmlReplayEvent::set(const QString & name, const QString & value)
{
    if (!m_values.contains(name)) {
        m_keys.append(name);
    }
    m_values[name] = value;
}

void XmlReplayEvent::set(const QString & name, qint64 value)
{
    set(name, QString::number(value));
}

void XmlReplayEvent::setData(const char* data, qint64 length)
{
    Q_ASSERT(usesData() == true);
    QByteArray bytes = QByteArray::fromRawData(data, length);
    m_data = bytes.toHex(' ').toUpper();
}

QString XmlReplayEvent::get(const QString & name) const
{
    if (!m_values.contains(name)) {
        qWarning().noquote() << *this << "missing attribute:" << name;
    }
    return m_values[name];
}

QByteArray XmlReplayEvent::getData() const
{
    Q_ASSERT(usesData() == true);
    if (m_data.isEmpty()) {
        qWarning().noquote() << "replaying event with missing data" << *this;
        QByteArray empty;
        return empty;  // toUtf8() below crashes with an empty string.
    }
    return QByteArray::fromHex(m_data.toUtf8());
}

XmlReplayEvent::operator QString() const
{
    QString out;
    QXmlStreamWriter xml(&out);
    xml << *this;
    return out;
}

void XmlReplayEvent::copyIf(const XmlReplayEvent* other)
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

void XmlReplayEvent::copy(const XmlReplayEvent & other)
{
    copyIf(&other);
    // Copy the timestamp, as it is necessary for replaying substreams that use the timestamp as part of their ID.
    m_time = other.m_time;
}

void XmlReplayEvent::signal(QObject* target)
{
    // Queue the signal so that it won't be processed before the current event returns to its caller.
    // (See XmlReplayLock below.)
    QMetaObject::invokeMethod(target, m_signal, Qt::QueuedConnection);
}

void XmlReplayEvent::write(QXmlStreamWriter & xml) const
{
    // Write key/value pairs as attributes in the order they were set.
    for (auto key : m_keys) {
        xml.writeAttribute(key, m_values[key]);
    }
    if (!m_data.isEmpty()) {
        Q_ASSERT(usesData() == true);
        xml.writeCharacters(m_data);
    }
}

void XmlReplayEvent::read(QXmlStreamReader & xml)
{
    QXmlStreamAttributes attribs = xml.attributes();
    for (auto & attrib : attribs) {
        if (attrib.name() != "time") {    // skip outer timestamp, which is decoded by operator>>
            set(attrib.name().toString(), attrib.value().toString());
        }
    }
    if (usesData()) {
        m_data = xml.readElementText();
    } else {
        xml.skipCurrentElement();
    }
}


/*
 * XML replay lock class
 *
 * An instance of this class should be created on the stack during any replayable
 * event. Exiting scope will release the lock, at which point any signals that
 * need to be replayed will be queued.
 *
 * Has no effect if events are not being replayed.
 */
class XmlReplayLock
{
public:
    //! \brief Temporarily lock the XML replay (if any) until exiting scope, at which point any pending signals will be sent to the specified object.
    XmlReplayLock(const QObject* obj, XmlReplay* replay);
    ~XmlReplayLock();

protected:
    const QObject* m_target;  // target object to receive any pending signals
    XmlReplay* m_replay;      // replay instance, or nullptr if not replaying
};

XmlReplayLock::XmlReplayLock(const QObject* obj, XmlReplay* replay)
    : m_target(obj), m_replay(replay)
{
    if (m_replay) {
        // Prevent any triggered signal events from processing until the triggering lock is released.
        m_replay->lock();
    }
}

XmlReplayLock::~XmlReplayLock()
{
    if (m_replay) {
        m_replay->processPendingSignals(m_target);
        m_replay->unlock();
    }
}


// Derive the filepath for the given substream ID relative to the parent stream.
static QString substreamFilepath(QFile* parent, const QString & id)
{
    Q_ASSERT(parent);
    QFileInfo info(*parent);
    QString path = info.canonicalPath() + QDir::separator() + info.completeBaseName() + "-" + id + ".xml";
    return path;
}


XmlRecorder::XmlRecorder(QFile* stream, const QString & tag)
    : m_tag(tag), m_file(stream), m_xml(new QXmlStreamWriter(stream)), m_parent(nullptr)
{
    prologue();
}

XmlRecorder::XmlRecorder(QString & string, const QString & tag)
    : m_tag(tag), m_file(nullptr), m_xml(new QXmlStreamWriter(&string)), m_parent(nullptr)
{
    prologue();
}

// Protected constructor for substreams
XmlRecorder::XmlRecorder(XmlRecorder* parent, const QString & id, const QString & tag)
    : m_tag(tag), m_file(nullptr), m_xml(nullptr), m_parent(parent)
{
    Q_ASSERT(m_parent);
    m_xml = m_parent->addSubstream(this, id);
    if (m_xml == nullptr) {
        qWarning() << "Not recording" << id;
        static QString null;
        m_xml = new QXmlStreamWriter(&null);
    }

    prologue();
}

// Initialize a child recording substream.
QXmlStreamWriter* XmlRecorder::addSubstream(XmlRecorder* child, const QString & id)
{
    Q_ASSERT(child);
    QXmlStreamWriter* xml = nullptr;

    if (m_file) {
        QString childPath = substreamFilepath(m_file, id);
        child->m_file = new QFile(childPath);
        if (child->m_file->open(QIODevice::WriteOnly | QIODevice::Append)) {
            xml = new QXmlStreamWriter(child->m_file);
            qDebug() << "Recording to" << childPath;
        } else {
            qWarning() << "Unable to open" << childPath << "for writing";
        }
    } else {
        qWarning() << "String-based substreams are not supported";
        // Maybe some day support string-based substreams:
        // - parent passes string to child
        // - on connectionClosed, parent asks recorder to flush string to stream
    }
    
    return xml;
}

XmlRecorder::~XmlRecorder()
{
    epilogue();
    delete m_xml;
    // File substreams manage their own file.
    if (m_parent && m_file) {
        delete m_file;
    }
}

// Close out a substream and return its parent.
XmlRecorder* XmlRecorder::closeSubstream()
{
    auto parent = m_parent;
    delete this;
    return parent;
}

void XmlRecorder::prologue()
{
    Q_ASSERT(m_xml);
    m_xml->setAutoFormatting(true);
    m_xml->setAutoFormattingIndent(2);
    m_xml->writeStartElement(m_tag);  // open enclosing tag
}

void XmlRecorder::epilogue()
{
    Q_ASSERT(m_xml);
    m_xml->writeEndElement();  // close enclosing tag
}


XmlReplay::XmlReplay(QFile* file, const QString & tag)
    : m_tag(tag), m_file(file), m_pendingSignal(nullptr), m_parent(nullptr)
{
    Q_ASSERT(file);
    QFileInfo info(*file);
    qDebug() << "Replaying from" << info.canonicalFilePath();

    QXmlStreamReader xml(file);
    deserialize(xml);
}

XmlReplay::XmlReplay(QXmlStreamReader & xml, const QString & tag)
    : m_tag(tag), m_file(nullptr), m_pendingSignal(nullptr), m_parent(nullptr)
{
    deserialize(xml);
}

// Protected constructor for substreams
XmlReplay::XmlReplay(XmlReplay* parent, const QString & id, const QString & tag)
    : m_tag(tag), m_file(nullptr), m_pendingSignal(nullptr), m_parent(parent)
{
    Q_ASSERT(m_parent);

    auto xml = m_parent->findSubstream(this, id);
    if (xml) {
        deserialize(*xml);
        delete xml;
    } else {
        qWarning() << "Not replaying" << id;
    }
}

// Initialize a child replay substream.
QXmlStreamReader* XmlReplay::findSubstream(XmlReplay* child, const QString & id)
{
    Q_ASSERT(child);
    QXmlStreamReader* xml = nullptr;

    if (m_file) {
        QString childPath = substreamFilepath(m_file, id);
        child->m_file = new QFile(childPath);
        if (child->m_file->open(QIODevice::ReadOnly)) {
            xml = new QXmlStreamReader(child->m_file);
            qDebug() << "Replaying from" << childPath;
        } else {
            qWarning() << "Unable to open" << childPath << "for reading";
        }
    } else {
        qWarning() << "String-based substreams are not supported";
        // Maybe some day support string-based substreams:
        // - when deserializing, use e.g. ConnectionEvents to cache the substream strings
        // - then return a QXmlStreamReader here using that string
    }
 
    return xml;
}

XmlReplay::~XmlReplay()
{
    for (auto event : m_events) {
        delete event;
    }
    // File substreams manage their own file.
    if (m_parent && m_file) {
        delete m_file;
    }
}

// Close out a substream and return its parent.
XmlReplay* XmlReplay::closeSubstream()
{
    auto parent = m_parent;
    delete this;
    return parent;
}

void XmlReplay::deserialize(QXmlStreamReader & xml)
{
    if (xml.readNextStartElement()) {
        if (xml.name() == m_tag) {
            deserializeEvents(xml);
        } else {
            qWarning() << "unexpected payload in replay XML:" << xml.name();
            xml.skipCurrentElement();
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

// Queue any pending signals when a replay lock is released.
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
    }
}

// Update the positions at which to begin searching the index, so that only events on or after the given time are returned by getNextEvent.
void XmlReplay::seekToTime(const QDateTime & time)
{
    for (auto & type : m_eventIndex.keys()) {
        for (auto & key : m_eventIndex[type].keys()) {
            // Find the index of the first event on or after the given time.
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

// Find and return the next event of the given type with the given ID, or nullptr if no more events match.
XmlReplayEvent* XmlReplay::getNextEvent(const QString & type, const QString & id)
{
    XmlReplayEvent* event = nullptr;
    
    // Event handlers should always be wrapped in an XmlReplayLock, so warn if that's not the case.
    if (m_lock.tryLock()) {
        qWarning() << "XML replay" << type << "object not locked by event handler!";
        m_lock.unlock();
    }

    // Search the index for the next matching event (if any).
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
    
    // If this is a random-access event, we need to update the index positions for all non-random-access events.
    if (event && event->randomAccess()) {
        seekToTime(event->m_time);
    }

    // If the event following this one is a signal (that replay needs to trigger), save it as pending
    // so that it can be emitted when the replay lock for this event is released.
    if (event && event->m_next && event->m_next->isSignal()) {
        Q_ASSERT(m_pendingSignal == nullptr);  // if this ever fails, we may need m_pendingSignal to be a list
        m_pendingSignal = event->m_next;
    }

    return event;
}

// Public, typesafe wrapper for getNextEvent above.
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

void XmlReplayEvent::record(XmlRecorder* writer) const
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

void XmlReplayEvent::writeTag(QXmlStreamWriter & xml) const
{
    QDateTime time = m_time.toOffsetFromUtc(m_time.offsetFromUtc());  // force display of UTC offset
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    // TODO: Can we please deprecate support for Qt older than 5.9?
    QString timestamp = time.toString(Qt::ISODate);
#else
    QString timestamp = time.toString(Qt::ISODateWithMs);
#endif
    xml.writeAttribute("time", timestamp);

    // Call this event's overridable write method.
    write(xml);
}

QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const XmlReplayEvent & event)
{
    xml.writeStartElement(event.tag());
    event.writeTag(xml);
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
    
    // Call this event's overridable read method.
    event.read(xml);
    return xml;
}

// Convenience template for serializing QLists to XML
template<typename T> QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const QList<T> & list)
{
    for (auto & item : list) {
        xml << item;
    }
    return xml;
}

// Convenience template for deserializing QLists from XML
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

/*
 * Intermediate parent class of concrete event subclasses.
 *
 * We use this extra CRTP templating so that concrete event subclasses
 * require as little code as possible:
 *
 * The subclass's tag and factory method are automatically generated by this
 * template.
 */
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

/*
 * Macro to define an XmlReplayEvent subclass's tag and automatically
 * register the subclass at global-initialization time, before main()
 */
#define REGISTER_XMLREPLAYEVENT(tag, type) \
template<> const QString XmlReplayBase<type>::TAG = tag; \
template<> const bool XmlReplayBase<type>::registered = XmlReplayEvent::registerClass(XmlReplayBase<type>::TAG, XmlReplayBase<type>::createInstance);


// MARK: -
// MARK: Device connection manager

/*
 * DeviceRecorder/DeviceReplay subclasses of XmlRecorder/XmlReplay
 *
 * Used by DeviceConnectionManager to record its activity, such as
 * port scanning and connection opening/closing.
 */
class DeviceRecorder : public XmlRecorder
{
public:
    static const QString TAG;

    DeviceRecorder(class QFile * file) : XmlRecorder(file, DeviceRecorder::TAG) { m_xml->writeAttribute("oscar", getVersion().toString()); }
    DeviceRecorder(QString & string) : XmlRecorder(string, DeviceRecorder::TAG) { m_xml->writeAttribute("oscar", getVersion().toString()); }
};
const QString DeviceRecorder::TAG = "devicereplay";

class DeviceReplay : public XmlReplay
{
public:
    DeviceReplay(class QFile * file) : XmlReplay(file, DeviceRecorder::TAG) {}
    DeviceReplay(QXmlStreamReader & xml) : XmlReplay(xml, DeviceRecorder::TAG) {}
};

void DeviceConnectionManager::record(QFile* stream)
{
    if (m_record) {
        delete m_record;
    }
    if (stream) {
        m_record = new DeviceRecorder(stream);
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
    m_record = new DeviceRecorder(string);
}

void DeviceConnectionManager::replay(const QString & string)
{
    QXmlStreamReader xml(string);
    reset();
    if (m_replay) {
        delete m_replay;
    }
    m_replay = new DeviceReplay(xml);
}

void DeviceConnectionManager::replay(QFile* file)
{
    reset();
    if (m_replay) {
        delete m_replay;
    }
    if (file) {
        m_replay = new DeviceReplay(file);
    } else {
        // nullptr turns off replay
        m_replay = nullptr;
    }
}


// Return singleton instance of DeviceConnectionManager, creating it if necessary.
inline DeviceConnectionManager & DeviceConnectionManager::getInstance()
{
    static DeviceConnectionManager instance;
    return instance;
}

// Protected constructor
DeviceConnectionManager::DeviceConnectionManager()
    : m_record(nullptr), m_replay(nullptr)
{
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

    // Recording/replay (if any) is handled by the connection.
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
    return conn;
}

// Called by connections to deregister themselves.
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

// Since there are relatively few connection types, don't bother with a CRTP
// parent class. Instead, this macro defines the factory method, and the
// subclass will need to declare createInstance() and TYPE manually.
#define REGISTER_DEVICECONNECTION(type, T) \
const QString T::TYPE = type; \
const bool T::registered = DeviceConnectionManager::registerClass(T::TYPE, T::createInstance); \
DeviceConnection* T::createInstance(const QString & name, XmlRecorder* record, XmlReplay* replay) { return static_cast<DeviceConnection*>(new T(name, record, replay)); }

// MARK: -
// MARK: Device manager events

// See XmlReplayEvent discussion of complex data types above.
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
        // Query the actual hardware present.
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

/*
 * This class is both a drop-in replacement for QSerialPortInfo and
 * supports XML serialization for the GetAvailableSerialPortsEvent above.
 */

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

// TODO: This method is a temporary wrapper that mimics the QSerialPortInfo interface until we begin refactoring.
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

/*
 * Event recorded in the Device Connection Manager XML stream that indicates
 * a connection was opened (or attempted). On success, a ConnectionEvent
 * (see below) will begin the connection's substream.
 */
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

/*
 * Event created when a connection is successfully opened, used as the
 * enclosing tag for the connection substream.
 */
class ConnectionEvent : public XmlReplayBase<ConnectionEvent>
{
public:
    ConnectionEvent() { Q_ASSERT(false); }  // Implement if we ever support string-based substreams
    ConnectionEvent(const OpenConnectionEvent & trigger)
    {
        copy(trigger);
    }
    virtual const QString id() const
    {
        QString time = m_time.toString("yyyyMMdd.HHmmss.zzz");
        return m_values["name"] + "-" + time;
    }
};
REGISTER_XMLREPLAYEVENT("connection", ConnectionEvent);

/*
 * ConnectionRecorder/ConnectionReplay subclasses of XmlRecorder/XmlReplay
 *
 * Used by DeviceConnection subclasses to record their activity, such as
 * configuration and data sent and received.
 */
class ConnectionRecorder : public XmlRecorder
{
public:
    ConnectionRecorder(XmlRecorder* parent, const ConnectionEvent& event) : XmlRecorder(parent, event.id(), event.tag())
    {
        Q_ASSERT(m_xml);
        event.writeTag(*m_xml);
        m_xml->writeAttribute("oscar", getVersion().toString());
    }
};

class ConnectionReplay : public XmlReplay
{
public:
    ConnectionReplay(XmlReplay* parent, const ConnectionEvent& event) : XmlReplay(parent, event.id(), event.tag()) {}
};


// Device connection base class
DeviceConnection::DeviceConnection(const QString & name, XmlRecorder* record, XmlReplay* replay)
    : m_name(name), m_record(record), m_replay(replay), m_opened(false)
{
}

DeviceConnection::~DeviceConnection()
{
}

/*
 * Generic get/set events
 */
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

/*
 * Event recorded in the Device Connection Manager XML stream when a
 * open connection is closed. This is the complement to a successful
 * OpenConnectionEvent (see above), and does not appear when the connection
 * failed to open.
 */
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

/*
 * Event representing data received from a device
 *
 * The data is stored as hexadecimal data in the XML tag's contents.
 */
class ReceiveDataEvent : public XmlReplayBase<ReceiveDataEvent>
{
    virtual bool usesData() const { return true; }
};
REGISTER_XMLREPLAYEVENT("rx", ReceiveDataEvent);

/*
 * Event representing data sent to a device
 *
 * The data is stored as hexadecimal data in the XML tag's contents.
 *
 * These events are random-access events (see discussion above), which cause
 * subsequent event retrieval to begin searching after the transmission
 * event.
 *
 * Since the data sent is used as the ID for these events, we can treat
 * these like distinct "commands" that that elicit a deterministic response,
 * which can be replayed independently of other events if desired.
 *
 * Of course, for any device that has more complex internal state (where
 * responses to multiple transmissions of a particular "command" depend
 * on some intervening event), this reordering will not be accurate.
 *
 * But the intent is that some small changes to client code should still
 * work with existing recordings before requiring creation of new ones.
 */
class TransmitDataEvent : public XmlReplayBase<TransmitDataEvent>
{
    virtual bool usesData() const { return true; }
public:
    virtual const QString id() const { return m_data; }
    virtual bool randomAccess() const { return true; }
};
REGISTER_XMLREPLAYEVENT("tx", TransmitDataEvent);

/*
 * Event representing a "readyRead" signal emitted by a physical device.
 *
 * These events are marked as "signal" events (see discussion of m_signal
 * above) so that connections will automatically send them to clients when
 * the preceding event (API call) is processed.
 */
class ReadyReadEvent : public XmlReplayBase<ReadyReadEvent>
{
public:
    // Use the connection's slot that receives readyRead signals.
    ReadyReadEvent() { m_signal = "onReadyRead"; }
};
REGISTER_XMLREPLAYEVENT("readyRead", ReadyReadEvent);


// MARK: -
// MARK: Serial port connection

/*
 * Serial port connection class
 *
 * This class wraps calls to an underlying QSerialPort with the logic
 * necessary to record and replay arbitrary serial port activity.
 * (or, at least, the serial port functionality currently used by OSCAR).
 *
 * Clients obtain a connection instance via DeviceConnectionManager::openConnection()
 * or openSerialPortConnection (for convenience, if they require a serial port).
 */

REGISTER_DEVICECONNECTION("serial", SerialPortConnection);

SerialPortConnection::SerialPortConnection(const QString & name, XmlRecorder* record, XmlReplay* replay)
    : DeviceConnection(name, record, replay)
{
    connect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

SerialPortConnection::~SerialPortConnection()
{
    // This will only be false if the connection failed to open immediately after construction.
    if (m_opened) {
        close();
        DeviceConnectionManager::getInstance().connectionClosed(this);
    }
    disconnect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

/*
 * Try to open the physical port (or replay a previous attempt), returning
 * false if the port was not opened.
 *
 * DeviceConnectionManager::openConnection calls this immediately after
 * creating a connection instance, and will only return open connections
 * to clients.
 */
bool SerialPortConnection::open()
{
    if (m_opened) {
        qWarning() << "serial connection to" << m_name << "already opened";
        return false;
    }
    XmlReplayLock lock(this, m_replay);
    OpenConnectionEvent* replayEvent = nullptr;
    OpenConnectionEvent event("serial", m_name);

    if (!m_replay) {
        // TODO: move this into SerialPortConnection::openDevice() and move
        // the rest of the logic up to DeviceConnection::open().
        m_port.setPortName(m_name);
        checkResult(m_port.open(QSerialPort::ReadWrite), event);
    } else {
        replayEvent = m_replay->getNextEvent<OpenConnectionEvent>(event.id());
        if (replayEvent) {
            event.copyIf(replayEvent);
        } else {
            event.set("error", QSerialPort::DeviceNotFoundError);
        }
    }

    event.record(m_record);
    m_opened = event.ok();

    if (m_opened) {
        // open a connection substream for connection events
        if (m_record) {
            ConnectionEvent connEvent(event);
            m_record = new ConnectionRecorder(m_record, connEvent);
        }
        if (m_replay) {
            Q_ASSERT(replayEvent);
            ConnectionEvent connEvent(*replayEvent);  // we need to use the replay's timestamp to find the referenced substream
            m_replay = new ConnectionReplay(m_replay, connEvent);
        }
    }

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
            event.set("error", QSerialPort::WriteError);
        }

        bool ok;
        len = event.get("len").toLong(&ok);
        // No need to copy any data, since the event already contains it.
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
    if (m_opened) {
        // close event substream first
        if (m_record) {
            m_record = m_record->closeSubstream();
        }
        if (m_replay) {
            m_replay = m_replay->closeSubstream();
        }
    }

    XmlReplayLock lock(this, m_replay);
    CloseConnectionEvent event("serial", m_name);

    // TODO: We'll also need to include a loader ID and stream version number
    // in the "connection" tag, so that if we ever have to change a loader's download code,
    // the older replays will still work as expected.
    // NOTE: This may only be required for downloads rather than all connections.

    if (!m_replay) {
        // TODO: move this into SerialPortConnection::closeDevice() and move
        // the remaining logic up to DeviceConnection::close().
        m_port.close();
        checkError(event);
    } else {
        auto replayEvent = m_replay->getNextEvent<CloseConnectionEvent>(event.id());
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

// Check the boolean returned by a serial port call and the port's error status, and update the event accordingly.
void SerialPortConnection::checkResult(bool ok, XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (ok && error == QSerialPort::NoError) return;
    event.set("error", error);
    if (ok) event.set("ok", ok);  // we don't expect to see this, but we should know if it happens
}

// Check the length returned by a serial port call and the port's error status, and update the event accordingly.
void SerialPortConnection::checkResult(qint64 len, XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (len < 0 || error != QSerialPort::NoError) {
        event.set("error", error);
    }
}

// Check the port's error status, and update the event accordingly.
void SerialPortConnection::checkError(XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (error != QSerialPort::NoError) {
        event.set("error", error);
    }
}


// MARK: -
// MARK: SerialPort legacy class

/*
 * SerialPort drop-in replacement for QSerialPort
 *
 * This class mimics the interface of QSerialPort for client code, while
 * using DeviceConnectionManager to open the SerialPortConnection, allowing
 * for transparent recording and replay.
 */

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
        // Listen for readyRead events from the connection so that we can relay them to the client.
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
    // Relay readyRead events from the connection on to the client.
    emit readyRead();
}
