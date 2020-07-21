#ifndef LOGGER_H
#define LOGGER_H

#include <QDebug>
#include <QRunnable>
#include <QThreadPool>
#include <QMutex>
#include <QWaitCondition>
#include <QTime>

void initializeLogger();
void shutdownLogger();

QString GetLogDir();
void rotateLogs(const QString & filePath, int maxPrevious=-1);


void MyOutputHandler(QtMsgType type, const QMessageLogContext &context, const QString &msgtxt);

class LogThread:public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit LogThread() : QRunnable() { running = false; logtime.start(); connected = false; m_logFile = nullptr; m_logStream = nullptr; }
    virtual ~LogThread();

    void run();
    void append(QString msg);
    void appendClean(QString msg);
    bool isRunning() { return running; }
    void connectionReady();
    void logToFile();
    QString logFileName();

    void quit();

    QStringList buffer;
    QMutex strlock;
    QThreadPool *threadpool;
signals:
    void outputLog(QString);
protected:
    volatile bool running;
    QTime logtime;
    bool connected;
    class QFile* m_logFile;
    class QTextStream* m_logStream;
    QWaitCondition logTrigger;
};

extern LogThread * logger;
extern QThreadPool * otherThreadPool;

#endif // LOGGER_H
