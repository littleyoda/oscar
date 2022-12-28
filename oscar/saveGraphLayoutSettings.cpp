/* backup graph settiongs Implementation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define BACKUPMENUHEADER_OFF
#define MESSAGEBOXHEADER_OFF

#define TEST_MACROS_ENABLED_OFF
#include <test_macros.h>

#include <QMessageBox>
#include <QAbstractButton>
#include <QPixmap>
#include <QSize>
#include "SleepLib/profiles.h"
#include "saveGraphLayoutSettings.h"



SaveGraphLayoutSettings::SaveGraphLayoutSettings(QString title,QWidget* parent) : parent(parent),title(title)
{
    // Initialize directory accesses to profile files.
    QString dirname = p_profile->Get("{DataFolder}/");
    dir = new QDir(dirname);
    if (!dir->exists()) {
        //qWarning (QString("Cannot find the directory %1").arg(dirname));
        return ;
    }
    dir->setFilter(QDir::Files | QDir::Readable | QDir::Writable | QDir::NoSymLinks);

    createMenu() ;
    QString descFileName = QString("%1.%2").arg(title).arg("descriptions.txt");
    backupDescriptions = new BackupDescriptions (dir,descFileName);

    backupDialog->connect(backupaddFullBtn,     SIGNAL(clicked()), this, SLOT (addFull_feature()  ));
    backupDialog->connect(backupAddBtn,    SIGNAL(clicked()), this, SLOT (add_feature()  ));
    backupDialog->connect(backupRestoreBtn, SIGNAL(clicked()), this, SLOT (restore_feature()  ));
    backupDialog->connect(backupUpdateBtn,  SIGNAL(clicked()), this, SLOT (update_feature()  ));
    backupDialog->connect(backupRenameBtn,  SIGNAL(clicked()), this, SLOT (rename_feature()  ));
    backupDialog->connect(backupDeleteBtn,  SIGNAL(clicked()), this, SLOT (delete_feature()  ));
#ifndef BACKUPMENUHEADER
    backupDialog->connect(backupExitBtn,    SIGNAL(clicked()), this, SLOT (exit()  ));
#endif

    backupDialog->connect(backuplist, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)   ));
    backupDialog->connect(backuplist, SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChanged()   ));

    singleLineRe = new QRegularExpression( QString("^\\s*([^\\r\\n]{0,%1})").arg(1+maxDescriptionLen) );
    backupFileNumRe = new QRegularExpression( QString("backup(\\d+)(.shg)?$") );
    parseFilenameRe = new QRegularExpression(QString("^(%1[.](backup(\\d*)))[.]shg$").arg(title));
}

SaveGraphLayoutSettings::~SaveGraphLayoutSettings()
{
    backupDialog->disconnect(backupaddFullBtn,     SIGNAL(clicked()), this, SLOT (addFull_feature()  ));
    backupDialog->disconnect(backupAddBtn,    SIGNAL(clicked()), this, SLOT (add_feature()  ));
    backupDialog->disconnect(backupRestoreBtn, SIGNAL(clicked()), this, SLOT (restore_feature()  ));
    backupDialog->disconnect(backupUpdateBtn,  SIGNAL(clicked()), this, SLOT (update_feature()  ));
    backupDialog->disconnect(backupDeleteBtn,  SIGNAL(clicked()), this, SLOT (delete_feature()  ));
    backupDialog->disconnect(backupRenameBtn,  SIGNAL(clicked()), this, SLOT (rename_feature()  ));
#ifndef BACKUPMENUHEADER
    backupDialog->disconnect(backupExitBtn,    SIGNAL(clicked()), this, SLOT (exit()  ));
#endif

    backupDialog->disconnect(backuplist, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)   ));
    backupDialog->disconnect(backuplist, SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChanged()   ));

    delete  backupDescriptions;

    delete singleLineRe;
    delete backupFileNumRe;
    delete parseFilenameRe;
}

void SaveGraphLayoutSettings::createMenu() {
    styleOn=        calculateStyle(true,false);
    styleOff=       calculateStyle(false,false);
    styleExitBtn=calculateStyle(true,true);
            //"background:transparent;"
            //"background-color:yellow;"
    styleMessageBox=
            "QMessageBox   { "
                "background-color:yellow;"
                "color:black;"
                //"font: 24px ;"
                //"padding: 4px;"
                "border: 2px solid black;"
                "text-align: center;"
            "}"
            "QPushButton   { "
                //"font: 24px ;"
                "color:black;"
            "}"
            ;
    styleDialog=
            "QDialog { "
                "border: 3px solid black;"
                "min-width:30em;"   // make large fonts in message box
            "}";


    backupDialog= new QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint|Qt::WindowSystemMenuHint);
#ifndef BACKUPMENUHEADER
    backupDialog->setWindowFlag(Qt::FramelessWindowHint);
#else
    backupDialog->setWindowTitle(tr("Backup Files Management"));
#endif

    backupaddFullBtn   =new QPushButton("Add", parent);
    backupAddBtn  =new QPushButton(tr("Add"), parent);
    backupRestoreBtn = new QPushButton(tr("Restore"), parent);
    backupUpdateBtn = new QPushButton(tr("Update"), parent);
    backupRenameBtn = new QPushButton(tr("Rename"), parent);
    backupDeleteBtn = new QPushButton(tr("Delete"), parent);
#ifndef BACKUPMENUHEADER
    backupExitBtn = new QPushButton(*m_icon_exit,tr(""),parent);
#endif

    backuplist=new QListWidget();
    backuplist->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    backupLayout = new QVBoxLayout(backupDialog);
    layout1 = new QHBoxLayout();
    layout2 = new QVBoxLayout();
    layout2->setMargin(6);
    backupLayout->addLayout(layout1);
    backupLayout->addLayout(layout2);

    layout1->addWidget(backupaddFullBtn);
    layout1->addWidget(backupAddBtn);
    layout1->addWidget(backupRestoreBtn);
    layout1->addWidget(backupRenameBtn);
    layout1->addWidget(backupUpdateBtn);
    layout1->addWidget(backupDeleteBtn);
#ifndef BACKUPMENUHEADER
    layout1->addWidget(backupExitBtn);
#endif
    layout2->addWidget(backuplist, 1);
/*TRANSLATION*/
    backupaddFullBtn->      setToolTip(tr("Add has been inhibited. The maximum number of backups have been exceeded. "));
    backupAddBtn->          setToolTip(tr("Adds new Backup with current settings."));
    backupRestoreBtn->      setToolTip(tr("Restores settings from the selected Backup."));
    backupRenameBtn->       setToolTip(tr("Rename the selected Backup. Must edit existing name then press enter."));
    backupUpdateBtn->       setToolTip(tr("Update the selected Backup with current settings."));
    backupDeleteBtn->       setToolTip(tr("Delete the selected Backup."));
