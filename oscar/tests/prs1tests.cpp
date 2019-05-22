/* PRS1 Unit Tests
 *
 * Copyright (c) 2019 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "prs1tests.h"
#include "sessiontests.h"

#define TESTDATA_PATH "./testdata/"

static PRS1Loader* s_loader = nullptr;
static void iterateTestCards(const QString & root, void (*action)(const QString &));
static QString prs1OutputPath(const QString & inpath, const QString & serial, const QString & basename, const QString & suffix);
static QString prs1OutputPath(const QString & inpath, const QString & serial, int session, const QString & suffix);

void PRS1Tests::initTestCase(void)
{
    initializeStrings();
    qDebug() << STR_TR_OSCAR + " " + getBranchVersion();
    QString profile_path = TESTDATA_PATH "profile/";
    Profiles::Create("test", &profile_path);

    schema::init();
    PRS1Loader::Register();
    s_loader = dynamic_cast<PRS1Loader*>(lookupLoader(prs1_class_name));
}

void PRS1Tests::cleanupTestCase(void)
{
}


void parseAndEmitSessionYaml(const QString & path)
{
    qDebug() << path;

    // This mirrors the functional bits of PRS1Loader::OpenMachine.
    // Maybe there's a clever way to add parameters to OpenMachine that
    // would make it more amenable to automated tests. But for now
    // something is better than nothing.

    QStringList paths;
    QString propertyfile;
    int sessionid_base;
    sessionid_base = s_loader->FindSessionDirsAndProperties(path, paths, propertyfile);

    Machine *m = s_loader->CreateMachineFromProperties(propertyfile);
    Q_ASSERT(m != nullptr);

    s_loader->ScanFiles(paths, sessionid_base, m);
    
    // Each session now has a PRS1Import object in m_tasklist
    QList<ImportTask*>::iterator i;
    while (!s_loader->m_tasklist.isEmpty()) {
        ImportTask* task = s_loader->m_tasklist.takeFirst();

        // Run the parser
        PRS1Import* import = dynamic_cast<PRS1Import*>(task);
        import->ParseSession();
        
        // Emit the parsed session data to compare against our regression benchmarks
        Session* session = import->session;
        QString outpath = prs1OutputPath(path, m->serial(), session->session(), "-session.yml");
        SessionToYaml(outpath, session);
        
        delete session;
        delete task;
   }
}

void PRS1Tests::testSessionsToYaml()
{
    iterateTestCards(TESTDATA_PATH "prs1/input/", parseAndEmitSessionYaml);
}


// ====================================================================================================

static QString ts(qint64 msecs)
{
    return QDateTime::fromMSecsSinceEpoch(msecs).toString(Qt::ISODate);
}

static QString byteList(QByteArray data)
{
    QStringList l;
    for (int i = 0; i < data.size(); i++) {
        l.push_back(QString( "%1" ).arg((int) data[i] & 0xFF, 2, 16, QChar('0') ).toUpper());
    }
    QString s = l.join("");
    return s;
}

void ChunkToYaml(QFile & file, PRS1DataChunk* chunk)
{
    QTextStream out(&file);

    // chunk header
    out << "chunk:" << endl;
    out << "  at: " << hex << chunk->m_filepos << endl;
    out << "  version: " << dec << chunk->fileVersion << endl;
    out << "  size: " << chunk->blockSize << endl;
    out << "  htype: " << chunk->htype << endl;
    out << "  family: " << chunk->family << endl;
    out << "  familyVersion: " << chunk->familyVersion << endl;
    out << "  ext: " << chunk->ext << endl;
    out << "  session: " << chunk->sessionid << endl;
    out << "  start: " << ts(chunk->timestamp * 1000L) << endl;

    // hblock for V3 non-waveform chunks
    if (chunk->fileVersion == 3 && chunk->htype == 0) {
        out << "  hblock:" << endl;
        QMapIterator<unsigned char, short> i(chunk->hblock);
        while (i.hasNext()) {
            i.next();
            out << "    " << (int) i.key() << ": " << i.value() << endl;
        }
    }

    // waveform chunks
    if (chunk->htype == 1) {
        out << "  intervals: " << chunk->interval_count << endl;
        out << "  intervalSeconds: " << (int) chunk->interval_seconds << endl;
        out << "  interleave:" << endl;
        for (int i=0; i < chunk->waveformInfo.size(); i++) {
            const PRS1Waveform & w = chunk->waveformInfo.at(i);
            out << "    " << i << ": " << w.interleave << endl;
        }
        out << "  end: " << ts((chunk->timestamp + chunk->duration) * 1000L) << endl;
    }
    
    // header checksum
    out << "  checksum: " << hex << chunk->storedChecksum << endl;
    if (chunk->storedChecksum != chunk->calcChecksum) {
        out << "  calcChecksum: " << hex << chunk->calcChecksum << endl;
    }
    
    // data
    out << "  data: " << byteList(chunk->m_data) << endl;
    
    // data CRC
    out << "  crc: " << hex << chunk->storedCrc << endl;
    if (chunk->storedCrc != chunk->calcCrc) {
        out << "  calcCrc: " << hex << chunk->calcCrc << endl;
    }
    out << endl;
}

void parseAndEmitChunkYaml(const QString & path)
{
    qDebug() << path;

    QStringList paths;
    QString propertyfile;
    int sessionid_base;
    sessionid_base = s_loader->FindSessionDirsAndProperties(path, paths, propertyfile);

    Machine *m = s_loader->CreateMachineFromProperties(propertyfile);
    Q_ASSERT(m != nullptr);

    // This mirrors the functional bits of PRS1Loader::ScanFiles.
    
    QDir dir;
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);

    int size = paths.size();

    // for each p0/p1/p2/etc... folder
    for (int p=0; p < size; ++p) {
        dir.setPath(paths.at(p));
        if (!dir.exists() || !dir.isReadable()) {
            qWarning() << dir.canonicalPath() << "can't read directory";
            continue;
        }
        QFileInfoList flist = dir.entryInfoList();

        // Scan for individual .00X files
        for (int i = 0; i < flist.size(); i++) {
            QFileInfo fi = flist.at(i);
            QString inpath = fi.canonicalFilePath();
            bool ok;

            QString ext_s = fi.fileName().section(".", -1);
            ext_s.toInt(&ok);
            if (!ok) {
                // not a numerical extension
                qWarning() << inpath << "unexpected filename";
                continue;
            }

            QString session_s = fi.fileName().section(".", 0, -2);
            session_s.toInt(&ok, sessionid_base);
            if (!ok) {
                // not a numerical session ID
                qWarning() << inpath << "unexpected filename";
                continue;
            }
            
            // Create the YAML file.
            QString outpath = prs1OutputPath(path, m->serial(), fi.fileName(), "-chunks.yml");
            QFile file(outpath);
            if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
                qDebug() << outpath;
                Q_ASSERT(false);
            }

            // Parse the chunks in the file.
            QList<PRS1DataChunk *> chunks = s_loader->ParseFile(inpath);
            for (int i=0; i < chunks.size(); i++) {
                // Emit the YAML.
                PRS1DataChunk * chunk = chunks.at(i);
                ChunkToYaml(file, chunk);
                delete chunk;
            }
            
            file.close();
        }
    }
}

void PRS1Tests::testChunksToYaml()
{
    iterateTestCards(TESTDATA_PATH "prs1/input/", parseAndEmitChunkYaml);
}


// ====================================================================================================

QString prs1OutputPath(const QString & inpath, const QString & serial, int session, const QString & suffix)
{
    QString basename = QString("%1").arg(session, 8, 10, QChar('0'));
    return prs1OutputPath(inpath, serial, basename, suffix);
}

QString prs1OutputPath(const QString & inpath, const QString & serial, const QString & basename, const QString & suffix)
{
    // Output to prs1/output/FOLDER/SERIAL-000000(-session.yml, etc.)
    QDir path(inpath);
    QStringList pathlist = QDir::toNativeSeparators(inpath).split(QDir::separator(), QString::SkipEmptyParts);
    pathlist.pop_back();  // drop serial number directory
    pathlist.pop_back();  // drop P-Series directory
    QString foldername = pathlist.last();

    QDir outdir(TESTDATA_PATH "prs1/output/" + foldername);
    outdir.mkpath(".");
    
    QString filename = QString("%1-%2%3")
                        .arg(serial)
                        .arg(basename)
                        .arg(suffix);
    return outdir.path() + QDir::separator() + filename;
}

void iterateTestCards(const QString & root, void (*action)(const QString &))
{
    QDir dir(root);
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    // Look through each folder in the given root
    for (int i = 0; i < flist.size(); i++) {
        QFileInfo fi = flist.at(i);
        if (fi.isDir()) {
            // If it contains a P-Series folder, it's a PRS1 SD card
            QDir pseries(fi.canonicalFilePath() + QDir::separator() + "P-Series");
            if (pseries.exists()) {
                pseries.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
                pseries.setSorting(QDir::Name);
                QFileInfoList plist = pseries.entryInfoList();

                // Look for machine directories (containing a PROP.TXT or properties.txt)
                for (int j = 0; j < plist.size(); j++) {
                    QFileInfo pfi = plist.at(j);
                    if (pfi.isDir()) {
                        QString machinePath = pfi.canonicalFilePath();
                        QDir machineDir(machinePath);
                        QFileInfoList mlist = machineDir.entryInfoList();
                        for (int k = 0; k < mlist.size(); k++) {
                            QFileInfo mfi = mlist.at(k);
                            if (QDir::match("PROP*.TXT", mfi.fileName())) {
                                // Found a properties file, this is a machine folder
                                action(machinePath);
                            }
                        }
                    }
                }
            }
        }
    }
}