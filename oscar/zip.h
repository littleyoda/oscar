/* OSCAR ZIP archive creation
 * Provides a Qt-convenient wrapper around miniz, see https://github.com/richgel999/miniz
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QString>
#include <QDir>
#include <QFile>

class ZipFile
{
public:
    ZipFile();
    virtual ~ZipFile();
    
    bool Open(const QString & filepath);
    bool Add(const QDir & root);
    bool Add(const QDir & dir, const QString & archive_name);  // add a directory and recurse
    bool Add(const QString & path, const QString & archive_name);  // add a file and recurse
    void Close();

protected:
    void* m_ctx;
    QFile m_file;
};
