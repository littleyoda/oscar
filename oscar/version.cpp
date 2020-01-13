/* Version management
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "version.h"
#include "git_info.h"
#include "SleepLib/common.h"

const int major_version = 1;   // incompatible API changes
const int minor_version = 1;   // new features that don't break things
const int revision_number = 0; // bugfixes, revisions
const QString ReleaseStatus = "beta"; // testing/nightly/unstable, beta/untamed, rc/almost, r/stable
#include "build_number.h"

const QString VersionString = QString("%1.%2.%3-%4-%5").arg(major_version).arg(minor_version).arg(revision_number).arg(ReleaseStatus).arg(build_number);
const QString ShortVersionString = QString("%1.%2.%3").arg(major_version).arg(minor_version).arg(revision_number);

#ifdef Q_OS_MAC
const QString PlatformString = "MacOSX";
#elif defined(Q_OS_WIN32)
const QString PlatformString = "Win32";
#elif defined(Q_OS_WIN64)
const QString PlatformString = "Win64";
#elif defined(Q_OS_LINUX)
const QString PlatformString = "Linux";
#elif defined(Q_OS_HAIKU)
const QString PlatformString = "Haiku";
#endif


const QString & gitRevision()
{
    return GIT_REVISION;
}
const QString & gitBranch()
{
    return GIT_BRANCH;
}

QString getBranchVersion()
{
    QString version = STR_TR_AppVersion;

    if (!((ReleaseStatus.compare("r", Qt::CaseInsensitive)==0) ||
       (ReleaseStatus.compare("release", Qt::CaseInsensitive)==0)))
        version += " ("+GIT_REVISION + ")";

    if (GIT_BRANCH != "master") {
        version += " [Branch: " + GIT_BRANCH + "]";
    }

#ifdef BROKEN_OPENGL_BUILD
    version += " ["+CSTR_GFX_BrokenGL+"]";
#endif
    return version;
}


int checkVersionStatus(QString statusstr)
{
    bool ok;
    // because Qt Install Framework is dumb and doesn't handle beta/release strings in version numbers,
    // so we store them numerically instead
    int v =statusstr.toInt(&ok);
    if (ok) {
        return v;
    }

    if ((statusstr.compare("testing", Qt::CaseInsensitive) == 0) || (statusstr.compare("unstable", Qt::CaseInsensitive) == 0)) return 0;
    else if ((statusstr.compare("beta", Qt::CaseInsensitive) == 0) || (statusstr.compare("untamed", Qt::CaseInsensitive) == 0)) return 1;
    else if ((statusstr.compare("rc", Qt::CaseInsensitive) == 0)  || (statusstr.compare("almost", Qt::CaseInsensitive) == 0)) return 2;
    else if ((statusstr.compare("r", Qt::CaseInsensitive) == 0)  || (statusstr.compare("stable", Qt::CaseInsensitive) == 0)) return 3;

    // anything else is considered a test build
    return 0;
}
struct VersionStruct {
    short major;
    short minor;
    short revision;
    short status;
    short build;
};

VersionStruct parseVersion(QString versionstring)
{
    static VersionStruct version;

    QStringList parts = versionstring.split(".");
    bool ok, dodgy = false;

    if (parts.size() < 3) dodgy = true;

    short major = parts[0].toInt(&ok);
    if (!ok) dodgy = true;

    short minor = parts[1].toInt(&ok);
    if (!ok) dodgy = true;

    QStringList patchver = parts[2].split("-");
    if (patchver.size() < 3) dodgy = true;

    short rev = patchver[0].toInt(&ok);
    if (!ok) dodgy = true;

    short build = patchver[2].toInt(&ok);
    if (!ok) dodgy = true;

    int status = checkVersionStatus(patchver[1]);

    if (!dodgy) {
        version.major = major;
        version.minor = minor;
        version.revision = rev;
        version.status = status;
        version.build = build;
    }
    return version;
}


// Compare supplied version string with current version
// < 0 = this one is newer or version supplied is dodgy, 0 = same, and > 0 there is a newer version
int compareVersion(const QString & version)
{
    // v1.0.0-beta-2
    QStringList parts = version.split(".");
    bool ok;

    if (parts.size() < 3) {
        // dodgy version string supplied.
        return -1;
    }

    int major = parts[0].toInt(&ok);
    if (!ok) return -1;

    int minor = parts[1].toInt(&ok);
    if (!ok) return -1;

    if (major > major_version) {
        return 1;
    } else if (major < major_version) {
        return -1;
    }

    if (minor > minor_version) {
        return 1;
    } else if (minor < minor_version) {
        return -1;
    }

    int build_index = 1;
    int build = 0;
    int status = 0;
    QStringList patchver = parts[2].split("-");
    if (patchver.size() >= 3) {
        build_index = 2;
        status = checkVersionStatus(patchver[1]);

    } else if (patchver.size() < 2) {
        return -1;
        // dodgy version string supplied.
    }

    int rev = patchver[0].toInt(&ok);
    if (!ok) return -1;
    if (rev > revision_number) {
        return 1;
    } else if (rev < revision_number) {
        return -1;
    }


    build = patchver[build_index].toInt(&ok);
    if (!ok) return -1;

    int rstatus = checkVersionStatus(ReleaseStatus);

    if (patchver.size() == 3) {
        // read it if it's actually present.
    }

    if (status > rstatus) {
        return 1;
    } else if (status < rstatus) {
        return -1;
    }

    if (build > build_number) {
        return 1;
    } else if (build < build_number) {
        return -1;
    }

    // Versions match
    return 0;
}

