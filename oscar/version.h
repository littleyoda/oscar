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

#include <QString>

extern const QString VersionString;

int compareVersion(const QString & version);

QString getBranchVersion();
QString getPrereleaseSuffix();
const QString & gitRevision();
const QString & gitBranch();

bool isReleaseVersion();

#endif // VERSION_H