#ifndef BACKUPMENUHEADER
    backupExitBtn->         setToolTip(tr("Closes the dialog menu.  Returns to previous menu."));
#endif

    backupDialog->setStyleSheet(styleDialog);
    backupaddFullBtn->setStyleSheet(styleOff);
    backupAddBtn->setStyleSheet(styleOn);
    backupRestoreBtn->setStyleSheet(styleOn);
    backupUpdateBtn->setStyleSheet(styleOn);
    backupRenameBtn->setStyleSheet(styleOn);
    backupDeleteBtn->setStyleSheet(styleOn);
#ifndef BACKUPMENUHEADER
    backupExitBtn->setStyleSheet( styleExitBtn );
#endif


    backupaddFullBtn->setIcon(*m_icon_add  );
    backupAddBtn->setIcon(*m_icon_add  );
    backupRestoreBtn->setIcon(*m_icon_restore  );
    backupRenameBtn->setIcon(*m_icon_rename  );
    backupUpdateBtn->setIcon(*m_icon_update  );
    backupDeleteBtn->setIcon(*m_icon_delete  );
    backupDeleteBtn->setIcon(*m_icon_delete  );


    int timeout = AppSetting->tooltipTimeout();
    backupaddFullBtn->setToolTipDuration(timeout);
    backupAddBtn->setToolTipDuration(timeout);
    backupRestoreBtn->setToolTipDuration(timeout);
    backupUpdateBtn->setToolTipDuration(timeout);
    backupRenameBtn->setToolTipDuration(timeout);
    backupDeleteBtn->setToolTipDuration(timeout);
#ifndef BACKUPMENUHEADER
    backupExitBtn->setToolTipDuration(timeout);
#endif

};

bool SaveGraphLayoutSettings::confirmAction(QString name,QString question,QIcon* icon) {
    QMessageBox msgBox;
    msgBox.setText(question);
    if (icon!=nullptr) {
        msgBox.setIconPixmap(icon->pixmap(QSize(50,50)));
    }

    msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes );
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setStyleSheet(styleMessageBox);

    #ifdef MESSAGEBOXHEADER
        msgBox.setWindowTitle(name);
    #else
        Q_UNUSED(name);
        msgBox.setWindowFlag(Qt::FramelessWindowHint,true);
    #endif

    return (msgBox.exec()==QMessageBox::Yes);

}

