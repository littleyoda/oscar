/* EDF Parser Implementation
 *
 * Copyright (c) 2019 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMutexLocker>
#ifdef _MSC_VER
#include <QtZlib/zlib.h>
#else
#include <zlib.h>
#endif

#include "edfparser.h"

EDFParser::EDFParser(QString name)
{
    filesize = 0;
    datasize = 0;
    signalPtr = nullptr;
    hdrPtr = nullptr;
    fileData.clear();
    if (!name.isEmpty())
        Open(name);
}
EDFParser::~EDFParser()
{
    for (auto & s : edfsignals) {
        if (s.value)  
            delete [] s.value;
    }
}

bool EDFParser::Open(const QString & name)
{
    if (hdrPtr != nullptr) {
        qWarning() << "EDFParser::Open() called with file already open " << name;
        sleep(1);
        return false;
    }
    QFile fi(name);
    if (!fi.open(QFile::ReadOnly)) {
        qDebug() << "EDFParser::Open() Couldn't open file " << name;
        sleep(1);
        return false;
    }
    if (name.endsWith(STR_ext_gz)) {
        fileData = gUncompress(fi.readAll()); // Open and decompress file
    } else {
        fileData = fi.readAll(); // Open and read uncompressed file
    }
    fi.close();
    if (fileData.size() <= EDFHeaderSize) {
        qDebug() << "EDFParser::Open() File too short " << name;
        sleep(1);
        return false;
    }
    hdrPtr = (EDFHeaderRaw *)fileData.constData();
    signalPtr = (char *)fileData.constData() + EDFHeaderSize;
    filename = name;
    filesize = fileData.size();
    datasize = filesize - EDFHeaderSize;
    pos = 0;
    return true;
}

bool EDFParser::Parse()
{
    bool ok;

    if (hdrPtr == nullptr) {
        qWarning() << "EDFParser::Parse() called without valid EDF data " << filename;
        sleep(1);
        return false;
    }

    eof = false;
    edfHdr.version = QString::fromLatin1(hdrPtr->version, 8).toLong(&ok);
    if (!ok) {
        qWarning() << "EDFParser::Parser() Bad Version " << filename;
        sleep(1);
        return false;
    }

    //patientident=QString::fromLatin1(header.patientident,80);
    edfHdr.recordingident = QString::fromLatin1(hdrPtr->recordingident, 80); // Serial number is in here..
    edfHdr.startdate_orig = QDateTime::fromString(QString::fromLatin1(hdrPtr->datetime, 16), "dd.MM.yyHH.mm.ss");
    edfHdr.num_header_bytes = QString::fromLatin1(hdrPtr->num_header_bytes, 8).toLong(&ok);
    if (!ok) {
        qWarning() << "EDFParser::Parde() Bad header byte count " << filename;
        sleep(1);
        return false;
    }
    edfHdr.reserved44=QString::fromLatin1(hdrPtr->reserved, 44);
    edfHdr.num_data_records = QString::fromLatin1(hdrPtr->num_data_records, 8).toLong(&ok);
    if (!ok) {
        qWarning() << "EDFParser::Parse() Bad data record count " << filename;
        sleep(1);
        return false;
    }
    edfHdr.duration_Seconds = QString::fromLatin1(hdrPtr->dur_data_records, 8).toDouble(&ok);
    if (!ok) {
        qWarning() << "EDFParser::Parse() Bad duration " << filename;
        sleep(1);
        return false;
    }
    edfHdr.num_signals = QString::fromLatin1(hdrPtr->num_signals, 4).toLong(&ok);
    if (!ok) {
        qWarning() << "EDFParser::Parse() Bad number of signals " << filename;
        sleep(1);
        return false;
    }

    // Initialize fixed-size signal list.
    edfsignals.resize(edfHdr.num_signals);

    // Now copy all the Signal descriptives into edfsignals
    for (auto & sig : edfsignals) {
        sig.value = nullptr;
        sig.label = Read(16);

        signal_labels.push_back(sig.label);
        signalList[sig.label].push_back(&sig);
        if (eof) {
            qWarning() << "EDFParser::Parse() Early end of file " << filename;
            sleep(1);
            return false;
        }
    }
    for (auto & sig : edfsignals) { 
        sig.transducer_type = Read(80); 
    }
    for (auto & sig : edfsignals) { 
        sig.physical_dimension = Read(8); 
    }
    for (auto & sig : edfsignals) { 
        sig.physical_minimum = Read(8).toDouble(&ok); 
    }
    for (auto & sig : edfsignals) { 
        sig.physical_maximum = Read(8).toDouble(&ok); 
    }
    for (auto & sig : edfsignals) { 
        sig.digital_minimum = Read(8).toDouble(&ok); 
    }
    for (auto & sig : edfsignals) { 
        sig.digital_maximum = Read(8).toDouble(&ok);
        sig.gain = (sig.physical_maximum - sig.physical_minimum) / (sig.digital_maximum - sig.digital_minimum);
        sig.offset = 0;
    }
    for (auto & sig : edfsignals) { 
        sig.prefiltering = Read(80); 
    }
    for (auto & sig : edfsignals) { 
        sig.nr = Read(8).toLong(&ok); 
    }
    for (auto & sig : edfsignals) { 
        sig.reserved = Read(32); 
    }

    // could do it earlier, but it won't crash from > EOF Reads
    if (eof) {
        qWarning() << "EDFParser::Parse() Early end of file " << filename;
        sleep(1);
        return false;
    }

    // Now check the file isn't truncated before allocating space for the values
    long allocsize = 0;
    for (auto & sig : edfsignals) {
        if (edfHdr.num_data_records > 0) {
            allocsize += sig.nr * edfHdr.num_data_records * 2;
        }
    }
    if (allocsize > (datasize - pos)) {
        // Space required more than the remainder left to read,
        // so abort and let the user clean up the corrupted file themselves
        qWarning() << "EDFParser::Parse(): " << filename << " is too short!";
        sleep(1);
        return false;
    }

    // allocate the arrays for the signal values
    for (auto & sig : edfsignals) {
        long recs = sig.nr * edfHdr.num_data_records;
        if (edfHdr.num_data_records <= 0) {
            sig.value = nullptr;
            continue;
        }
        sig.value = new qint16 [recs];
        sig.pos = 0;
    }
    for (int x = 0; x < edfHdr.num_data_records; x++) {
        for (auto & sig : edfsignals) {
#ifdef Q_LITTLE_ENDIAN
            // Intel x86, etc..
            memcpy((char *)&sig.value[sig.pos], (char *)&signalPtr[pos], sig.nr * 2);
            sig.pos += sig.nr;
            pos += sig.nr * 2;
#else
            // Big endian safe
            for (int j=0;j<sig.nr;j++) {
                qint16 t=Read16();
                sig.value[sig.pos++]=t;
            }
#endif
        }
    }

    // Now massage some stuff into OSCAR's layout
    int snp = edfHdr.recordingident.indexOf("SRN=");
    serialnumber.clear();

    for (int i = snp + 4; i < edfHdr.recordingident.length(); i++) {
        if (edfHdr.recordingident[i] == ' ') {
            break;
        }
        serialnumber += edfHdr.recordingident[i];
    }

    QDate d2 = edfHdr.startdate_orig.date();
    if (d2.year() < 2000) {
        d2.setDate(d2.year() + 100, d2.month(), d2.day());
        edfHdr.startdate_orig.setDate(d2);
    }

    if (!edfHdr.startdate_orig.isValid()) {
        qDebug() << "Invalid date time retreieved parsing EDF File " << filename;
        sleep(1);
        return false;
    }

    startdate = qint64(edfHdr.startdate_orig.toTime_t()) * 1000LL;
    //startdate-=timezoneOffset();
    if (startdate == 0) {
        qDebug() << "Invalid startdate = 0 in EDF File " << filename;
        sleep(1);
        return false;
    }

    dur_data_record = (edfHdr.duration_Seconds * 1000.0L);

    enddate = startdate + dur_data_record * qint64(edfHdr.num_data_records);

    return true;
}

// Read a 16 bits integer
qint16 EDFParser::Read16()
{
    if ((pos + 2) > datasize) {
        eof = true;
        return 0;
    }
#ifdef Q_LITTLE_ENDIAN // Intel, etc...
    qint16 res = *(qint16 *)&signalPtr[pos];
#else // ARM, PPC, etc..
    qint16 res = quint8(signalPtr[pos]) | (qint8(signalPtr[pos+1]) << 8);
#endif
    pos += 2;
    return res;
}

QString EDFParser::Read(unsigned n)
{
    if ((pos + long(n)) > datasize) {
        eof = true;
        return QString();
    }
    QByteArray buf(&signalPtr[pos], n);
    pos+=n;
    return buf.trimmed();
}

EDFSignal *EDFParser::lookupLabel(const QString & name, int index)
{
    auto it = signalList.find(name);
    if (it == signalList.end()) 
        return nullptr;

    if (index >= it.value().size()) 
        return nullptr;

    return it.value()[index];
}

