/* PRS1 Unit Tests
 *
 * Copyright (c) 2019-2020 The OSCAR Team
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
    p_profile = new Profile(TESTDATA_PATH "profile/", false);

    schema::init();
    PRS1Loader::Register();
    s_loader = dynamic_cast<PRS1Loader*>(lookupLoader(prs1_class_name));
}

void PRS1Tests::cleanupTestCase(void)
{
    delete p_profile;
    p_profile = nullptr;
}


// ====================================================================================================

extern PRS1ModelInfo s_PRS1ModelInfo;
void PRS1Tests::testMachineSupport()
{
    QHash<QString,QString> tested = {
        { "ModelNumber", "550P" },
        { "Family", "0" },
        { "FamilyVersion", "3" },
    };
    QHash<QString,QString> supported = {
        { "ModelNumber", "700X999" },
        { "Family", "0" },
        { "FamilyVersion", "6" },
    };
    QHash<QString,QString> unsupported = {
        { "ModelNumber", "550P" },
        { "Family", "0" },
        { "FamilyVersion", "9" },
    };
    
    Q_ASSERT(s_PRS1ModelInfo.IsSupported(5, 3));
    Q_ASSERT(!s_PRS1ModelInfo.IsSupported(5, 9));
    Q_ASSERT(!s_PRS1ModelInfo.IsSupported(9, 9));
    Q_ASSERT(s_PRS1ModelInfo.IsTested("550P", 0, 2));
    Q_ASSERT(s_PRS1ModelInfo.IsTested("550P", 0, 3));
    Q_ASSERT(s_PRS1ModelInfo.IsTested("760P", 0, 4));
    Q_ASSERT(s_PRS1ModelInfo.IsTested("700X110", 0, 6));
    Q_ASSERT(!s_PRS1ModelInfo.IsTested("700X999", 0, 6));
    
    Q_ASSERT(s_PRS1ModelInfo.IsTested(tested));
    Q_ASSERT(!s_PRS1ModelInfo.IsTested(supported));
    Q_ASSERT(s_PRS1ModelInfo.IsSupported(tested));
    Q_ASSERT(s_PRS1ModelInfo.IsSupported(supported));
    Q_ASSERT(!s_PRS1ModelInfo.IsSupported(unsupported));
}


// ====================================================================================================


void parseAndEmitSessionYaml(const QString & path)
{
    qDebug() << path;

    // This mirrors the functional bits of PRS1Loader::OpenMachine.
    // TODO: Refactor PRS1Loader so that the tests can use the same
    // underlying logic as OpenMachine rather than duplicating it here.

    QStringList paths;
    QString propertyfile;
    int sessionid_base;
    sessionid_base = s_loader->FindSessionDirsAndProperties(path, paths, propertyfile);

    Machine *m = s_loader->CreateMachineFromProperties(propertyfile);
    if (m == nullptr) {
        qWarning() << "*** Skipping unsupported machine!";
        return;
    }

    s_loader->ScanFiles(paths, sessionid_base, m);
    
    // Each session now has a PRS1Import object in m_MLtasklist
    QList<ImportTask*>::iterator i;
    while (!s_loader->m_MLtasklist.isEmpty()) {
        ImportTask* task = s_loader->m_MLtasklist.takeFirst();

        // Run the parser
        PRS1Import* import = dynamic_cast<PRS1Import*>(task);
        bool ok = import->ParseSession();
        
        // Emit the parsed session data to compare against our regression benchmarks
        Session* session = import->session;
        QString outpath = prs1OutputPath(path, m->serial(), session->session(), "-session.yml");
        SessionToYaml(outpath, session, ok);
        
        delete session;
        delete task;
    }
    if (s_loader->m_unexpectedMessages.count() > 0) {
        qWarning() << "*** Found unexpected data";
    }
}

void PRS1Tests::testSessionsToYaml()
{
    iterateTestCards(TESTDATA_PATH "prs1/input/", parseAndEmitSessionYaml);
}


// ====================================================================================================

static QString ts(qint64 msecs)
{
    // TODO: make this UTC so that tests don't vary by where they're run
    return QDateTime::fromMSecsSinceEpoch(msecs).toString(Qt::ISODate);
}

static QString dur(qint64 msecs)
{
    qint64 s = msecs / 1000L;
    int h = s / 3600; s -= h * 3600;
    int m = s / 60; s -= m * 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

static QString byteList(QByteArray data, int limit=-1)
{
    int count = data.size();
    if (limit == -1 || limit > count) limit = count;
    int first = limit / 2;
    int last = limit - first;
    QStringList l;
    for (int i = 0; i < first; i++) {
        l.push_back(QString( "%1" ).arg((int) data[i] & 0xFF, 2, 16, QChar('0') ).toUpper());
    }
    if (limit < count) l.push_back("...");
    for (int i = count - last; i < count; i++) {
        l.push_back(QString( "%1" ).arg((int) data[i] & 0xFF, 2, 16, QChar('0') ).toUpper());
    }
    QString s = l.join(" ");
    return s;
}

void ChunkToYaml(QTextStream & out, PRS1DataChunk* chunk, bool ok)
{
    // chunk header
    out << "chunk:" << endl;
    out << "  at: " << hex << chunk->m_filepos << endl;
    out << "  parsed: " << ok << endl;
    out << "  version: " << dec << chunk->fileVersion << endl;
    out << "  size: " << chunk->blockSize << endl;
    out << "  htype: " << chunk->htype << endl;
    out << "  family: " << chunk->family << endl;
    out << "  familyVersion: " << chunk->familyVersion << endl;
    out << "  ext: " << chunk->ext << endl;
    out << "  session: " << chunk->sessionid << endl;
    out << "  start: " << ts(chunk->timestamp * 1000L) << endl;
    out << "  duration: " << dur(chunk->duration * 1000L) << endl;

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
    bool dump_data = true;
    if (chunk->m_parsedData.size() > 0) {
        dump_data = false;
        out << "  events:" << endl;
        for (auto & e : chunk->m_parsedData) {
            QString name = _PRS1ParsedEventName(e);
            if (name == "raw" || name == "unknown") {
                dump_data = true;
            }
            QMap<QString,QString> contents = _PRS1ParsedEventContents(e);
            if (name == "setting" && contents.size() == 1) {
                out << "  - set_" << contents.firstKey() << ": " << contents.first() << endl;
            }
            else {
                out << "  - " << name << ":" << endl;
                
                // Always emit start first if present
                if (contents.contains("start")) {
                    out << "      " << "start" << ": " << contents["start"] << endl;
                }
                for (auto & key : contents.keys()) {
                    if (key == "start") continue;
                    out << "      " << key << ": " << contents[key] << endl;
                }
            }
        }
    }
    if (dump_data || !ok) {
        out << "  data: " << byteList(chunk->m_data, 100) << endl;
    }
    
    // data CRC
    out << "  crc: " << hex << chunk->storedCrc << endl;
    if (chunk->storedCrc != chunk->calcCrc) {
        out << "  calcCrc: " << hex << chunk->calcCrc << endl;
    }
    out << endl;
}

void parseAndEmitChunkYaml(const QString & path)
{
    bool FV = false;  // set this to true to emit family/familyVersion for this path
    qDebug() << path;

    QHash<QString,QSet<quint64>> written;
    QStringList paths;
    QString propertyfile;
    int sessionid_base;
    sessionid_base = s_loader->FindSessionDirsAndProperties(path, paths, propertyfile);

    Machine *m = s_loader->CreateMachineFromProperties(propertyfile);
    if (m == nullptr) {
        qWarning() << "*** Skipping unsupported machine!";
        return;
    }

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
            
            if (fi.fileName() == ".DS_Store") {
                continue;
            }

            QString ext_s = fi.fileName().section(".", -1);
            int ext = ext_s.toInt(&ok);
            if (!ok) {
                // not a numerical extension
                qInfo() << inpath << "unexpected filename";
                continue;
            }

            QString session_s = fi.fileName().section(".", 0, -2);
            int sessionid = session_s.toInt(&ok, sessionid_base);
            if (!ok) {
                // not a numerical session ID
                qInfo() << inpath << "unexpected filename";
                continue;
            }
            
            // Create the YAML file.
            QString suffix = QString(".%1-chunks.yml").arg(ext, 3, 10, QChar('0'));
            QString outpath = prs1OutputPath(path, m->serial(), sessionid, suffix);
            QFile file(outpath);
            // Truncate the first time we open this file, to clear out any previous test data.
            // Otherwise append, allowing session chunks to be split among multiple files.
            if (!file.open(QFile::WriteOnly | (written.contains(outpath) ? QFile::Append : QFile::Truncate))) {
                qDebug() << outpath;
                Q_ASSERT(false);
            }
            QTextStream out(&file);

            // keep only P1234568/Pn/00000000.001
            QStringList pathlist = QDir::toNativeSeparators(inpath).split(QDir::separator(), QString::SkipEmptyParts);
            QString relative = pathlist.mid(pathlist.size()-3).join(QDir::separator());
            bool first_chunk_from_file = true;

            // Parse the chunks in the file.
            QList<PRS1DataChunk *> chunks = s_loader->ParseFile(inpath);
            for (int i=0; i < chunks.size(); i++) {
                PRS1DataChunk * chunk = chunks.at(i);
                if (i == 0 && FV) { qWarning() << QString("F%1V%2").arg(chunk->family).arg(chunk->familyVersion); FV=false; }
                // Only write unique chunks to the file.
                if (written[outpath].contains(chunk->hash()) == false) {
                    if (first_chunk_from_file) {
                        out << "file: " << relative << endl;
                        first_chunk_from_file = false;
                    }
                    bool ok = true;
                    
                    // Parse the inner data.
                    switch (chunk->ext) {
                        case 0: ok = chunk->ParseCompliance(); break;
                        case 1: ok = chunk->ParseSummary(); break;
                        case 2: ok = chunk->ParseEvents(); break;
                        case 5: break;  // skip flow/pressure waveforms
                        case 6: break;  // skip oximetry data
                        default:
                            qWarning() << relative << "unexpected file type";
                            break;
                    }
                    
                    // Emit the YAML.
                    ChunkToYaml(out, chunk, ok);
                    written[outpath] += chunk->hash();
                }
                delete chunk;
            }
            
            file.close();
        }
    }
    
    p_profile->removeMachine(m);
    delete m;
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
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    // Look through each folder in the given root
    for (auto & fi : flist) {
        QStringList machinePaths = s_loader->FindMachinesOnCard(fi.canonicalFilePath());

        // Tests should be run newest to oldest, since older sets tend to have more
        // complete data. (These are usually previously cleared data in the Clear0/Cn
        // directories.) The machines themselves will write out the summary data they
        // remember when they see an empty folder, without event or waveform data.
        // And since these tests (by design) overwrite existing output, we want the
        // earlier (more complete) data to be what's written last.
        //
        // Since the loader itself keeps only the first set of data it sees for a session,
        // we want to leave its earliest-to-latest ordering in place, and just reverse it
        // here.
        for (auto i = machinePaths.crbegin(); i != machinePaths.crend(); i++) {
            action(*i);
        }
    }
}