QString SaveGraphLayoutSettings::calculateStyle(bool on,bool exitBtn){
        QString btnStyle=QString("QPushButton{%1%2}").arg(on
            ?"color: black;background-color:white;"
            :"color: darkgray;background-color:#e8e8e8 ;"
            ).arg(!exitBtn ?
            ""
            :
            "background: transparent;"
            "border-radius: 8px;"
            "border: 2px solid transparent;"
            "max-width:1em;"
            "border:none;"
            );

        QString toolTipStyle="  QToolTip { "
            "border: 1px solid black;"
            "border-width: 1px;"
            "padding: 4px;"
            "font: 14px ; color:black; background-color:yellow;"
            "}";
        btnStyle.append(toolTipStyle);
        return btnStyle;
}

void SaveGraphLayoutSettings::looksOn(QPushButton* button,bool on){
    button->setEnabled(on);
    button->setStyleSheet(on?styleOn:styleOff);
}

void SaveGraphLayoutSettings::enableButtons(bool enable) {
    looksOn(backupUpdateBtn,enable);
    looksOn(backupRestoreBtn,enable);
    looksOn(backupDeleteBtn,enable);
    looksOn(backupRenameBtn,enable);
    if (unusedBackupNum<0) {  // check if at Maximum limit
        backupAddBtn->hide();
        backupaddFullBtn->show();
    } else {
        backupaddFullBtn->hide();
        backupAddBtn->show();
    }
}

void SaveGraphLayoutSettings::update_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=backuplist->currentItem();
    if (!item) return;
    if(!confirmAction( item->text(), tr("Ok to Update?") , m_icon_update) ) return;
    QString backupName = item->data(backupFileName).toString();
    QString name = QString("%1.%2").arg(title).arg(backupName);
    graphView->SaveSettings(name);
};

void SaveGraphLayoutSettings::restore_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=backuplist->currentItem();
    if (!item) return;
    QString backupName = item->data(backupFileName).toString();
    QString name = QString("%1.%2").arg(title).arg(backupName);
    graphView->LoadSettings(name);
    exit();
};

void SaveGraphLayoutSettings::rename_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=backuplist->currentItem();
    if (!item) return;
    backuplist->editItem(item);
    // SaveGraphLayoutSettings::itemChanged(QListWidgetItem *item) is called when edit changes the entry.
    // itemChanged will update the description map
}

void SaveGraphLayoutSettings::delete_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=backuplist->currentItem();
    if (!item) return;

    if(!confirmAction(item->text(),  tr("Ok To Delete?") ,m_icon_delete) ) return;

    QString backupName = item->data(backupFileName).toString();
    backupDescriptions->remove(backupName);
    backupDescriptions->save();
    QString relFileName=QString("%1.%2.shg").arg(title).arg(backupName);
    dir->remove(relFileName);
    delete item;
    unusedBackupNum=backupNum(backupName);
    enableButtons(true);
}

