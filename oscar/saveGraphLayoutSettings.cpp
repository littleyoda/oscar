/* user graph settings Implementation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLED
#include <test_macros.h>

#include <QMessageBox>
#include <QAbstractButton>
#include <QPixmap>
#include <QSize>
#include <QChar>
#include "SleepLib/profiles.h"
#include "saveGraphLayoutSettings.h"


SaveGraphLayoutSettings::SaveGraphLayoutSettings(QString title,QWidget* parent) : parent(parent),title(title)
{
    createSaveFolder();
    if (dir==nullptr) return;
    dir->setFilter(QDir::Files | QDir::Readable | QDir::Writable | QDir::NoSymLinks);

    QString descFileName = dirName+title.toLower()+".descriptions.txt";
    descriptionMap = new DescriptionMap (dir,descFileName);

    createMenu() ;

    menuDialog->connect(menuAddFullBtn,     SIGNAL(clicked()), this, SLOT (addFull_feature()  ));
    menuDialog->connect(menuAddBtn,    SIGNAL(clicked()), this, SLOT (add_feature()  ));
    menuDialog->connect(menuRestoreBtn, SIGNAL(clicked()), this, SLOT (restore_feature()  ));
    menuDialog->connect(menuUpdateBtn,  SIGNAL(clicked()), this, SLOT (update_feature()  ));
    menuDialog->connect(menuRenameBtn,  SIGNAL(clicked()), this, SLOT (rename_feature()  ));
    menuDialog->connect(menuDeleteBtn,  SIGNAL(clicked()), this, SLOT (delete_feature()  ));
    menuDialog->connect(menuExitBtn,    SIGNAL(clicked()), this, SLOT (exit()  ));

    menuDialog->connect(menulist, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)   ));
    menuDialog->connect(menulist, SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChanged()   ));

    singleLineRe = new QRegularExpression( QString("^\\s*([^\\r\\n]{0,%1})").arg(1+maxDescriptionLen) );
    fileNumRe = new QRegularExpression( QString("%1(\\d+)(.shg)?$").arg(baseName) );
    parseFilenameRe = new QRegularExpression(QString("^(%1[.](%2(\\d*)))[.]shg$").arg(title).arg(baseName));
}

SaveGraphLayoutSettings::~SaveGraphLayoutSettings()
{

    if (dir==nullptr) {return;}
    menuDialog->disconnect(menuAddFullBtn,     SIGNAL(clicked()), this, SLOT (addFull_feature()  ));
    menuDialog->disconnect(menuAddBtn,    SIGNAL(clicked()), this, SLOT (add_feature()  ));
    menuDialog->disconnect(menuRestoreBtn, SIGNAL(clicked()), this, SLOT (restore_feature()  ));
    menuDialog->disconnect(menuUpdateBtn,  SIGNAL(clicked()), this, SLOT (update_feature()  ));
    menuDialog->disconnect(menuDeleteBtn,  SIGNAL(clicked()), this, SLOT (delete_feature()  ));
    menuDialog->disconnect(menuRenameBtn,  SIGNAL(clicked()), this, SLOT (rename_feature()  ));
    menuDialog->disconnect(menuExitBtn,    SIGNAL(clicked()), this, SLOT (exit()  ));

    menuDialog->disconnect(menulist, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)   ));
    menuDialog->disconnect(menulist, SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChanged()   ));

    delete descriptionMap;
    delete singleLineRe;
    delete fileNumRe;
    delete parseFilenameRe;
}

void SaveGraphLayoutSettings::createSaveFolder() {
    // Insure that the save folder exists
    // Get the directory name for the save files
    QString layoutFileFolder = "savedGraphLayoutSettings/";

    #if 0
    // home directory for the current  profile.
    QString baseName = p_profile->Get("{DataFolder}/");

    #else
    // home directory for all profiles.
    // allows settings to be shared accross profiles.
    QString baseName = p_pref->Get("{home}/");

    #endif

    dirName = baseName+layoutFileFolder;

    // Check if the save folder exists
    QDir* tmpDir = new QDir(dirName);
    if (!tmpDir->exists()) {
        QDir* baseDir=new QDir(baseName);
        if (!baseDir->exists()) { 
            // Base folder does not exist - terminate
            return ;
        }
        // saved Setting folder does not exist. make it
        if (!baseDir->mkdir(dirName)) {
            // Did not create the folder.
            return ;
        }
        tmpDir = new QDir(dirName);
        // double check if save folder exists or not.
        if (!tmpDir->exists()) {
            return ;
        }
    }
    dir=tmpDir;
}

void SaveGraphLayoutSettings::createMenu() {
    styleOn=        calculateStyle(true,false);
    styleOff=       calculateStyle(false,false);
    styleExitBtn=calculateStyle(true,true);
    styleMessageBox=
            "QMessageBox   { "
                "background-color:yellow;"
                "color:black;"
                "border: 2px solid black;"
                "text-align: center;"
            "}"
            "QPushButton   { "
                "color:black;"
            "}"
            ;
    styleDialog=
            "QDialog { "
                "border: 3px solid black;"
                "min-width:30em;"   // make large fonts in message box
            "}";


    menuDialog= new QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint|Qt::WindowSystemMenuHint);
    menuDialog->setWindowFlag(Qt::FramelessWindowHint);

    menuAddFullBtn   =new QPushButton("Add", parent);
    menuAddBtn  =new QPushButton(tr("Add"), parent);
    menuRestoreBtn = new QPushButton(tr("Restore"), parent);
    menuUpdateBtn = new QPushButton(tr("Update"), parent);
    menuRenameBtn = new QPushButton(tr("Rename"), parent);
    menuDeleteBtn = new QPushButton(tr("Delete"), parent);
    menuExitBtn = new QPushButton(*m_icon_exit,tr(""),parent);

    menulist=new QListWidget(menuDialog);
    menulist->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    menuLayout = new QVBoxLayout(menuDialog);
    menuLayout1 = new QHBoxLayout();
    menuLayout2 = new QVBoxLayout();
    menuLayout2->setMargin(6);
    menuLayout->addLayout(menuLayout1);
    menuLayout->addLayout(menuLayout2);

    menuLayout1->addWidget(menuAddFullBtn);
    menuLayout1->addWidget(menuAddBtn);
    menuLayout1->addWidget(menuRestoreBtn);
    menuLayout1->addWidget(menuRenameBtn);
    menuLayout1->addWidget(menuUpdateBtn);
    menuLayout1->addWidget(menuDeleteBtn);
    menuLayout1->addWidget(menuExitBtn);
    menuLayout2->addWidget(menulist, 1);
/*TRANSLATION*/
    menuAddFullBtn->      setToolTip(tr("Add Feature inhibited. The maximum number has been exceeded."));
    menuAddBtn->          setToolTip(tr("Save current settings."));
    menuRestoreBtn->      setToolTip(tr("Restores saved settings."));
    menuRenameBtn->       setToolTip(tr("Renames the selection. Must edit existing name then press enter."));
    menuUpdateBtn->       setToolTip(tr("Updates the selection with current settings."));
    menuDeleteBtn->       setToolTip(tr("Delete the selection."));
    menuExitBtn->         setToolTip(tr("Closes the dialog menu.  Returns to previous menu."));

    menuDialog->setStyleSheet(styleDialog);
    menuAddFullBtn->setStyleSheet(styleOff);
    menuAddBtn->setStyleSheet(styleOn);
    menuRestoreBtn->setStyleSheet(styleOn);
    menuUpdateBtn->setStyleSheet(styleOn);
    menuRenameBtn->setStyleSheet(styleOn);
    menuDeleteBtn->setStyleSheet(styleOn);
    menuExitBtn->setStyleSheet( styleExitBtn );


    menuAddFullBtn->setIcon(*m_icon_add  );
    menuAddBtn->setIcon(*m_icon_add  );
    menuRestoreBtn->setIcon(*m_icon_restore  );
    menuRenameBtn->setIcon(*m_icon_rename  );
    menuUpdateBtn->setIcon(*m_icon_update  );
    menuDeleteBtn->setIcon(*m_icon_delete  );
    menuDeleteBtn->setIcon(*m_icon_delete  );


    int timeout = AppSetting->tooltipTimeout();
    menuAddFullBtn->setToolTipDuration(timeout);
    menuAddBtn->setToolTipDuration(timeout);
    menuRestoreBtn->setToolTipDuration(timeout);
    menuUpdateBtn->setToolTipDuration(timeout);
    menuRenameBtn->setToolTipDuration(timeout);
    menuDeleteBtn->setToolTipDuration(timeout);
    menuExitBtn->setToolTipDuration(timeout);

};

