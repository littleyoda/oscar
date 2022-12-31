/* Overview GUI Headers
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SAVEGRAPHLAYOUTSETTINGS_H
#define SAVEGRAPHLAYOUTSETTINGS_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>

#include <QRegularExpression>
#include <QDir>
#include "Graphs/gGraphView.h"

class DescriptionMap
{
public:
    DescriptionMap(QDir* dir, QString filename) ;
    virtual ~DescriptionMap();
    void add(QString key,QString desc);
    void remove(QString key);
    QString get(QString key);
    void load();
    void save();
private:
    QString filename;
    QMap <QString,QString> descriptions;
    const QRegularExpression* parseDescriptionsRe;
    QChar delimiter = QChar(177);
};

class SaveGraphLayoutSettings : public QWidget
{
	Q_OBJECT
public:
    SaveGraphLayoutSettings(QString title, QWidget* parent) ;
    ~SaveGraphLayoutSettings();
    void menu(gGraphView* graphView);
protected:
    QIcon*  m_icon_exit         = new QIcon(":/icons/exit.png");
    QIcon*  m_icon_delete       = new QIcon(":/icons/trash_can.png");
    QIcon*  m_icon_update       = new QIcon(":/icons/update.png");
    QIcon*  m_icon_restore      = new QIcon(":/icons/restore.png");
    QIcon*  m_icon_rename       = new QIcon(":/icons/rename.png");
    QIcon*  m_icon_add          = new QIcon(":/icons/plus.png");
    QIcon*  m_icon_addFull      = new QIcon(":/icons/brick-wall.png");

private:
    const static int fileNumMaxLength=3;
    const static int maxFiles=20;     // Max supported design limited is 1000 - based on layoutName has has 3 numeric digits fileNumMaxLength=3.
    const static int maxDescriptionLen=80;
    const QString baseName=QString("layout");

    const QRegularExpression* singleLineRe;
    const QRegularExpression* fileNumRe;
    const QRegularExpression* parseFilenameRe;


    QWidget*            parent;
    const   QString     title;
    gGraphView*         graphView=nullptr;

    QDialog*     menuDialog;
    QListWidget* menulist;

    QPushButton* menuAddFullBtn;   // Must be first item for workaround.
    QPushButton* menuAddBtn;
    QPushButton* menuDeleteBtn;
    QPushButton* menuRestoreBtn;
    QPushButton* menuUpdateBtn;
    QPushButton* menuRenameBtn;
    QPushButton* menuExitBtn;

    QVBoxLayout* menuLayout;
    QHBoxLayout* menuLayout1;
    QVBoxLayout* menuLayout2;

    QDir*   dir=nullptr;
    QString dirName;
    int     nextNumToUse;
    QListWidgetItem* updateFileList(QString find=QString());
    QString styleOn;
    QString styleOff;
    QString styleExitBtn;
    QString styleMessageBox;
    QString styleDialog;
    QString calculateStyle(bool on,bool border);
    void    looksOn(QPushButton* button,bool on);
    DescriptionMap* descriptionMap;
    bool    confirmAction(QString name,QString question,QIcon* icon);

    void    createMenu();
    void    createSaveFolder();
    void    enableButtons(bool enable);
    void    add_featurertn();

    const int fileNameRole = Qt::UserRole;
    int     fileNum(QString fileName);
    void    writeSettings(QString filename);
    void    loadSettings(QString filename);
    void    deleteSettings(QString filename);

public slots:
private slots:
    void    add_feature();
    void    addFull_feature();
    void    update_feature();
    void    restore_feature();
    void    delete_feature();
    void    rename_feature();
    void    exit();

    void    itemChanged(QListWidgetItem *item);
    void    itemSelectionChanged();
};


#endif // SAVEGRAPHLAYOUTSETTINGS_H

