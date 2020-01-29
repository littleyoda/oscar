/* ZEO Unit Tests
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "zeotests.h"
#include "sessiontests.h"

#define TESTDATA_PATH "./testdata/"

static ZEOLoader* s_loader = nullptr;
static QString zeoOutputPath(const QString & inpath, const QString & suffix);

void ZeoTests::initTestCase(void)
{
    p_profile = new Profile(TESTDATA_PATH "profile/", false);

    schema::init();
    ZEOLoader::Register();
    s_loader = dynamic_cast<ZEOLoader*>(lookupLoader(zeo_class_name));
}

void ZeoTests::cleanupTestCase(void)
{
    delete p_profile;
    p_profile = nullptr;
}


// ====================================================================================================

static void parseAndEmitSessionYaml(const QString & path)
{
    qDebug() << path;

    if (s_loader->openCSV(path)) {
        Session* session;
        while ((session = s_loader->readNextSession()) != nullptr) {
            QString outpath = zeoOutputPath(path, "-session.yml");
            SessionToYaml(outpath, session, true);
            delete session;
        }
    }
}

void ZeoTests::testSessionsToYaml()
{
    static const QString root_path = TESTDATA_PATH "zeo/input/";

    QDir root(root_path);
    root.setFilter(QDir::NoDotAndDotDot | QDir::Dirs);
    root.setSorting(QDir::Name);
    for (auto & dir_info : root.entryInfoList()) {
        QDir dir(dir_info.canonicalFilePath());
        dir.setFilter(QDir::Files | QDir::Hidden);
        dir.setNameFilters(QStringList("*.csv"));
        dir.setSorting(QDir::Name);
        for (auto & fi : dir.entryInfoList()) {
            parseAndEmitSessionYaml(fi.canonicalFilePath());
        }
    }
}


// ====================================================================================================

QString zeoOutputPath(const QString & inpath, const QString & suffix)
{
    // Output to zeo/output/DIR/FILENAME(-session.yml, etc.)
    QFileInfo path(inpath);
    QString basename = path.fileName();
    QString foldername = path.dir().dirName();

    QDir outdir(TESTDATA_PATH "zeo/output/" + foldername);
    outdir.mkpath(".");
    
    QString filename = QString("%1%2")
                        .arg(basename)
                        .arg(suffix);
    return outdir.path() + QDir::separator() + filename;
}