bool SaveGraphLayoutSettings::confirmAction(QString name,QString question,QIcon* icon) {
    QMessageBox msgBox(menuDialog);
    msgBox.setText(question);
    if (icon!=nullptr) {
        msgBox.setIconPixmap(icon->pixmap(QSize(50,50)));
    }

    msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes );
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setStyleSheet(styleMessageBox);
    msgBox.setWindowFlag(Qt::FramelessWindowHint,true);

    return (msgBox.exec()==QMessageBox::Yes);
    Q_UNUSED(name);

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
    looksOn(menuUpdateBtn,enable);
    looksOn(menuRestoreBtn,enable);
    looksOn(menuDeleteBtn,enable);
    looksOn(menuRenameBtn,enable);
    if (nextNumToUse<0) {  // check if at Maximum limit
        menuAddBtn->hide();
        menuAddFullBtn->show();
    } else {
        menuAddFullBtn->hide();
        menuAddBtn->show();
    }
}

void SaveGraphLayoutSettings::add_feature() {
    if(!graphView) return;
    QString fileName = QString("%1%2").arg(baseName).arg(nextNumToUse,fileNumMaxLength,10,QLatin1Char('0'));
    writeSettings(fileName);
    // create a default description - use formatted datetime.
    QString desc=QDateTime::currentDateTime().toString();
    descriptionMap->add(fileName,desc);
    descriptionMap->save();
    QListWidgetItem* item = updateFileList( fileName);
    if (item!=nullptr) {
        menulist->setCurrentItem(item,QItemSelectionModel::ClearAndSelect);
        menulist->editItem(item);
    }
}

