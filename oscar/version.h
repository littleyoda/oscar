/* Version.h
 *
 * Copyright (c) 2020 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef VERSION_H
#define VERSION_H

#include <QVariant>

class Version
{
    friend class VersionTests;
public:
    Version(const QString & version_string);
    operator const QString &() const;
    const QString & toString() const;
    const QString PrereleaseType() const;
    bool IsReleaseVersion() const { return mPrerelease.isEmpty(); }
    bool IsValid() const { return mIsValid; }
    bool operator==(const Version & b) const { return Compare(*this, b) == 0; }
    bool operator!=(const Version & b) const { return Compare(*this, b) != 0; }
    bool operator<(const Version & b) const { return Compare(*this, b) < 0; }
    bool operator>(const Version & b) const { return Compare(*this, b) > 0; }

protected:
    const QString mString;
    bool mIsValid;
    
    int mMajor, mMinor, mPatch;
    QString mPrerelease, mBuild;
    
    void ParseSemanticVersion();
    void FixLegacyVersions();
    static int Compare(const Version & a, const Version & b);
};

//!brief Get the current version of the application.
const Version & getVersion();

QString getBranchVersion();
QString getPrereleaseSuffix();
const QString & gitRevision();
const QString & gitBranch();

#endif // VERSION_H
