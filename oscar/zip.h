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

class FileQueue
{
    struct Entry
    {
        QString path;
        QString name;
    };
    QList<Entry> m_files;
    int m_dir_count;
    int m_file_count;
    quint64 m_byte_count;

public:
    FileQueue() : m_dir_count(0), m_file_count(0), m_byte_count(0) {}
    ~FileQueue() = default;

    //!brief Recursively add a directory and its contents to the queue along with the prefix to be used in an archive.
    bool Add(const QDir & dir, const QString & prefix="");
    
    //!brief Add a file to the queue along with the name to be used in an archive.
    bool Add(const QString & path, const QString & archive_name="");

    inline int dirCount() const { return m_dir_count; }
    inline int fileCount() const { return m_file_count; }
    inline quint64 byteCount() const { return m_byte_count; }
    const QList<Entry> & files() const { return m_files; }
    const QString toString() const;
};


class ZipFile
{
public:
    ZipFile();
    virtual ~ZipFile();
    
    bool Open(const QString & filepath);
    bool Add(const QDir & root);
    bool Add(const QDir & dir, const QString & archive_name);  // add a directory and recurse
    bool Add(const QString & path, const QString & archive_name);  // add a single file
    bool Add(const FileQueue & queue);  // add a fixed list of files
    void Close();

protected:
    void* m_ctx;
    QFile m_file;
};
