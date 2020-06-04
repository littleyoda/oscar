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
#include <QtSerialPort/QSerialPortInfo>

// TODO: This class may eventually be internal to a DeviceConnection class,
// but for now it is used to provide support for recording and playback of
// serial port connections before refactoring.
class SerialPort : public QSerialPort
{
};

// TODO: This class's functionality will eventually be internal to a
// DeviceConnection class, but for now it is needed to support recording
// and playback of serial port scanning before refactoring.
class SerialPortInfo : public QSerialPortInfo
{
public:
    static QList<SerialPortInfo> availablePorts();
    SerialPortInfo(const SerialPortInfo & other);

protected:
    SerialPortInfo(const QSerialPortInfo & other);
};

#endif // DEVICECONNECTION_H
