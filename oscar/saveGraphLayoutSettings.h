/* Overview GUI Headers
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef BACKUPFILES_H
#define BACKUPFILES_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>

#include <QRegularExpression>
#include <QDir>
#include "Graphs/gGraphView.h"

class BackupDescriptions
{
public:
    BackupDescriptions(QDir* dir, QString filename) ;
    virtual ~BackupDescriptions();
    void add(QString key,QString desc);
    void remove(QString key);
    QString get(QString key);
    void load();
    void save();
private:
    QString filename;
    QMap <QString,QString> descriptions;
    const QRegularExpression* parseDescriptionsRe;
};

class SaveGraphLayoutSettings : public QWidget
{
	Q_OBJECT
public:
    explicit SaveGraphLayoutSettings(QString title , QWidget* parent) ;
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
    const static int backupNumMaxLength=3;
    const static int maxFiles=10;     // Max supported design limited is 1000 - based on backupName has has 3 numeric digits backupNumMaxLength=3.
    const static int maxDescriptionLen=60;

    const QRegularExpression* singleLineRe;
    const QRegularExpression* backupFileNumRe;
    const QRegularExpression* parseFilenameRe;


    QWidget*            parent;
    const   QString     title;
    gGraphView*         graphView=nullptr;

    // backup widget
    QDialog*     backupDialog;
    QListWidget* backuplist;

    QPushButton* backupaddFullBtn;   // Must be first item for workaround.
    QPushButton* backupAddBtn;
    QPushButton* backupDeleteBtn;
    QPushButton* backupRestoreBtn;
    QPushButton* backupUpdateBtn;
    QPushButton* backupRenameBtn;
    QPushButton* backupExitBtn;

    QVBoxLayout* backupLayout;
    QHBoxLayout* layout1;
    QVBoxLayout* layout2;

    QDir*   dir;
    int     unusedBackupNum;
    QListWidgetItem* updateFileList(QString findi=QString());
    QString styleOn;
    QString styleOff;
    QString styleExitBtn;
    QString styleMessageBox;
    QString styleDialog;
    QString calculateStyle(bool on,bool border);
    void    looksOn(QPushButton* button,bool on);
    BackupDescriptions* backupDescriptions;
    bool    confirmAction(QString name,QString question,QIcon* icon);

    void    createMenu();
    void    enableButtons(bool enable);
    void    add_featurertn();

    const int backupFileName = Qt::UserRole;
    int     backupNum(QString backupName);

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


#endif // BACKUPFILES_H
