/* Raw Data Unit Tests
 *
 * Copyright (c) 2021 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "tests/AutoTest.h"

class RawDataTests : public QObject
{
    Q_OBJECT
private slots:
    void testQIODeviceInterface();

    void onAboutToClose();
    void onChannelReadyRead(int);
    void onReadChannelFinished();
    void onReadyRead();
private:
    bool m_aboutToClose;
    int m_channelReadyRead;
    bool m_readChannelFinished;
    bool m_readyRead;
};
DECLARE_TEST(RawDataTests)
