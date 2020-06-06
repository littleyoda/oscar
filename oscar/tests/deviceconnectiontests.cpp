/* Device Connection Unit Tests
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "deviceconnectiontests.h"
#include "SleepLib/deviceconnection.h"

void DeviceConnectionTests::testSerialPortInfoSerialization()
{
    QString serialized;
    
    // With VID and PID
    const QString tag = R"(<serial portName="cu.SLAB_USBtoUART" systemLocation="/dev/cu.SLAB_USBtoUART" description="CP210x USB to UART Bridge Controller" manufacturer="Silicon Labs" serialNumber="0001" vendorIdentifier="0x10C4" productIdentifier="0xEA60"/>)";
    SerialPortInfo info = SerialPortInfo(tag);
    Q_ASSERT(info.isNull() == false);
    Q_ASSERT(info.portName() == "cu.SLAB_USBtoUART");
    Q_ASSERT(info.systemLocation() == "/dev/cu.SLAB_USBtoUART");
    Q_ASSERT(info.description() == "CP210x USB to UART Bridge Controller");
    Q_ASSERT(info.manufacturer() == "Silicon Labs");
    Q_ASSERT(info.serialNumber() == "0001");
    Q_ASSERT(info.hasVendorIdentifier());
    Q_ASSERT(info.hasProductIdentifier());
    Q_ASSERT(info.vendorIdentifier() == 0x10C4);
    Q_ASSERT(info.productIdentifier() == 0xEA60);
    serialized = info;
    Q_ASSERT(serialized == tag);
    
    // Without VID or PID
    const QString tag2 = R"(<serial portName="cu.Bluetooth-Incoming-Port" systemLocation="/dev/cu.Bluetooth-Incoming-Port" description="incoming port - Bluetooth-Incoming-Port" manufacturer="" serialNumber=""/>)";
    SerialPortInfo info2 = SerialPortInfo(tag2);
    Q_ASSERT(info2.isNull() == false);
    Q_ASSERT(info2.portName() == "cu.Bluetooth-Incoming-Port");
    Q_ASSERT(info2.systemLocation() == "/dev/cu.Bluetooth-Incoming-Port");
    Q_ASSERT(info2.description() == "incoming port - Bluetooth-Incoming-Port");
    Q_ASSERT(info2.manufacturer() == "");
    Q_ASSERT(info2.serialNumber() == "");
    Q_ASSERT(info2.hasVendorIdentifier() == false);
    Q_ASSERT(info2.hasProductIdentifier() == false);
    serialized = info2;
    Q_ASSERT(serialized == tag2);

    // Empty
    const QString tag3 = R"(<serial/>)";
    SerialPortInfo info3 = SerialPortInfo(tag3);
    Q_ASSERT(info3.isNull() == true);
    serialized = info3;
    Q_ASSERT(serialized == tag3);
}

void DeviceConnectionTests::testSerialPortScanning()
{
    QString string;
    QXmlStreamWriter xml(&string);
    xml.setAutoFormatting(true);

    DeviceConnectionManager::Record(&xml);
    SerialPortInfo::availablePorts();
    SerialPortInfo::availablePorts();
    DeviceConnectionManager::Record(nullptr);

    qDebug().noquote() << string;
}
