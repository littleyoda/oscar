/* user graph settings Implementation
 *
 * Copyright (c) 2022-2023 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QMessageBox>
#include <QAbstractButton>
#include <QPixmap>
#include <QSize>
#include <QChar>
#include "SleepLib/profiles.h"
#include "saveGraphLayoutSettings.h"

#define USE_FRAMELESS_WINDOW
#define USE_PROFILE_SPECIFIC_FOLDERoff      // off implies saved layouts worked for all profiles.

SaveGraphLayoutSettings::SaveGraphLayoutSettings(QString title,QWidget* parent) : parent(parent),title(title)
{
    fontSizeIncrease = 1;
    helpFontSizeIncrease = 1;
    createSaveFolder();
    if (dir==nullptr) return;
    dir->setFilter(QDir::Files | QDir::Readable | QDir::Writable | QDir::NoSymLinks);

    QString descFileName = dirName+title.toLower()+".descriptions.txt";
    descriptionMap = new DescriptionMap (dir,descFileName);

    createMenu() ;

    menuDialog->connect(menuAddFullBtn, SIGNAL(clicked()), this, SLOT (addFull_feature()  ));
    menuDialog->connect(menuAddBtn,     SIGNAL(clicked()), this, SLOT (add_feature()  ));
    menuDialog->connect(menuRestoreBtn, SIGNAL(clicked()), this, SLOT (restore_feature()  ));
    menuDialog->connect(menuUpdateBtn,  SIGNAL(clicked()), this, SLOT (update_feature()  ));
    menuDialog->connect(menuRenameBtn,  SIGNAL(clicked()), this, SLOT (rename_feature()  ));
    menuDialog->connect(menuDeleteBtn,  SIGNAL(clicked()), this, SLOT (delete_feature()  ));
    menuDialog->connect(menuHelpBtn,    SIGNAL(clicked()), this, SLOT (help_feature()  ));
    menuDialog->connect(menuExitBtn,    SIGNAL(clicked()), this, SLOT (exit()  ));
    menuDialog->connect(menuList,       SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)   ));
    menuDialog->connect(menuList,       SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChanged()   ));



    singleLineRe = new QRegularExpression( QString("^\\s*([^\\r\\n]{0,%1})").arg(1+maxDescriptionLen) );
    fileNumRe = new QRegularExpression( QString("%1(\\d+)(.shg)?$").arg(fileBaseName) );
    parseFilenameRe = new QRegularExpression(QString("^(%1[.](%2(\\d*)))[.]shg$").arg(title).arg(fileBaseName));
}

SaveGraphLayoutSettings::~SaveGraphLayoutSettings()
{

    if (dir==nullptr) {return;}
    menuDialog->disconnect(menuAddFullBtn, SIGNAL(clicked()), this, SLOT (addFull_feature()  ));
    menuDialog->disconnect(menuAddBtn,     SIGNAL(clicked()), this, SLOT (add_feature()  ));
    menuDialog->disconnect(menuRestoreBtn, SIGNAL(clicked()), this, SLOT (restore_feature()  ));
    menuDialog->disconnect(menuUpdateBtn,  SIGNAL(clicked()), this, SLOT (update_feature()  ));
    menuDialog->disconnect(menuRenameBtn,  SIGNAL(clicked()), this, SLOT (rename_feature()  ));
    menuDialog->disconnect(menuDeleteBtn,  SIGNAL(clicked()), this, SLOT (delete_feature()  ));
    menuDialog->disconnect(menuExitBtn,    SIGNAL(clicked()), this, SLOT (exit()  ));
    menuDialog->disconnect(menuHelpBtn,    SIGNAL(clicked()), this, SLOT (help_feature()  ));
    menuDialog->disconnect(menuList,       SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)   ));
    menuDialog->disconnect(menuList,       SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChanged()   ));
    helpDestructor();


    delete descriptionMap;
    delete singleLineRe;
    delete fileNumRe;
    delete parseFilenameRe;
}

void SaveGraphLayoutSettings::createSaveFolder() {
    // Insure that the save folder exists
    // Get the directory name for the save files
    //QString layoutFileFolder = "savedGraphLayoutSettings/";
    QString layoutFileFolder = "layoutSettings/";

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

QPushButton*  SaveGraphLayoutSettings::menuBtn(QString name, QIcon* icon, QString style,QSizePolicy::Policy hPolicy,QString tooltip) {
    return newBtnRtn(menuLayoutButtons, name, icon, style, hPolicy,tooltip) ;
}
QPushButton*  SaveGraphLayoutSettings::helpBtn(QString name, QIcon* icon, QString style,QSizePolicy::Policy hPolicy,QString tooltip) {
    return newBtnRtn(helpLayoutButtons, name, icon, style, hPolicy,tooltip) ;
}
QPushButton*  SaveGraphLayoutSettings::newBtnRtn(QHBoxLayout* layout,QString name, QIcon* icon, QString style,QSizePolicy::Policy hPolicy,QString tooltip) {
    QPushButton* button = new QPushButton(name, menuDialog);
    button->setIcon(*icon);
    button->setStyleSheet(style);
    button->setSizePolicy(hPolicy,QSizePolicy::Fixed);
    button->setToolTip(tooltip);
    button->setToolTipDuration(AppSetting->tooltipTimeout());
    button->setFont(*mediumfont);
    layout->addWidget(button);
    return button;
}

void SaveGraphLayoutSettings::createStyleSheets() {
    styleOn=        calculateButtonStyle(true,false);
    styleOff=       calculateButtonStyle(false,false);
    styleExit=      calculateButtonStyle(true,true);
    styleMessageBox=
            "QMessageBox   { "
                "background-color:yellow;"
                //"color:black;"
                "color:red;"
                "border: 2px solid black;"
                "text-align: right;"
                "font-size: 16px;"
            "}"
            "QTextEdit {"
                "background-color:lightblue;"
                //"color:black;"
                "color:red;"
                "border: 9px solid black;"
                "text-align: center;"
                "vertical-align:top;"
            "}"
            ;
    styleDialog=
            "QDialog { "
                "border: 3px solid black;"
            "}";
}


void SaveGraphLayoutSettings::createMenu() {

    menuListFont =*defaultfont;
    menuListFont.setPointSize(fontSizeIncrease+menuListFont.pointSize());


    createStyleSheets();

    #ifdef USE_FRAMELESS_WINDOW
        menuDialog= new QDialog(parent, Qt::FramelessWindowHint);
        menuDialog->setSizeGripEnabled(true);       // allows lower right hand corner to resize dialog
    #else
        menuDialog= new QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint|Qt::WindowSystemMenuHint);
        menuDialog->setWindowTitle(tr("Manage Save Layout Settings")) ;
    #endif
    menuDialog->setStyleSheet(styleDialog);

    menuLayout  = new QVBoxLayout(menuDialog);
    menuLayoutButtons = new QHBoxLayout();

    menuAddFullBtn = menuBtn(tr("Add"    ),m_icon_add     , styleOff ,QSizePolicy::Minimum, tr("Add Feature inhibited. The maximum number of Items has been exceeded.") );
    menuAddBtn     = menuBtn(tr("Add"    ),m_icon_add     , styleOn  ,QSizePolicy::Minimum, tr("creates new copy of current settings."));
    menuRestoreBtn = menuBtn(tr("Restore"),m_icon_restore , styleOn  ,QSizePolicy::Minimum, tr("Restores saved settings from selection."));
    menuRenameBtn  = menuBtn(tr("Rename" ),m_icon_rename  , styleOn  ,QSizePolicy::Minimum, tr("Renames the selection. Must edit existing name then press enter."));
    menuUpdateBtn  = menuBtn(tr("Update" ),m_icon_update  , styleOn  ,QSizePolicy::Minimum, tr("Updates the selection with current settings."));
    menuDeleteBtn  = menuBtn(tr("Delete" ),m_icon_delete  , styleOn  ,QSizePolicy::Minimum, tr("Deletes the selection."));
    menuHelpBtn    = menuBtn(""           ,m_icon_help    , styleOn  ,QSizePolicy::Fixed  , tr("Expanded Help menu."));
    menuExitBtn    = menuBtn(""           ,m_icon_exit    , styleExit,QSizePolicy::Fixed  , tr("Exits the Layout menu."));

    #ifndef USE_FRAMELESS_WINDOW
        menuExitBtn->hide();
    #endif

    menuList = new QListWidget(menuDialog);
    menuList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    menuList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    menuList->setFont(menuListFont);


    menuLayout->addLayout(menuLayoutButtons);
    menuLayout->addWidget(menuList, 1);

    //menuList->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    menuList->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);


};

void SaveGraphLayoutSettings::createHelp() {
    if(helpDialog) return;

    helpDialog= new QDialog(parent, Qt::FramelessWindowHint);
    helpDialog->setStyleSheet(styleDialog);
    menuDialog->setSizeGripEnabled(true);       // allows lower right hand corner to resize dialog

    QFont helpInfoFont = menuListFont;
    helpInfoFont.setPointSize(helpFontSizeIncrease+helpInfoFont.pointSize());

    QFont helpInfoLabelFont = helpInfoFont;
    helpInfoLabelFont.setPointSize(fontSizeIncrease+ helpInfoFont.pointSize());

    QLabel* helpInfoHeaderLabel = new QLabel("helpInfoHeaderLabel",parent);
    helpInfoHeaderLabel->setText(QString( tr("<h4>Help Menu - Manage Layout Settings</h4>")));
    helpInfoHeaderLabel->setFont(helpInfoLabelFont);

    QLabel* helpInfoLabel = new QLabel("helpInfo",parent);
    helpInfoLabel->setFont(helpInfoFont);
    helpInfoLabel->setText(helpInfo()) ;

    helpLayoutButtons = new QHBoxLayout();
    helpLayoutButtons->addWidget(helpInfoHeaderLabel);
    helpInfoExitBtn= helpBtn(""          ,m_icon_return  , styleOn  ,QSizePolicy::Fixed  , tr("Exits the help menu."));
    helpExitBtn    = helpBtn(""          ,m_icon_exit    , styleExit,QSizePolicy::Fixed  , tr("Exits the dialog menu."));

    QVBoxLayout* helpLayout = new QVBoxLayout(helpDialog);
    helpLayout->addLayout(helpLayoutButtons);
    helpLayout->addWidget(helpInfoLabel, 1);

    helpDialog->connect(helpInfoExitBtn,SIGNAL(clicked()), this, SLOT (help_off_feature()  ));
    helpDialog->connect(helpExitBtn,    SIGNAL(clicked()), this, SLOT (help_exit_feature()  ));

}

void SaveGraphLayoutSettings::helpDestructor() {
    if(!helpDialog) return;
    helpDialog->disconnect(helpInfoExitBtn,SIGNAL(clicked()), this, SLOT (help_off_feature()  ));
    helpDialog->disconnect(helpExitBtn,    SIGNAL(clicked()), this, SLOT (help_exit_feature()  ));
}

QString SaveGraphLayoutSettings::helpInfo() {
QStringList strList;
    strList<<QString("<p style=\"color:black;\">")
<<QString(tr("This feature manages the saving and restoring of Layout Settings."))
    <<QString("<br>")
<<QString(tr("Layout Settings control the layout of a graph or chart."))
    <<QString("<br>")
<<QString(tr("Different Layouts Settings can be saved and later restored."))
    <<QString("<br> </p> <table width=\"100%\"> <tr><td><b>")
<<QString(tr("Button"))
    <<QString("</b></td> <td><b>")
<<QString(tr("Description"))
    <<QString("</b></td></tr> <tr><td valign=\"top\">")
<<QString(tr("Add"))
    <<QString("</td> <td>")
<<QString(tr("Creates a copy of the current Layout Settings."))
    <<QString("<br>")
<<QString(tr("The default description is the current date."))
    <<QString("<br>")
<<QString(tr("The description may be changed."))
    <<QString("<br>")
<<QString(tr("The Add button will be greyed out when maximum number is reached."))
    <<QString("</td></tr> <br> <tr><td><i><u>")
<<QString(tr("Other Buttons"))
    <<QString("</u> </i></td>  <td>")
<<QString(tr("Greyed out when there are no selections"))
    <<QString("</td></tr> <tr><td>")
<<QString(tr("Restore"))
    <<QString("</td> <td>")
<<QString(tr("Loads the Layout Settings from the selection. Automatically exits."))
    <<QString("</td></tr> <tr><td>")
<<QString(tr("Rename"))
    <<QString("</td><td>")
<<QString(tr("Modify the description of the selection. Same as a double click."))
    <<QString("</td></tr> <tr><td valign=\"top\">")
<<QString(tr("Update"))
    <<QString("</td><td>")
<<QString(tr("Saves the current Layout Settings to the selection."))
    <<QString("<br>")
<<QString(tr("Prompts for confirmation."))
    <<QString("</td></tr> <tr><td valign=\"top\">")
<<QString(tr("Delete"))
    <<QString("</td> <td>")
<<QString(tr("Deletes the selecton."))
    <<QString("<br>")
<<QString(tr("Prompts for confirmation."))
    <<QString("</td></tr> <tr><td><i><u>")
<<QString(tr("Control"))
    <<QString("</u> </i></td> <td></td></tr> <tr><td>")
<<QString(tr("Exit"))
    <<QString("</td> <td>")
<<QString(tr("(Red circle with a white \"X\".) Returns to OSCAR menu."))
    <<QString("</td></tr> <tr><td>")
<<QString(tr("Return"))
    <<QString("</td> <td>")
<<QString(tr("Next to Exit icon. Only in Help Menu. Returns to Layout menu."))
    <<QString("</td></tr> <tr><td>")
<<QString(tr("Escape Key"))
    <<QString("</td> <td>")
<<QString(tr("Exit the Help or Layout menu."))
    <<QString("</td></tr> </table> <p><b>")
<<QString(tr("Layout Settings"))
    <<QString("</b></p> <table width=\"100%\"> <tr> <td>")
<<QString(tr("* Name"))
    <<QString("</td> <td>")
<<QString(tr("* Pinning"))
    <<QString("</td> <td>")
<<QString(tr("* Plots Enabled"))
    <<QString("</td> <td>")
<<QString(tr("* Height"))
    <<QString("</td> </tr> <tr> <td>")
<<QString(tr("* Order"))
    <<QString("</td> <td>")
<<QString(tr("* Event Flags"))
    <<QString("</td> <td>")
<<QString(tr("* Dotted Lines"))
    <<QString("</td> <td>")
<<QString(tr("* Height Options"))
    <<QString("</td> </tr> </table> <p><b>")
<<QString(tr("General Information"))
    <<QString("</b></p> <ul style=margin-left=\"20\"; > <li>")
<<QString(tr("Maximum description size = 80 characters.	"))
    <<QString("</li> <li>")
<<QString(tr("Maximum Saved Layout Settings = 30.	"))
    <<QString("</li> <li>")
<<QString(tr("Saved Layout Settings can be accessed by all profiles."))
    <<QString("<li>")
<<QString(tr("Layout Settings only control the layout of a graph or chart."))
    <<QString("<br>")
<<QString(tr("They do not contain any other data."))
    <<QString("<br>")
<<QString(tr("They do not control if a graph is displayed or not."))
    <<QString("</li> <li>")
<<QString(tr("Layout Settings for daily and overview are managed independantly."))
    <<QString("</li> </ul>");
return strList.join("\n");
};

const QString  SaveGraphLayoutSettings::calculateStyleMessageBox(QFont* font , QString& s1, QString& s2) {
    QFontMetrics fm = QFontMetrics(*font);
    int width = fm.boundingRect(s1).size().width();
    int width2 = fm.boundingRect(s2).size().width();
    width = qMax(width,width2) + iconWidthMessageBox;

    QString style = QString("%1 QLabel{%2 min-width: %3px;}" ).
            arg(styleMessageBox).
            arg("text-align: center;" "color:black;").
            arg(width);

    return style;
}

bool SaveGraphLayoutSettings::verifyItem(QListWidgetItem* item,QString text,QIcon* icon) {
    //if (verifyhelp() ) return false;
    if (item) return true;
    initminMenuListSize();
    confirmAction( text ,"" , icon);
    return false;
}

#if 1
bool SaveGraphLayoutSettings::confirmAction(QString top ,QString bottom,QIcon* icon,QMessageBox::StandardButtons flags , QMessageBox::StandardButton adefault, QMessageBox::StandardButton success) {
    //QString topText=QString("<Center>%1</Center>").arg(top);
    //QString bottomText=QString("<Center>%1</Center>").arg(bottom);
    QString topText=QString("<p style=""text-align:center"">%1</p>").arg(top);
    QString bottomText=QString("<p style=""text-align:center"">%1</p>").arg(bottom);

    QMessageBox msgBox(menuDialog);
    msgBox.setText(topText);
    msgBox.setInformativeText(bottomText);
    if (icon!=nullptr) {
        msgBox.setIconPixmap(icon->pixmap(QSize(iconWidthMessageBox,iconWidthMessageBox)));
    }
    // may be good for help menu. a pull down box with box of data.  msgBox.setDetailedText("some detailed string");
    msgBox.setStandardButtons(flags);
    msgBox.setDefaultButton(adefault);
    msgBox.setWindowFlag(Qt::FramelessWindowHint,true);

    msgBox.setStyleSheet(calculateStyleMessageBox(&menuListFont,top,bottom));

    // displaywidgets((QWidget*)&msgBox);
    bool ret= msgBox.exec()==success;
    return ret;
}
#else
bool SaveGraphLayoutSettings::confirmAction(QString name ,QString question,QIcon* icon,QMessageBox::StandardButtons flags , QMessageBox::StandardButton adefault, QMessageBox::StandardButton success) {
//bool SaveGraphLayoutSettings::confirmAction(QString name,QString question,QIcon* icon) 
    Q_UNUSED(flags);
    Q_UNUSED(adefault);
    Q_UNUSED(success);
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
#endif

QString SaveGraphLayoutSettings::calculateButtonStyle(bool on,bool exitBtn){
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
    //button->setEnabled(on);
    button->setStyleSheet(on?styleOn:styleOff);
}

void SaveGraphLayoutSettings::manageButtonApperance() {

    bool enable = menuList->currentItem();
    looksOn(menuUpdateBtn,enable);
    looksOn(menuRestoreBtn,enable);
    looksOn(menuDeleteBtn,enable);
    looksOn(menuRenameBtn,enable);

    if (nextNumToUse<0) {  // check if at Maximum limit
        // must always hide first
        menuAddBtn->hide();
        menuAddFullBtn->show();
    } else {
        // must always hide first
        menuAddFullBtn->hide();
        menuAddBtn->show();
    }
}


void SaveGraphLayoutSettings::add_feature() {
    if(!graphView) return;
    QString fileName = QString("%1%2").arg(fileBaseName).arg(nextNumToUse,fileNumMaxLength,10,QLatin1Char('0'));
    writeSettings(fileName);
    // create a default description - use formatted datetime.
    QString desc=QDateTime::currentDateTime().toString();
    descriptionMap->add(fileName,desc);
    descriptionMap->save();
    QListWidgetItem* item = updateFileList( fileName);
    if (item!=nullptr) {
        menuList->setCurrentItem(item,QItemSelectionModel::ClearAndSelect);
        menuList->editItem(item);
    }
    menuList->sortItems();
    resizeMenu();
}

void SaveGraphLayoutSettings::addFull_feature() {
    verifyItem( nullptr,tr("Maximum number of Items exceeded.") , m_icon_add);
}

void SaveGraphLayoutSettings::update_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=menuList->currentItem();
    if (!verifyItem(item,  tr("No Item Selected") ,  m_icon_update)) return ;
    if(!confirmAction( item->text(), tr("Ok to Update?") , m_icon_update) ) return;
    QString fileName = item->data(fileNameRole).toString();
    writeSettings(fileName);
};

void SaveGraphLayoutSettings::restore_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=menuList->currentItem();
    if (!verifyItem(item,  tr("No Item Selected") ,  m_icon_restore)) return ;
    QString fileName = item->data(fileNameRole).toString();
    loadSettings(fileName);
    exit();
};

void SaveGraphLayoutSettings::rename_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=menuList->currentItem();
    if (!verifyItem(item,  tr("No Item Selected") ,  m_icon_rename)) return ;
    menuList->editItem(item);
    // SaveGraphLayoutSettings::itemChanged(QListWidgetItem *item) is called when edit changes the entry.
    // itemChanged will update the description map
}

void SaveGraphLayoutSettings::help_exit_feature() {
    helpDialog->close();
    exit();
}

void SaveGraphLayoutSettings::help_off_feature() {
    helpDialog->close();
}

void SaveGraphLayoutSettings::help_feature() {
    initminMenuListSize();
    createHelp();
    if(!helpDialog) return;
    helpDialog->raise();
    helpDialog->exec();
    manageButtonApperance();
}

void SaveGraphLayoutSettings::delete_feature() {
    if(!graphView) return;
    QListWidgetItem *	item=menuList->currentItem();
    if (!verifyItem(item,  tr("No Item Selected") ,  m_icon_delete)) return ;
    if(!confirmAction(item->text(),  tr("Ok To Delete?") ,m_icon_delete) ) return;

    QString fileName = item->data(fileNameRole).toString();
    descriptionMap->remove(fileName);
    descriptionMap->save();
    deleteSettings(fileName);
    delete item;
    if (nextNumToUse<0) {
        nextNumToUse=fileNum(fileName);
    }
    manageButtonApperance();
    resizeMenu();
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
    menuList->sortItems();
    menuList->setCurrentItem(item);
    resizeMenu();

}

void SaveGraphLayoutSettings::itemSelectionChanged()
{
    initminMenuListSize();
    manageButtonApperance();
}

void SaveGraphLayoutSettings::initminMenuListSize() {
    if (minMenuDialogSize.width()==0) {
        menuDialogSize     = menuDialog->size();
        minMenuDialogSize  = menuDialogSize;

        menuListSize       = menuList->size();
        minMenuListSize    = menuListSize;

        dialogListDiff     = menuDialogSize - menuListSize;

        dialogListDiff.setWidth (horizontalWidthAdjustment + dialogListDiff.width());

        resizeMenu();
    }
};

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


QSize  SaveGraphLayoutSettings::maxSize(const QSize AA , const QSize BB ) {
    return QSize (  qMax(AA.width(),BB.width()) , qMax(AA.height(),BB.height() ) );
}

bool  SaveGraphLayoutSettings::sizeEqual(const QSize AA , const QSize BB ) {
    return  (AA.width()==BB.width()) && ( AA.height()==BB.height()) ;
}



void SaveGraphLayoutSettings::resizeMenu() {
    if (minMenuDialogSize.width()==0) return; 

    QSize newSize = calculateMenuDialogSize();
    newSize.setWidth ( newSize.width());

    menuDialogSize = menuDialog->size();

    if ( sizeEqual(newSize , menuDialogSize)) {
        // no work to do 
        return;
    };

    if ( menuDialogSize.width() < newSize.width() )  {
        menuDialog->setMinimumWidth(newSize.width());
        menuDialog->setMaximumWidth(QWIDGETSIZE_MAX);
    } else if ( menuDialogSize.width() > newSize.width() )  {
        menuDialog->setMaximumWidth(newSize.width());
        menuDialog->setMinimumWidth(newSize.width());
    }
    if ( menuDialogSize.height() < newSize.height() )  {
        menuDialog->setMinimumHeight(newSize.height());
        menuDialog->setMaximumHeight(QWIDGETSIZE_MAX);
    } else if ( menuDialogSize.height() > newSize.height() )  {
        menuDialog->setMaximumHeight(newSize.height());
        menuDialog->setMinimumHeight(newSize.height());
    }
    menuDialogSize = newSize;
}

QSize SaveGraphLayoutSettings::calculateMenuDialogSize() {
    if (menuDialogSize.width()==0) return QSize(0,0);
    QListWidgetItem* item;
    widestItem=nullptr;
    QFontMetrics fm = QFontMetrics(menuListFont);

    // account for scrollbars.
    QSize returnValue = QSize( 0 , fm.height() );  // add an extra space at the bottom. width didn't work

    // Account for dialog Size
    returnValue += dialogListDiff;
    QSize size;

    for (int index = 0; index < menuList->count(); ++index) {
        item = menuList->item(index);
        if (!item) continue;
        size  = fm.boundingRect(item->text()).size();
        if (returnValue.width() < size.width()) {
            returnValue.setWidth( qMax( returnValue.width(),size.width()));
            widestItem=item;
        }
        returnValue.setHeight( returnValue.height()+size.height());
    }
    returnValue.setWidth( horizontalWidthAdjustment +  returnValue.width() ) ;

    returnValue = maxSize(returnValue, minMenuDialogSize);
    return returnValue;
}

QListWidgetItem* SaveGraphLayoutSettings::updateFileList(QString find) {
    QListWidgetItem* ret=nullptr;
    manageButtonApperance();
    dir->refresh();
    QFileInfoList filelist = dir->entryInfoList( QDir::Files | QDir::Readable | QDir::Writable | QDir::NoSymLinks,QDir::Name);

    // Restrict number of files. easy to find availble unused entry for add function.

    int row=0;
    int count=0;
    menuList->clear();
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
                menuList->insertItem(row,item);
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
    manageButtonApperance();
    menuList->sortItems();
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
    manageButtonApperance();
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

    Different languages unicodes to test. optained from translation files

    도움주신분들
    已成功删除
    删除
    הצג את מחיצת הנתונים

    הצג את מחיצת הנתונים  已成功删除 عذرا ، لا يمكن تحديد موقع ملف.
    已成功删除 عذرا ، لا يمكن تحديد موقع ملف.  删除
    Toon gegevensmap
    عذرا ، لا يمكن تحديد موقع ملف.

#endif

