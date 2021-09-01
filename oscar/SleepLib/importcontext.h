/* SleepLib Import Context Header
 *
 * Copyright (c) 2021 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef IMPORTCONTEXT_H
#define IMPORTCONTEXT_H

#include "SleepLib/machine_loader.h"


class ImportContext : public QObject
{
    Q_OBJECT
public:
    ImportContext() {}
    virtual ~ImportContext();

    // Loaders will call this directly. It manages the machine's stored set of previously seen messages
    // and will emit an importEncounteredUnexpectedData signal in its dtor if any are new.
    void LogUnexpectedMessage(const QString & message);

signals:
    void importEncounteredUnexpectedData(const MachineInfo & info, QSet<QString> & newMessages);

public:
    virtual bool ShouldIgnoreOldSessions() { return false; }
    virtual QDateTime IgnoreSessionsOlderThan() { return QDateTime(); }
    
    // TODO: Isolate the Machine object from the loader rather than returning it.
    virtual Machine* CreateMachineFromInfo(const MachineInfo & info) = 0;

protected:
    void FlushUnexpectedMessages();

    QMutex m_mutex;
    QSet<QString> m_unexpectedMessages;
    MachineInfo m_machineInfo;
    Machine* m_machine;
};


// ProfileImportContext isolates the loader from Profile and its storage.
class Profile;
class ProfileImportContext : public ImportContext
{
    Q_OBJECT
public:
    ProfileImportContext(Profile* profile);
    virtual ~ProfileImportContext() {};

    virtual bool ShouldIgnoreOldSessions();
    virtual QDateTime IgnoreSessionsOlderThan();

    virtual Machine* CreateMachineFromInfo(const MachineInfo & info);

protected:
    Profile* m_profile;
};


// TODO: Add a TestImportContext that writes the data to YAML for regression tests.


// TODO: Once all loaders support context and UI, move this into the main application
// and refactor its import UI logic into this class.
class ImportUI : public QObject
{
    Q_OBJECT
public:
    ImportUI(Profile* profile);
    virtual ~ImportUI() {}

public slots:
    void onUnexpectedData(const MachineInfo & machine, QSet<QString> & newMessages);
    void onDeviceReportsUsageOnly(const MachineInfo & info);
    void onDeviceIsUntested(const MachineInfo & info);
    void onDeviceIsUnsupported(const MachineInfo & info);

protected:
    Profile* m_profile;
};


#endif // IMPORTCONTEXT_H