void SaveGraphLayoutSettings::addFull_feature() {
    QMessageBox msgBox;
    msgBox.setText(tr("Maximum number of Backups exceeded."));
    msgBox.setStandardButtons(QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
#ifdef MESSAGEBOXHEADER
    msgBox.setWindowTitle(tr("Maximum Files Exceeded"));
#else
    msgBox.setWindowFlag(Qt::FramelessWindowHint,true);
#endif
    msgBox.setStyleSheet(styleMessageBox);
    msgBox.setIconPixmap(m_icon_addFull->pixmap(QSize(50,50)));
    msgBox.exec();
}

void SaveGraphLayoutSettings::add_feature() {
    if(!graphView) return;

    QString backupName = QString("%2%3").arg("backup").arg(unusedBackupNum,backupNumMaxLength,10,QLatin1Char('0'));
    QString name = QString("%1.%2").arg(title).arg(backupName);
    graphView->SaveSettings(name);
    // create a default description - use formatted datetime.
    QString desc=QDateTime::currentDateTime().toString();
    backupDescriptions->add(backupName,desc);
    backupDescriptions->save();
    QListWidgetItem* item = updateFileList( backupName);
    if (item!=nullptr) {
        backuplist->setCurrentItem(item,QItemSelectionModel::ClearAndSelect);
        backuplist->editItem(item);
    }
}

void SaveGraphLayoutSettings::itemChanged(QListWidgetItem *item)
{
    QString backupName=item->data(backupFileName).toString();
    QString desc= item->text();

    // use only the first line in a multiline string. Can be set using cut and paste
    QRegularExpressionMatch match = singleLineRe->match(desc);
    if (match.hasMatch()) {
        // captured match is the first line and has been truncated
        desc=match.captured(1).trimmed();   // reoves spaces at end.
    } else {
        // no match.
        // an invalid name was entered.  too much white space or empty
        desc="";
    }
    if (desc.length()>maxDescriptionLen) {
        desc.append("...");
    }
    if (desc.length() <=0) {
        // returns name back to previous saved name
        desc=backupDescriptions->get(backupName);
    } else {
        backupDescriptions->add(backupName,desc);
        backupDescriptions->save();
    }
    item->setText(desc);
}

void SaveGraphLayoutSettings::itemSelectionChanged()
{
    enableButtons(true);
}

int SaveGraphLayoutSettings::backupNum(QString backupName) {
    QRegularExpressionMatch match = backupFileNumRe->match(backupName);
    int value=-1;
    if (match.hasMatch()) {
        value=match.captured(1).toInt();
    }
    return value;
}

QListWidgetItem* SaveGraphLayoutSettings::updateFileList(QString find) {
    Q_UNUSED(find);
    QListWidgetItem* ret=nullptr;
    enableButtons(false);
    dir->refresh();
    QFileInfoList filelist = dir->entryInfoList( QDir::Files | QDir::Readable | QDir::Writable | QDir::NoSymLinks,QDir::Name);

    // Restrict number of files. easy to find availble unused entry for add function.

    int row=0;
    int count=0;
    backuplist->clear();
    unusedBackupNum=-1;
    backupDescriptions->load();
    for (int i = 0; i < filelist.size(); ++i) {
        QFileInfo fileInfo = filelist.at(i);
        QString fileName = fileInfo.fileName();
        QRegularExpressionMatch match = parseFilenameRe->match(fileName);
        if (match.hasMatch()) {
            if (match.lastCapturedIndex()==3) {
                QString backupName=match.captured(2);
                int backupNum=match.captured(3).toInt();
                // find an available backup name(number);
                if (backupNum!=count) {
                   if (unusedBackupNum<0) unusedBackupNum=count;
                }
                count++;

                QListWidgetItem *item = new QListWidgetItem(backupDescriptions->get(backupName));
                item->setData(backupFileName,backupName);
                item->setFlags(item->flags() | Qt::ItemIsEditable);
                backuplist->insertItem(row,item);
                //DEBUGF Q(count) Q(backupName) Q(item->text());
                row++;
                if (find!=nullptr && backupName==find) {
                    ret=item;
                }
            }
        }
    }
    if (unusedBackupNum<0) { // check if there is an existing empty slot
        // if not then the next available slot is at the end. CHeck if at max files.
        if (count<maxFiles) {
            // a slot is available
            unusedBackupNum=count;
        }
    }
    enableButtons(false);
    backuplist->sortItems();
    return ret;
}


void SaveGraphLayoutSettings::exit() {
    backupDialog->close();
}

void SaveGraphLayoutSettings::menu(gGraphView* graphView) {
    this->graphView=graphView;
    updateFileList();
    backupDialog->raise();
    backupDialog->exec();
    exit();
}

//====================================================================================================
//====================================================================================================
// Backup Descriptions map class with file backup

BackupDescriptions::BackupDescriptions(QDir* dir, QString _filename)
{
    filename  = dir->absoluteFilePath(_filename);
    parseDescriptionsRe = new QRegularExpression("^\\s*(\\w+):(.*)$");
};

BackupDescriptions::~BackupDescriptions() {
    delete parseDescriptionsRe;
};

void BackupDescriptions::add(QString key,QString desc) {
    descriptions.insert(key,desc);
};

void BackupDescriptions::remove(QString key) {
    descriptions.remove(key);
}
QString BackupDescriptions::get(QString key) {
    QString ret =descriptions.value(key,key);
    return ret;
}

void BackupDescriptions::save() {
    QFile file(filename);
    file.open(QFile::WriteOnly);
    QTextStream out(&file);

    QMapIterator<QString, QString>it(descriptions);
    while (it.hasNext()) {
        it.next();
        QString line=QString("%1:%2\n").arg(it.key()).arg(it.value());
        out <<line;
    }
    file.close();
}

void BackupDescriptions::load() {
    QString line;
    QFile file(filename);
    file.open(QFile::ReadOnly);
    QTextStream instr(&file);
    descriptions.clear();
    while (instr.readLineInto(&line)) {
        QRegularExpressionMatch match = parseDescriptionsRe->match(line);
        if (match.hasMatch()) {
            QString backupName = match.captured(1);
            QString desc = match.captured(2);
            add(backupName,desc);
        } else {
            DEBUGF QQ("MATCH ERROR",line);
        }
    }
}


//====================================================================================================
//====================================================================================================


#if 0
Are you &lt;b&gt;absolutely sure&lt;/b&gt; you want to proceed?

QMessageBox msgBox; msgBox.setText(tr("Confirm?"));
QAbstractButton* pButtonYes = msgBox.addButton(tr("Yeah!"), QMessageBox::YesRole);
pButtonNo=msgBox.addButton(tr("Nope"), QMessageBox::NoRole);
btn.setIcon(const QIcon &icon);

msgBox.exec();

if (msgBox.clickedButton()==pButtonYes) {


QIcon groupIcon( style()->standardIcon( QStyle::SP_DirClosedIcon ) )
https://www.pythonguis.com/faq/built-in-qicons-pyqt/

QMessageBox msgBox;
msgBox.setText("The document has been modified.");
msgBox.setInformativeText("Do you want to save your changes?");
msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
msgBox.setDefaultButton(QMessageBox::Save);
int ret = msgBox.exec();
switch (ret) {
  case QMessageBox::Save:
      // Save was clicked
      break;
  case QMessageBox::Discard:
      // Don't Save was clicked
      break;
  case QMessageBox::Cancel:
      // Cancel was clicked
      break;
  default:
      // should never be reached
      break;
}



// Reminders For testing

    Different languages unicodes to test. optained from translation files

    도움주신분들
    已成功删除
    删除
    הצג את מחיצת הנתונים

    הצג את מחיצת הנתונים  已成功删除 عذرا ، لا يمكن تحديد موقع ملف.
    已成功删除 عذرا ، لا يمكن تحديد موقع ملف.  删除
    Toon gegevensmap
    عذرا ، لا يمكن تحديد موقع ملف.


    backupDialog->connect(backuplist, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(itemActivated(QListWidgetItem*)   ));
    backupDialog->connect(backuplist, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(itemDoubleClicked(QListWidgetItem*)   ));
    backupDialog->connect(backuplist, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(itemClicked(QListWidgetItem*)   ));
    backupDialog->connect(backuplist, SIGNAL(itemEntered(QListWidgetItem*)), this, SLOT(itemEntered(QListWidgetItem*)   ));
    backupDialog->connect(backuplist, SIGNAL(itemPressed(QListWidgetItem*)), this, SLOT(itemEntered(QListWidgetItem*)   ));



    backupDialog->disconnect(backuplist, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(itemActivated(QListWidgetItem*)   ));
    backupDialog->disconnect(backuplist, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(itemDoubleClicked(QListWidgetItem*)   ));
    backupDialog->disconnect(backuplist, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(itemClicked(QListWidgetItem*)   ));
    backupDialog->disconnect(backuplist, SIGNAL(itemEntered(QListWidgetItem*)), this, SLOT(itemEntered(QListWidgetItem*)   ));
    backupDialog->disconnect(backuplist, SIGNAL(itemPressed(QListWidgetItem*)), this, SLOT(itemEntered(QListWidgetItem*)   ));


void SaveGraphLayoutSettings::itemActivated(QListWidgetItem *item)
{
    Q_UNUSED( item );
    DEBUGF Q( item->text() );
}

void SaveGraphLayoutSettings::itemDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED( item );
    DEBUGF Q( item->text() );
}

void SaveGraphLayoutSettings::itemClicked(QListWidgetItem *item)
{
    Q_UNUSED( item );
    DEBUGF Q( item->text() );
}

void SaveGraphLayoutSettings::itemEntered(QListWidgetItem *item)
{
    Q_UNUSED( item );
    DEBUGF Q( item->text() );
}

void SaveGraphLayoutSettings::itemPressed(QListWidgetItem *item)
{
    Q_UNUSED( item );
    DEBUGF Q( item->text() );
}

//private_slots:
    void    itemActivated(QListWidgetItem *item);
    void    itemDoubleClicked(QListWidgetItem *item);
    void    itemClicked(QListWidgetItem *item);
    void    itemEntered(QListWidgetItem *item);
    void    itemPressed(QListWidgetItem *item);





#endif

