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
#include <QRegularExpression>

#define VERSION "1.1.0-beta-1"

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

QString getPrereleaseSuffix()
{
    QString suffix;
    
    // Append branch if there is a branch specified
    if (gitBranch() != "master") {
        suffix += "-"+gitBranch();
    }

    // Append "-test" if not release or release candidate
    else if (getVersion().IsReleaseVersion() == false) {
        if (getVersion().PrereleaseType().compare("rc") != 0) {
            suffix += "-test";
        }
    }

    return suffix;
}

// TODO: add preprocessor macros to build full version number including build metadata, accounting for tarball/non-git builds.

static const Version s_Version(VERSION "+" + GIT_BRANCH + "-" + GIT_REVISION);
const Version & getVersion()
{
    return s_Version;
}

Version::Version(const QString & version_string) : mString(version_string), mIsValid(false)
{
    ParseSemanticVersion();
    FixLegacyVersions();
}

Version::operator const QString &() const
{
    return toString();
}

const QString & Version::toString() const
{
    return mString;
}


// Alternate formatting of the version string for display or logging
const QString Version::minimalString() const
{
    return toString().section("+", 0, 0);
}

const QString & Version::getBuildMetadata() const
{
    return mBuild;
}

const QString Version::displayString() const
{
    if (IsReleaseVersion())
        return minimalString();
    else
        return toString();
}

// This is application-specific interpretation of the prerelease data.
const QString Version::PrereleaseType() const
{
    // Extract the first identifier
    QString type = mPrerelease.section(".", 0, 0);

    // Remove any "-2", etc. that's included in the first identifier rather than as a dot-separated identifier
    type = type.section("-", 0, 0);

    return type.toLower();
}

// Parse a version string as specified by Semantic Versioning 2.0.0, see https://semver.org/spec/v2.0.0.html
void Version::ParseSemanticVersion()
{
    // Use a C++11 raw string literal to keep the regular expression (mostly) legible.
    static const QRegularExpression re(R"(^(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)(?:-(?P<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+(?P<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$)");
    QRegularExpressionMatch match = re.match(mString);
    while (match.hasMatch()) {
        bool ok;

        mMajor = match.captured("major").toInt(&ok);
        if (!ok) break;
        mMinor = match.captured("minor").toInt(&ok);
        if (!ok) break;
        mPatch = match.captured("patch").toInt(&ok);
        if (!ok) break;
        mPrerelease = match.captured("prerelease");
        mBuild = match.captured("buildmetadata");
        mIsValid = true;
        break;
    }

    // If we ever encounter any really old version whose version isn't valid, its
    // major version will be 0, so it will correctly be considered older than
    // valid versions.
}

// Deal with non-Semantic-Versioning numbers used before 1.1.0-beta-2 to make sure they
// will have proper (lower) precedence compared to later versions.
//
// TODO: THIS CAN PROBABLY BE REMOVED AFTER THE RELEASE OF 1.1.0, since the release
// version will take precedence over all 1.1.0 prereleases, as well as 1.0.1 of any
// release status.
//
// Right now we just need to make sure that 1.1.0-beta versions take precedence over
// 1.1.0-testing.
void Version::FixLegacyVersions()
{
    if (mIsValid) {
        // Replace prerelease "testing" with "alpha" for backwards compatibility with 1.1.0-testing-*
        // versions: otherwise "testing" would take precedence over "beta".
        mPrerelease.replace("testing", "alpha");

        // Technically the use of "r1" in "1.0.1-r1" could also be corrected, as the code
        // will incorrectly consider that release version to be a prerelease, but it doesn't
        // matter because 1.1.0 and later will take precedence either way.
    }
}

// Compare two version instances in accordance with Semantic Versionin 2.0.0 precedence rules.
int Version::Compare(const Version & a, const Version & b)
{
    int diff;
    
    diff = a.mMajor - b.mMajor;
    if (diff) return diff;

    diff = a.mMinor - b.mMinor;
    if (diff) return diff;

    diff = a.mPatch - b.mPatch;
    if (diff) return diff;

    // Version numbers are equal, now check prerelease status:

    if (a.IsReleaseVersion() && b.IsReleaseVersion()) return 0;

    // A pre-release version has lower prededence than a release version.
    diff = a.IsReleaseVersion() - b.IsReleaseVersion();
    if (diff) return diff;

    // Both are prerelease versions, compare them:

    // The prerelease version may contain a series of dot-separated identifiers,
    // each of which is compared.
    QStringList ap = a.mPrerelease.split(".");
    QStringList bp = b.mPrerelease.split(".");
    int max = qMin(ap.size(), bp.size());
    for (int i = 0; i < max; i++) {
        bool a_is_num, b_is_num;
        int ai = ap[i].toInt(&a_is_num);
        int bi = bp[i].toInt(&b_is_num);

        // Numeric identifiers always have lower precedence than non-numeric.
        diff = b_is_num - a_is_num;
        if (diff) return diff;
        
        if (a_is_num) {
            // Numeric identifiers are compared numerically.
            diff = ai - bi;
            if (diff) return diff;
        } else {
            // Non-numeric identifiers are compared lexically.
            diff = ap[i].compare(bp[i]);
            if (diff) return diff;
        }
    }

    // A larger set of pre-release fields has higher precedence (if the above were equal).
    diff = ap.size() - bp.size();

    // We ignore build metadata in comparing semantic versions.

    return diff;
}
