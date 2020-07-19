/* OSCAR Logger module implementation
 *
 * Copyright (c) 2020 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "logger.h"
#include "SleepLib/preferences.h"
#include "version.h"
#include <QDir>

#define ASSERTS_SUCK

QThreadPool * otherThreadPool = NULL;

void MyOutputHandler(QtMsgType type, const QMessageLogContext &context, const QString &msgtxt)
{
    Q_UNUSED(context)

    if (!logger) {
        fprintf(stderr, "Pre/Post: %s\n", msgtxt.toLocal8Bit().constData());
        return;
    }

    QString msg, typestr;

    switch (type) {
    case QtWarningMsg:
        typestr = QString("Warning: ");
        break;

    case QtFatalMsg:
        typestr = QString("Fatal: ");
        break;

    case QtCriticalMsg:
        typestr = QString("Critical: ");
        break;

    default:
        typestr = QString("Debug: ");
        break;
    }

    msg = typestr + msgtxt; //+QString(" (%1:%2, %3)").arg(context.file).arg(context.line).arg(context.function);


    if (logger && logger->isRunning()) {
        logger->append(msg);
    }
#ifdef ASSERTS_SUCK
//    else {
//        fprintf(stderr, "%s\n", msg.toLocal8Bit().data());
//    }
#endif

    if (type == QtFatalMsg) {
        abort();
    }

}

static QMutex s_LoggerRunning;

void initializeLogger()
{
    s_LoggerRunning.lock();  // lock until the thread starts running
    logger = new LogThread();
    otherThreadPool = new QThreadPool();
    bool b = otherThreadPool->tryStart(logger);
    if (b) {
        s_LoggerRunning.lock();  // wait until the thread begins running
        s_LoggerRunning.unlock();  // we no longer need the lock
    }
    qInstallMessageHandler(MyOutputHandler);  // NOTE: comment this line out when debugging a crash, otherwise the deferred output will mislead you.
    if (b) {
        qDebug() << "Started logging thread";
    } else {
        qWarning() << "Logging thread did not start correctly";
    }
}

void LogThread::connectionReady()
{
    strlock.lock();
    connected = true;
    strlock.unlock();
    qDebug() << "Logging UI initialized";
}

void shutdownLogger()
{
    if (logger) {
        logger->quit();
        otherThreadPool->waitForDone(-1);
        logger = NULL;
    }
    delete otherThreadPool;
}

LogThread * logger = NULL;

void LogThread::append(QString msg)
{
    QString tmp = QString("%1: %2").arg(logtime.elapsed(), 5, 10, QChar('0')).arg(msg);
    //QStringList appears not to be threadsafe
    strlock.lock();
    buffer.append(tmp);
    strlock.unlock();
}

void LogThread::appendClean(QString msg)
{
    strlock.lock();
    buffer.append(msg);
    strlock.unlock();
}


void LogThread::quit() {
    qDebug() << "Shutting down logging thread";
    running = false;
    strlock.lock();
    while (!buffer.isEmpty()) {
        QString msg = buffer.takeFirst();
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    }
    strlock.unlock();
}


void LogThread::run()
{
    running = true;
    s_LoggerRunning.unlock();  // unlock as soon as the thread begins to run
    do {
        strlock.lock();
        //int r = receivers(SIGNAL(outputLog(QString())));
        while (connected && !buffer.isEmpty()) {
            QString msg = buffer.takeFirst();
                fprintf(stderr, "%s\n", msg.toLocal8Bit().data());
                emit outputLog(msg);
        }
        strlock.unlock();
        QThread::msleep(1000);
    } while (running);
}


QString GetLogDir()
{
    static const QString LOG_DIR_NAME = "logs";

    QDir oscarData(GetAppData());
    Q_ASSERT(oscarData.exists());
    if (!oscarData.exists(LOG_DIR_NAME)) {
        oscarData.mkdir(LOG_DIR_NAME);
    }
    QDir logDir(oscarData.canonicalPath() + "/" + LOG_DIR_NAME);
    if (!logDir.exists()) {
        qWarning() << "Unable to create" << logDir.absolutePath() << "reverting to" << oscarData.canonicalPath();
        logDir = oscarData;
    }
    Q_ASSERT(logDir.exists());

    return logDir.canonicalPath();
}

void rotateLogs(const QString & filePath, int maxPrevious)
{
    if (maxPrevious < 0) {
        if (getVersion().IsReleaseVersion()) {
            maxPrevious = 1;
        } else {
            // keep more in testing builds
            maxPrevious = 4;
        }
    }

    // Build the list of rotated logs for this filePath.
    QFileInfo info(filePath);
    QString path = QDir(info.absolutePath()).canonicalPath();
    QString base = info.baseName();
    QString ext = info.completeSuffix();
    if (!ext.isEmpty()) {
        ext = "." + ext;
    }
    if (path.isEmpty()) {
        qWarning() << "Skipping log rotation, directory does not exist:" << info.absoluteFilePath();
        return;
    }

    QStringList logs;
    logs.append(filePath);
    for (int i = 0; i < maxPrevious; i++) {
        logs.append(QString("%1/%2.%3%4").arg(path).arg(base).arg(i).arg(ext));
    }

    // Remove the expired log.
    QFileInfo expired(logs[maxPrevious]);
    if (expired.exists()) {
        if (expired.isDir()) {
            QDir dir(expired.canonicalFilePath());
            //qDebug() << "Removing expired log directory" << dir.canonicalPath();
            if (!dir.removeRecursively()) {
                qWarning() << "Unable to delete expired log directory" << dir.canonicalPath();
            }
        } else {
            QFile file(expired.canonicalFilePath());
            //qDebug() << "Removing expired log file" << file.fileName();
            if (!file.remove()) {
                qWarning() << "Unable to delete expired log file" << file.fileName();
            }
        }
    }

    // Rotate the remaining logs.
    for (int i = maxPrevious; i > 0; i--) {
        QFileInfo from(logs[i-1]);
        QFileInfo to(logs[i]);
        if (from.exists()) {
            if (to.exists()) {
                qWarning() << "Unable to rotate log:" << to.absoluteFilePath() << "exists";
                continue;
            }
            //qDebug() << "Renaming" << from.absoluteFilePath() << "to" << to.absoluteFilePath();
            if (!QFile::rename(from.absoluteFilePath(), to.absoluteFilePath())) {
                qWarning() << "Unable to rename" << from.absoluteFilePath() << "to" << to.absoluteFilePath();
            }
        }
    }
}
