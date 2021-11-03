/* Botan platform-specific wrapper
 *
 * Copyright (c) 2021 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef BOTAN_ALL_H
#define BOTAN_ALL_H

// This wrapper makes it easy to regenerate Botan's platform-specific headers.

#include <QtGlobal>

#ifdef Q_OS_WIN
#include "botan_windows.h"
#endif
#ifdef Q_OS_LINUX
#include "botan_linux.h"
#endif
#ifdef Q_OS_MACOS
#include "botan_macos.h"
#endif

#endif // BOTAN_ALL_H
