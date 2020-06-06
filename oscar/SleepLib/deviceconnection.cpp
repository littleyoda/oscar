/* Device Connection Class Implementation
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "deviceconnection.h"

SerialPortInfo::SerialPortInfo(const QSerialPortInfo & other)
    : QSerialPortInfo(other)
{
}

SerialPortInfo::SerialPortInfo(const SerialPortInfo & other)
    : QSerialPortInfo(dynamic_cast<const SerialPortInfo &>(other))
{
}

QList<SerialPortInfo> SerialPortInfo::availablePorts()
{
    QList<SerialPortInfo> out;
    for (auto & info : QSerialPortInfo::availablePorts()) {
        out.append(SerialPortInfo(info));
    }
    return out;
}