void SaveGraphLayoutSettings::addFull_feature() {
    QMessageBox msgBox(menuDialog);
    msgBox.setText(tr("Maximum number exceeded."));
    msgBox.setStandardButtons(QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setWindowFlag(Qt::FramelessWindowHint,true);
    msgBox.setStyleSheet(styleMessageBox);
    msgBox.setIconPixmap(m_icon_addFull->pixmap(QSize(50,50)));
    msgBox.exec();
}

void SaveGraphLayoutSettings::update_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=menulist->currentItem();
    if(!confirmAction( item->text(), tr("Ok to Update?") , m_icon_update) ) return;
    QString fileName = item->data(fileNameRole).toString();
    writeSettings(fileName);
};

void SaveGraphLayoutSettings::restore_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=menulist->currentItem();
    if (!item) return;
    QString fileName = item->data(fileNameRole).toString();
    loadSettings(fileName);
    exit();
};

void SaveGraphLayoutSettings::rename_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=menulist->currentItem();
    if (!item) return;
    menulist->editItem(item);
    // SaveGraphLayoutSettings::itemChanged(QListWidgetItem *item) is called when edit changes the entry.
    // itemChanged will update the description map
}

void SaveGraphLayoutSettings::delete_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=menulist->currentItem();
    if (!item) return;
    if(!confirmAction(item->text(),  tr("Ok To Delete?") ,m_icon_delete) ) return;

    QString fileName = item->data(fileNameRole).toString();
    descriptionMap->remove(fileName);
    descriptionMap->save();
    deleteSettings(fileName);
    delete item;
    if (nextNumToUse<0) {
        nextNumToUse=fileNum(fileName);
    }
    enableButtons(true);
}

void SaveGraphLayoutSettings::itemChanged(QListWidgetItem *item)
{
    QString fileName=item->data(fileNameRole).toString();
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
        desc=descriptionMap->get(fileName);
    } else {
        descriptionMap->add(fileName,desc);
        descriptionMap->save();
    }
    item->setText(desc);
}

void SaveGraphLayoutSettings::itemSelectionChanged()
{
    enableButtons(true);
}

void SaveGraphLayoutSettings::writeSettings(QString filename) {
    graphView->SaveSettings(title+"."+filename,dirName);
};

void SaveGraphLayoutSettings::loadSettings(QString filename) {
    graphView->LoadSettings(title+"."+filename,dirName);
};

void SaveGraphLayoutSettings::deleteSettings(QString filename) {
    QString fileName=graphView->settingsFilename (title+"."+filename,dirName) ;
    dir->remove(fileName);
};

int SaveGraphLayoutSettings::fileNum(QString fileName) {
    QRegularExpressionMatch match = fileNumRe->match(fileName);
    int value=-1;
    if (match.hasMatch()) {
        value=match.captured(1).toInt();
    }
    return value;
}

