/* OSCAR ZIP archive creation
 * Provides a Qt-convenient wrapper around miniz, see https://github.com/richgel999/miniz
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "zip.h"
#include <QDebug>
#include <QDateTime>


// Static functions to abstract the details of miniz from the primary logic.
static void* zip_init();
static bool zip_open(void* ctx, QFile & file);
static bool zip_add(void* ctx, const QString & archive_name, const QByteArray & data, const QDateTime & modified);
static void zip_close(void* ctx);
static void zip_done(void* ctx);


ZipFile::ZipFile()
{
    m_ctx = zip_init();
}

ZipFile::~ZipFile()
{
    Close();
    zip_done(m_ctx);
}

bool ZipFile::Open(const QString & filepath)
{
    m_file.setFileName(filepath);
    bool ok = m_file.open(QIODevice::WriteOnly);
    if (!ok) {
        qWarning() << "unable to open" << m_file.fileName();
        return false;
    }
    ok = zip_open(m_ctx, m_file);
    return ok;
}

void ZipFile::Close()
{
    if (m_file.isOpen()) {
        zip_close(m_ctx);
        m_file.close();
    }
}

bool ZipFile::Add(const QDir & root)
{
    return Add(root, root.dirName());
}

bool ZipFile::Add(const QDir & inDir, const QString & prefix)
{
    QDir dir(inDir);
    
    if (!dir.exists() || !dir.isReadable()) {
        qWarning() << dir.canonicalPath() << "can't read directory";
        return false;
    }

    // Add directory entry
    bool ok = Add(dir.canonicalPath(), prefix);
    if (!ok) {
        return false;
    }

    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    for (auto & fi : flist) {
        QString canonicalPath = fi.canonicalFilePath();
        QString relative_path = prefix + QDir::separator() + fi.fileName();
        if (fi.isSymLink()) {
            qWarning() << "skipping symlink" << canonicalPath << fi.symLinkTarget();
        } else if (fi.isDir()) {
            // Descend and recurse
            ok = Add(QDir(canonicalPath), relative_path);
        } else {
            // Add the file to the zip
            ok = Add(canonicalPath, relative_path);
        }
        if (!ok) {
            break;
        }
    }

    return ok;
}

bool ZipFile::Add(const QString & path, const QString & prefix)
{
    if (!m_file.isOpen()) {
        qWarning() << m_file.fileName() << "has not been opened for writing";
        return false;
    }

    QFileInfo fi(path);
    QByteArray data;
    QString archive_name = prefix;

    if (fi.isDir()) {
        archive_name += QDir::separator();
    } else {
        // Open and read file into memory.
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << path << "can't open";
            return false;
        }
        data = f.readAll();
    }

    //qDebug() << "attempting to add" << archive_name << ":" << data.size() << "bytes";

    bool ok = zip_add(m_ctx, archive_name, data, fi.lastModified());
    return ok;
}



// ==================================================================================================
// Static functions to abstract the details of miniz from the primary logic.

#include "miniz.h"

// Callback for miniz to write compressed data
static size_t zip_write(void *pOpaque, mz_uint64 /*file_ofs*/, const void *pBuf, size_t n)
{
    if (pOpaque == nullptr) {
        qCritical() << "null pointer passed to ZipFile::Write!";
        return 0;
    }
    QFile* file = (QFile*) pOpaque;
    size_t written = file->write((const char*) pBuf, n);
    if (written < n) {
        qWarning() << "error writing to" << file->fileName();
    }
    return written;
}

static void* zip_init()
{
    mz_zip_archive* pZip = new mz_zip_archive();  // zero-initializes struct
    pZip->m_pWrite = zip_write;
    return pZip;
}

static void zip_done(void* ctx)
{
    Q_ASSERT(ctx);
    mz_zip_archive* pZip = (mz_zip_archive*) ctx;
    delete pZip;
}

static bool zip_open(void* ctx, QFile & file)
{
    Q_ASSERT(ctx);
    mz_zip_archive* pZip = (mz_zip_archive*) ctx;

    pZip->m_pIO_opaque = &file;
    bool ok = mz_zip_writer_init_v2(pZip, 0, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (!ok) {
        mz_zip_error mz_err = mz_zip_get_last_error(pZip);
        qWarning() << "unable to initialize miniz writer" << MZ_VERSION << mz_zip_get_error_string(mz_err);
    }
    return ok;
}

static bool zip_add(void* ctx, const QString & archive_name, const QByteArray & data, const QDateTime & modified)
{
    Q_ASSERT(ctx);
    mz_zip_archive* pZip = (mz_zip_archive*) ctx;
    
    // Add to .zip
    time_t last_modified = modified.toTime_t();  // technically deprecated, but miniz expects a time_t
    bool ok = mz_zip_writer_add_mem_ex_v2(pZip, archive_name.toLocal8Bit(), data.constData(), data.size(),
                                          nullptr, 0,  // no comment
                                          MZ_DEFAULT_COMPRESSION,
                                          0, 0,  // not used when compressing data
                                          &last_modified,
                                          nullptr, 0,  // no user extra data
                                          nullptr, 0   // no user extra data central
                                         );
    if (!ok) {
        mz_zip_error mz_err = mz_zip_get_last_error(pZip);
        qWarning() << "unable to add" << archive_name << ":" << data.size() << "bytes" << mz_zip_get_error_string(mz_err);
    }
    return ok;
}

static void zip_close(void* ctx)
{
    Q_ASSERT(ctx);
    mz_zip_archive* pZip = (mz_zip_archive*) ctx;
    mz_zip_writer_finalize_archive(pZip);
    mz_zip_writer_end(pZip);
}
