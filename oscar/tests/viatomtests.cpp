/* Viatom Unit Tests
 *
 * Copyright (c) 2020 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "viatomtests.h"
#include "sessiontests.h"
#include <QRegularExpression>

#define TESTDATA_PATH "./testdata/"

static ViatomLoader* s_loader = nullptr;
static QString viatomOutputPath(const QString & inpath, const QString & suffix);

void ViatomTests::initTestCase(void)
{
    QString profile_path = TESTDATA_PATH "profile/";
    Profiles::Create("test", &profile_path);

    schema::init();
    ViatomLoader::Register();
    s_loader = dynamic_cast<ViatomLoader*>(lookupLoader(viatom_class_name));
}

void ViatomTests::cleanupTestCase(void)
{
}


// ====================================================================================================

static void parseAndEmitSessionYaml(const QString & path)
{
    qDebug() << path;

    Session* session = s_loader->ParseFile(path);
    if (session != nullptr) {
        QString outpath = viatomOutputPath(path, "-session.yml");
        SessionToYaml(outpath, session, true);
        delete session;
    }
}

void ViatomTests::testSessionsToYaml()
{
    static const QString root = TESTDATA_PATH "viatom/input/";
    static const QRegularExpression re("(20[0-5][0-9][01][0-9][0-3][0-9][012][0-9][0-5][0-9][0-5][0-9])");

    QDir dir(root);
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);

    for (auto & fi : dir.entryInfoList()) {
        QRegularExpressionMatch match = re.match(fi.fileName());
        if (match.hasMatch()) {
            parseAndEmitSessionYaml(fi.canonicalFilePath());
        }
    }
}


// ====================================================================================================

QString viatomOutputPath(const QString & inpath, const QString & suffix)
{
    // Output to viatom/output/FILENAME(-session.yml, etc.)
    QFileInfo path(inpath);
    QString basename = path.fileName();

    QDir outdir(TESTDATA_PATH "viatom/output");
    outdir.mkpath(".");
    
    QString filename = QString("%1%2")
                        .arg(basename)
                        .arg(suffix);
    return outdir.path() + QDir::separator() + filename;
}