QListWidgetItem* SaveGraphLayoutSettings::updateFileList(QString find) {
    QListWidgetItem* ret=nullptr;
    enableButtons(false);
    dir->refresh();
    QFileInfoList filelist = dir->entryInfoList( QDir::Files | QDir::Readable | QDir::Writable | QDir::NoSymLinks,QDir::Name);

    // Restrict number of files. easy to find availble unused entry for add function.

    int row=0;
    int count=0;
    menulist->clear();
    nextNumToUse=-1;
    descriptionMap->load();
    for (int i = 0; i < filelist.size(); ++i) {
        QFileInfo fileInfo = filelist.at(i);
        QString fileName = fileInfo.fileName();
        QRegularExpressionMatch match = parseFilenameRe->match(fileName);
        if (match.hasMatch()) {
            if (match.lastCapturedIndex()==3) {
                QString fileName=match.captured(2);
                if (nextNumToUse<0) {
                    // check if an entry is availavle to use
                    int fileNum=match.captured(3).toInt();
                    // find an available file name(number);
                    if (fileNum!=count) {
                       nextNumToUse=count;
                    }
                }
                count++;

                QListWidgetItem *item = new QListWidgetItem(descriptionMap->get(fileName));
                item->setData(fileNameRole,fileName);
                item->setFlags(item->flags() | Qt::ItemIsEditable);
                menulist->insertItem(row,item);
                row++;
                if (find!=nullptr && fileName==find) {
                    ret=item;
                }
            }
        }
    }
    if (nextNumToUse<0) { // check if there is an existing empty slot
        // if not then the next available slot is at the end. CHeck if at max files.
        if (count<maxFiles) {
            // a slot is available
            nextNumToUse=count;
        }
    }
    enableButtons(false);
    menulist->sortItems();
    return ret;
}


void SaveGraphLayoutSettings::exit() {
    menuDialog->close();
}

void SaveGraphLayoutSettings::menu(gGraphView* graphView) {
    if (dir==nullptr) {
        //const char* err=qPrintable(QString("Cannot find directory %1").arg(dirName));
        //qWarning(err);
        return;
    }
    this->graphView=graphView;
    updateFileList();
    menuDialog->raise();
    menuDialog->exec();
    exit();
}

//====================================================================================================
//====================================================================================================
// Descriptions map class with file storage

DescriptionMap::DescriptionMap(QDir* dir, QString _filename)
{
    filename  = dir->absoluteFilePath(_filename);
    parseDescriptionsRe = new QRegularExpression(QString("^\\s*(\\w+)%1(.*)$").arg(delimiter) );
};

DescriptionMap::~DescriptionMap() {
    delete parseDescriptionsRe;
};

void DescriptionMap::add(QString key,QString desc) {
    descriptions.insert(key,desc);
};

void DescriptionMap::remove(QString key) {
    descriptions.remove(key);
}
QString DescriptionMap::get(QString key) {
    QString ret =descriptions.value(key,key);
    return ret;
}

void DescriptionMap::save() {
    QFile file(filename);
    file.open(QFile::WriteOnly);
    QTextStream out(&file);
    QMapIterator<QString, QString>it(descriptions);
    while (it.hasNext()) {
        it.next();
        QString line=QString("%1%2%3\n").arg(it.key()).arg(delimiter).arg(it.value());
        out <<line;
    }
    file.close();
}

void DescriptionMap::load() {
    QString line;
    QFile file(filename);
    descriptions.clear();
    if (!file.exists()) return;
    file.open(QFile::ReadOnly);
    QTextStream instr(&file);
    while (instr.readLineInto(&line)) {
        QRegularExpressionMatch match = parseDescriptionsRe->match(line);
        if (match.hasMatch()) {
            QString fileName = match.captured(1);
            QString desc = match.captured(2);
            add(fileName,desc);
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


    menuDialog->connect(menulist, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(itemActivated(QListWidgetItem*)   ));
    menuDialog->connect(menulist, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(itemDoubleClicked(QListWidgetItem*)   ));
    menuDialog->connect(menulist, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(itemClicked(QListWidgetItem*)   ));
    menuDialog->connect(menulist, SIGNAL(itemEntered(QListWidgetItem*)), this, SLOT(itemEntered(QListWidgetItem*)   ));
    menuDialog->connect(menulist, SIGNAL(itemPressed(QListWidgetItem*)), this, SLOT(itemEntered(QListWidgetItem*)   ));



    menuDialog->disconnect(menulist, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(itemActivated(QListWidgetItem*)   ));
    menuDialog->disconnect(menulist, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(itemDoubleClicked(QListWidgetItem*)   ));
    menuDialog->disconnect(menulist, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(itemClicked(QListWidgetItem*)   ));
    menuDialog->disconnect(menulist, SIGNAL(itemEntered(QListWidgetItem*)), this, SLOT(itemEntered(QListWidgetItem*)   ));
    menuDialog->disconnect(menulist, SIGNAL(itemPressed(QListWidgetItem*)), this, SLOT(itemEntered(QListWidgetItem*)   ));


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

