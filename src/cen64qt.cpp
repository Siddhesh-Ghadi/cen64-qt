/***
 * Copyright (c) 2013, Presence
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the organization nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#include "cen64qt.h"
#include "aboutdialog.h"
#include "global.h"
#include "settingsdialog.h"


CEN64Qt::CEN64Qt(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("CEN64-Qt"));
    setWindowIcon(QIcon(":/images/cen64.png"));

    setupDatabase();

    romPath = SETTINGS.value("Paths/roms","").toString();
    romDir = QDir(romPath);

    widget = new QWidget(this);
    setCentralWidget(widget);
    setGeometry(QRect(SETTINGS.value("Geometry/windowx", 0).toInt(),
                      SETTINGS.value("Geometry/windowy", 0).toInt(),
                      SETTINGS.value("Geometry/width", 900).toInt(),
                      SETTINGS.value("Geometry/height", 600).toInt()));

    createMenu();
    createRomView();

    statusBar = new QStatusBar;

    if (SETTINGS.value("View/statusbar", "").toString() == "")
        statusBar->hide();

    layout = new QVBoxLayout(widget);
    layout->setMenuBar(menuBar);

    layout->addWidget(emptyView);
    layout->addWidget(romTree);
    layout->addWidget(gridView);
    layout->addWidget(listView);

    layout->addWidget(statusBar);
    layout->setMargin(0);

    widget->setLayout(layout);
    widget->setMinimumSize(300, 200);
}


Rom CEN64Qt::addRom(QString fileName, QString zipFile, qint64 size, QSqlQuery query)
{
    Rom currentRom;

    currentRom.fileName = fileName;
    currentRom.internalName = QString(romData->mid(32, 20)).trimmed();
    currentRom.romMD5 = QString(QCryptographicHash::hash(*romData,
                                QCryptographicHash::Md5).toHex());
    currentRom.zipFile = zipFile;
    currentRom.sortSize = (int)size;

    query.bindValue(":filename",      currentRom.fileName);
    query.bindValue(":internal_name", currentRom.internalName);
    query.bindValue(":md5",           currentRom.romMD5);
    query.bindValue(":zip_file",      currentRom.zipFile);
    query.bindValue(":size",          currentRom.sortSize);
    query.exec();

    initializeRom(&currentRom, false);

    return currentRom;
}


void CEN64Qt::addRoms()
{
    database.open();

    QSqlQuery query("DELETE FROM rom_collection", database);
    query.prepare(QString("INSERT INTO rom_collection ")
                  + "(filename, internal_name, md5, zip_file, size) "
                  + "VALUES (:filename, :internal_name, :md5, :zip_file, :size)");

    QList<Rom> roms;

    QStringList tableVisible = SETTINGS.value("Table/columns", "Filename|Size").toString().split("|");
    resetLayouts(tableVisible);

    romTree->setEnabled(false);
    gridView->setEnabled(false);
    listView->setEnabled(false);
    romTree->clear();
    downloadAction->setEnabled(false);
    startAction->setEnabled(false);
    stopAction->setEnabled(false);

    if (romPath != "") {
        if (romDir.exists()) {
            QStringList files = romDir.entryList(QStringList() << "*.z64" << "*.n64" << "*.zip",
                                                 QDir::Files | QDir::NoSymLinks);

            if (files.size() > 0) {
                setupProgressDialog(files.size());

                int count = 0;
                foreach (QString fileName, files)
                {
                    QString completeFileName = romDir.absoluteFilePath(fileName);
                    QFile file(completeFileName);

                    //If file is a zip file, extract info from any zipped ROMs
                    if (QFileInfo(file).suffix().toLower() == "zip") {
                        QuaZip zipFile(completeFileName);
                        zipFile.open(QuaZip::mdUnzip);

                        foreach(QString zippedFile, zipFile.getFileNameList())
                        {
                            QString ext = zippedFile.right(4).toLower();

                            //check for ROM files
                            if (ext == ".z64" || ext == ".n64") {
                                QuaZipFile zippedRomFile(completeFileName, zippedFile);

                                zippedRomFile.open(QIODevice::ReadOnly);
                                romData = new QByteArray(zippedRomFile.readAll());
                                qint64 size = zippedRomFile.usize();
                                zippedRomFile.close();

                                if (romData->left(4).toHex() == "80371240") //Else v64 or invalid
                                    roms.append(addRom(zippedFile, fileName, size, query));

                                delete romData;
                            }
                        }

                        zipFile.close();
                    } else { //Just a normal ROM file
                        file.open(QIODevice::ReadOnly);
                        romData = new QByteArray(file.readAll());
                        file.close();

                        qint64 size = QFileInfo(file).size();

                        if (romData->left(4).toHex() == "80371240") //Else v64 or invalid
                            roms.append(addRom(fileName, "", size, query));

                        delete romData;
                    }

                    count++;
                    progress->setValue(count);
                }

                progress->close();
            } else {
            QMessageBox::warning(this, "Warning", "No ROMs found.");
            }
        } else {
            QMessageBox::warning(this, "Warning", "Failed to open ROM directory.");
        }
    }

    qSort(roms.begin(), roms.end(), romSorter);

    int i = 0;
    foreach (Rom currentRom, roms)
    {
        if (SETTINGS.value("View/layout", "None") == "Table View")
            addToTableView(&currentRom);
        else if (SETTINGS.value("View/layout", "None") == "Grid View")
            addToGridView(&currentRom, i);
        else if (SETTINGS.value("View/layout", "None") == "List View")
            addToListView(&currentRom, i);

        i++;
    }

    if (roms.size() != 0) {
        if (tableVisible.join("") != "")
            romTree->setEnabled(true);

        gridView->setEnabled(true);
        listView->setEnabled(true);
    }

    database.close();
}


void CEN64Qt::addToGridView(Rom *currentRom, int count)
{
    ClickableWidget *gameGridItem = new ClickableWidget(gridWidget);
    gameGridItem->setMinimumHeight(getGridSize("height"));
    gameGridItem->setMaximumHeight(getGridSize("height"));
    gameGridItem->setMinimumWidth(getGridSize("width"));
    gameGridItem->setMaximumWidth(getGridSize("width"));
    gameGridItem->setGraphicsEffect(getShadow(false));

    //Assign ROM data to widget for use in click events
    gameGridItem->setProperty("fileName", currentRom->fileName);
    if (currentRom->goodName == "Unknown ROM" || currentRom->goodName == "Requires catalog file")
        gameGridItem->setProperty("search", currentRom->internalName);
    else
        gameGridItem->setProperty("search", currentRom->goodName);
    gameGridItem->setProperty("romMD5", currentRom->romMD5);
    gameGridItem->setProperty("zipFile", currentRom->zipFile);

    QGridLayout *gameGridLayout = new QGridLayout(gameGridItem);
    gameGridLayout->setColumnStretch(0, 1);
    gameGridLayout->setColumnStretch(3, 1);
    gameGridLayout->setRowMinimumHeight(1, getImageSize("Grid").height());

    QLabel *gridImageLabel = new QLabel(gameGridItem);
    gridImageLabel->setMinimumHeight(getImageSize("Grid").height());
    gridImageLabel->setMinimumWidth(getImageSize("Grid").width());
    QPixmap image;

    if (currentRom->imageExists)
        image = currentRom->image.scaled(getImageSize("Grid"), Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
    else
        image = QPixmap(":/images/not-found.png").scaled(getImageSize("Grid"), Qt::IgnoreAspectRatio,
                                                         Qt::SmoothTransformation);

    gridImageLabel->setPixmap(image);
    gameGridLayout->addWidget(gridImageLabel, 1, 1);

    if (SETTINGS.value("Grid/label","true") == "true") {
        QLabel *gridTextLabel = new QLabel(gameGridItem);

        //Don't allow label to be wider than image
        gridTextLabel->setMaximumWidth(getImageSize("Grid").width());

        QString text = "";
        QString labelText = SETTINGS.value("Grid/labeltext","Filename").toString();

        text = getRomInfo(labelText, currentRom);

        gridTextLabel->setText(text);

        QString textHex = getColor(SETTINGS.value("Grid/labelcolor","White").toString()).name();
        int fontSize = getGridSize("font");

#ifdef Q_OS_OSX //OSX is funky with the label text
        if (text.length() > 30)
            fontSize -= 2;
#endif

        gridTextLabel->setStyleSheet("QLabel { font-weight: bold; color: " + textHex + "; font-size: "
                                     + QString::number(fontSize) + "px; }");
        gridTextLabel->setWordWrap(true);
        gridTextLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

        gameGridLayout->addWidget(gridTextLabel, 2, 1);
    }

    gameGridItem->setLayout(gameGridLayout);

    int columnCount = SETTINGS.value("Grid/columncount", "4").toInt();
    gridLayout->addWidget(gameGridItem, count / columnCount + 1, count % columnCount + 1);
    gridWidget->adjustSize();

    connect(gameGridItem, SIGNAL(singleClicked(QWidget*)), this, SLOT(highlightGridWidget(QWidget*)));
    connect(gameGridItem, SIGNAL(doubleClicked(QWidget*)), this, SLOT(runEmulatorFromWidget(QWidget*)));
}


void CEN64Qt::addToListView(Rom *currentRom, int count)
{
    QStringList visible = SETTINGS.value("List/columns", "Filename|Internal Name|Size").toString().split("|");

    if (visible.join("") == "" && SETTINGS.value("List/displaycover", "") != "true")
        //Otherwise no columns, so don't bother populating
        return;

    ClickableWidget *gameListItem = new ClickableWidget(listWidget);
    gameListItem->setContentsMargins(0, 0, 20, 0);

    //Assign ROM data to widget for use in click events
    gameListItem->setProperty("fileName", currentRom->fileName);
    if (currentRom->goodName == "Unknown ROM" || currentRom->goodName == "Requires catalog file")
        gameListItem->setProperty("search", currentRom->internalName);
    else
        gameListItem->setProperty("search", currentRom->goodName);
    gameListItem->setProperty("romMD5", currentRom->romMD5);
    gameListItem->setProperty("zipFile", currentRom->zipFile);

    QGridLayout *gameListLayout = new QGridLayout(gameListItem);
    gameListLayout->setColumnStretch(3, 1);

    //Add image
    if (SETTINGS.value("List/displaycover", "") == "true") {
        QLabel *listImageLabel = new QLabel(gameListItem);

        QPixmap image;

        if (currentRom->imageExists)
            image = currentRom->image.scaled(getImageSize("List"), Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
        else
            image = QPixmap(":/images/not-found.png").scaled(getImageSize("List"), Qt::KeepAspectRatio,
                                                             Qt::SmoothTransformation);

        listImageLabel->setPixmap(image);

        gameListLayout->addWidget(listImageLabel, 0, 1);
    }

    //Create text label
    QLabel *listTextLabel = new QLabel("", gameListItem);
    QString listText = "";

    int i = 0;

    foreach (QString current, visible)
    {
        QString addition = "<style>h2 { margin: 0; }</style>";

        if (i == 0 && SETTINGS.value("List/firstitemheader","true") == "true")
            addition += "<h2>";
        else
            addition += "<b>" + current + ":</b> ";

        addition += getRomInfo(current, currentRom, true) + "<br />";

        if (i == 0 && SETTINGS.value("List/firstitemheader","true") == "true")
            addition += "</h2>";

        if (addition != "<style>h2 { margin: 0; }</style><b>" + current + ":</b> <br />")
            listText += addition;

        i++;
    }

    //Remove last break tag
    listText.remove(QRegExp("<br />$"));

    listTextLabel->setText(listText);
    listTextLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    listTextLabel->setWordWrap(true);
    gameListLayout->addWidget(listTextLabel, 0, 3);

    gameListLayout->setColumnMinimumWidth(0, 20);
    gameListLayout->setColumnMinimumWidth(2, 10);
    gameListItem->setLayout(gameListLayout);

    if (count != 0) {
        QFrame *separator = new QFrame();
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet("margin:0;padding:0;");
        listLayout->addWidget(separator);
    }

    listLayout->addWidget(gameListItem);

    connect(gameListItem, SIGNAL(singleClicked(QWidget*)), this, SLOT(highlightListWidget(QWidget*)));
    connect(gameListItem, SIGNAL(doubleClicked(QWidget*)), this, SLOT(runEmulatorFromWidget(QWidget*)));
}


void CEN64Qt::addToTableView(Rom *currentRom)
{
    QStringList visible = SETTINGS.value("Table/columns", "Filename|Size").toString().split("|");

    if (visible.join("") == "") //Otherwise no columns, so don't bother populating
        return;

    fileItem = new TreeWidgetItem(romTree);

    //Filename for launching ROM
    fileItem->setText(0, currentRom->fileName);

    //GoodName or Internal Name for searching
    if (currentRom->goodName == "Unknown ROM" || currentRom->goodName == "Requires catalog file")
        fileItem->setText(1, currentRom->internalName);
    else
        fileItem->setText(1, currentRom->goodName);

    //MD5 for cache info
    fileItem->setText(2, currentRom->romMD5.toLower());

    //Zip file
    fileItem->setText(3, currentRom->zipFile);

    int i = 4, c = 0;
    bool addImage = false;

    foreach (QString current, visible)
    {
        QString text = getRomInfo(current, currentRom);
        fileItem->setText(i, text);

        if (current == "GoodName" || current == "Game Title") {
            if (text == "Unknown ROM" || text == "Requires catalog file" || text == "Not found") {
                fileItem->setForeground(i, QBrush(Qt::gray));
                fileItem->setData(i, Qt::UserRole, "ZZZ"); //end of sorting
            } else
                fileItem->setData(i, Qt::UserRole, text);
        }

        if (current == "Size")
            fileItem->setData(i, Qt::UserRole, currentRom->sortSize);

        if (current == "Release Date")
            fileItem->setData(i, Qt::UserRole, currentRom->sortDate);

        if (current == "Game Cover") {
            c = i;
            addImage = true;
        }

        QStringList center, right;

        center << "MD5" << "CRC1" << "CRC2" << "Rumble" << "ESRB" << "Genre" << "Publisher" << "Developer";
        right << "Size" << "Players" << "Save Type" << "Release Date" << "Rating";

        if (center.contains(current))
            fileItem->setTextAlignment(i, Qt::AlignHCenter | Qt::AlignVCenter);
        else if (right.contains(current))
            fileItem->setTextAlignment(i, Qt::AlignRight | Qt::AlignVCenter);

        i++;
    }

    romTree->addTopLevelItem(fileItem);


    if (currentRom->imageExists && addImage) {
        QPixmap image(currentRom->image.scaled(getImageSize("Table"), Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));

        QWidget *imageContainer = new QWidget(romTree);
        QGridLayout *imageGrid = new QGridLayout(imageContainer);
        QLabel *imageLabel = new QLabel(imageContainer);

        imageLabel->setPixmap(image);
        imageGrid->addWidget(imageLabel, 1, 1);
        imageGrid->setColumnStretch(0, 1);
        imageGrid->setColumnStretch(2, 1);
        imageGrid->setRowStretch(0, 1);
        imageGrid->setRowStretch(2, 1);
        imageGrid->setContentsMargins(0,0,0,0);

        imageContainer->setLayout(imageGrid);

        romTree->setItemWidget(fileItem, c, imageContainer);
    }
}


void CEN64Qt::cachedRoms(bool imageUpdated)
{
    database.open();
    QSqlQuery query("SELECT filename, md5, internal_name, zip_file, size FROM rom_collection", database);

    query.last();
    int romCount = query.at() + 1;
    query.seek(-1);

    if (romCount == -1) { //Nothing cached so try adding ROMs instead
        addRoms();
        return;
    }

    QList<Rom> roms;

    romTree->setEnabled(false);
    gridView->setEnabled(false);
    listView->setEnabled(false);
    romTree->clear();
    downloadAction->setEnabled(false);
    startAction->setEnabled(false);
    stopAction->setEnabled(false);


    //Save position in current layout
    positionx = 0;
    positiony = 0;

    if (SETTINGS.value("View/layout", "None") == "Table View") {
        positionx = romTree->horizontalScrollBar()->value();
        positiony = romTree->verticalScrollBar()->value();
    } else if (SETTINGS.value("View/layout", "None") == "Grid View") {
        positionx = gridView->horizontalScrollBar()->value();
        positiony = gridView->verticalScrollBar()->value();
    } else if (SETTINGS.value("View/layout", "None") == "List View") {
        positionx = listView->horizontalScrollBar()->value();
        positiony = listView->verticalScrollBar()->value();
    }


    QStringList tableVisible = SETTINGS.value("Table/columns", "Filename|Size").toString().split("|");
    resetLayouts(tableVisible, imageUpdated);

    int count = 0;
    bool showProgress = false;
    QTime checkPerformance;

    while (query.next())
    {
        Rom currentRom;

        currentRom.fileName = query.value(0).toString();
        currentRom.romMD5 = query.value(1).toString();
        currentRom.internalName = query.value(2).toString();
        currentRom.zipFile = query.value(3).toString();
        currentRom.sortSize = query.value(4).toInt();

        //Check performance of adding first item to see if progress dialog needs to be shown
        if (count == 0) checkPerformance.start();

        initializeRom(&currentRom, true);
        roms.append(currentRom);

        if (count == 0) {
            int runtime = checkPerformance.elapsed();

            //check if operation expected to take longer than two seconds
            if (runtime * romCount > 2000) {
                setupProgressDialog(romCount);
                showProgress = true;
            }
        }

        count++;

        if (showProgress)
            progress->setValue(count);
    }

    database.close();

    if (showProgress)
        progress->close();

    qSort(roms.begin(), roms.end(), romSorter);

    int i = 0;
    foreach (Rom currentRom, roms)
    {
        if (SETTINGS.value("View/layout", "None") == "Table View")
            addToTableView(&currentRom);
        else if (SETTINGS.value("View/layout", "None") == "Grid View")
            addToGridView(&currentRom, i);
        else if (SETTINGS.value("View/layout", "None") == "List View")
            addToListView(&currentRom, i);

        i++;
    }

    if (roms.size() != 0) {
        if (tableVisible.join("") != "")
            romTree->setEnabled(true);

        gridView->setEnabled(true);
        listView->setEnabled(true);

        QTimer *timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(0);
        timer->start();

        if (SETTINGS.value("View/layout", "None") == "Table View")
            connect(timer, SIGNAL(timeout()), this, SLOT(setTablePosition()));
        else if (SETTINGS.value("View/layout", "None") == "Grid View")
            connect(timer, SIGNAL(timeout()), this, SLOT(setGridPosition()));
        else if (SETTINGS.value("View/layout", "None") == "List View")
            connect(timer, SIGNAL(timeout()), this, SLOT(setListPosition()));
    }
}


void CEN64Qt::checkStatus(int status)
{
    if (status > 0)
        QMessageBox::warning(this, "Warning",
            "CEN64 quit unexpectedly. Check to make sure you are using a valid ROM.");
    else
        statusBar->showMessage("Emulation stopped", 3000);
}


void CEN64Qt::cleanTemp()
{
    QFile::remove(QDir::tempPath() + "/cen64-qt/temp.z64");
}


void CEN64Qt::closeEvent(QCloseEvent *event)
{
    SETTINGS.setValue("Geometry/windowx", geometry().x());
    SETTINGS.setValue("Geometry/windowy", geometry().y());
    SETTINGS.setValue("Geometry/width", geometry().width());
    SETTINGS.setValue("Geometry/height", geometry().height());
    if (isMaximized())
        SETTINGS.setValue("Geometry/maximized", true);
    else
        SETTINGS.setValue("Geometry/maximized", "");

    saveColumnWidths();

    event->accept();
}


void CEN64Qt::createMenu()
{
    menuBar = new QMenuBar(this);


    fileMenu = new QMenu(tr("&File"), this);
    openAction = fileMenu->addAction(tr("&Open ROM..."));
    fileMenu->addSeparator();
    convertAction = fileMenu->addAction(tr("&Convert V64..."));
    refreshAction = fileMenu->addAction(tr("&Refresh List"));
    downloadAction = fileMenu->addAction(tr("&Download/Update Info..."));
#ifndef Q_OS_OSX //OSX does not show the quit action so the separator is unneeded
    fileMenu->addSeparator();
#endif
    quitAction = fileMenu->addAction(tr("&Quit"));

    openAction->setIcon(QIcon::fromTheme("document-open"));
    refreshAction->setIcon(QIcon::fromTheme("view-refresh"));
    quitAction->setIcon(QIcon::fromTheme("application-exit"));

    downloadAction->setEnabled(false);

    menuBar->addMenu(fileMenu);


    emulationMenu = new QMenu(tr("&Emulation"), this);
    startAction = emulationMenu->addAction(tr("&Start"));
    stopAction = emulationMenu->addAction(tr("St&op"));
    emulationMenu->addSeparator();
    logAction = emulationMenu->addAction(tr("View Log..."));

    startAction->setIcon(QIcon::fromTheme("media-playback-start"));
    stopAction->setIcon(QIcon::fromTheme("media-playback-stop"));

    startAction->setEnabled(false);
    stopAction->setEnabled(false);

    menuBar->addMenu(emulationMenu);


    settingsMenu = new QMenu(tr("&Settings"), this);
    inputMenu = settingsMenu->addMenu(tr("&Input"));
    inputMenu->setIcon(QIcon::fromTheme("input-gaming"));
    inputGroup = new QActionGroup(this);

    QStringList inputs;
    inputs << "keyboard" << "mayflash64" << "retrolink" << "wiiu" << "x360";

    QString inputValue = SETTINGS.value("input","keyboard").toString();

    foreach (QString inputName, inputs)
    {
        QAction *input = inputMenu->addAction(inputName);
        input->setData(inputName);
        input->setCheckable(true);
        inputGroup->addAction(input);

        //Only enable input actions when CEN64 is not running
        menuEnable << input;

        if(inputValue == inputName)
            input->setChecked(true);
    }

#ifndef Q_OS_OSX //OSX does not show the configure action so the separator is unneeded
    settingsMenu->addSeparator();
#endif
    configureAction = settingsMenu->addAction(tr("&Configure..."));
    configureAction->setIcon(QIcon::fromTheme("preferences-other"));

    menuBar->addMenu(settingsMenu);


    viewMenu = new QMenu(tr("&View"), this);
    layoutMenu = viewMenu->addMenu(tr("&Layout"));
    layoutGroup = new QActionGroup(this);

    QStringList layouts;
    layouts << "None" << "Table View" << "Grid View" << "List View";

    QString layoutValue = SETTINGS.value("View/layout", "None").toString();

    foreach (QString layoutName, layouts)
    {
        QAction *layoutItem = layoutMenu->addAction(layoutName);
        layoutItem->setData(layoutName);
        layoutItem->setCheckable(true);
        layoutGroup->addAction(layoutItem);

        //Only enable layout changes when CEN64 is not running
        menuEnable << layoutItem;

        if(layoutValue == layoutName)
            layoutItem->setChecked(true);
    }

    viewMenu->addSeparator();
    statusBarAction = viewMenu->addAction(tr("&Status Bar"));

    statusBarAction->setCheckable(true);

    if (SETTINGS.value("View/statusbar", "") == "true")
        statusBarAction->setChecked(true);

    menuBar->addMenu(viewMenu);


    helpMenu = new QMenu(tr("&Help"), this);
    aboutAction = helpMenu->addAction(tr("&About"));
    aboutAction->setIcon(QIcon::fromTheme("help-about"));
    menuBar->addMenu(helpMenu);

    //Create list of actions that are enabled only when CEN64 is not running
    menuEnable << startAction
               << logAction
               << openAction
               << convertAction
               << downloadAction
               << refreshAction
               << configureAction
               << quitAction;

    //Create list of actions that are disabled when CEN64 is not running
    menuDisable << stopAction;

    connect(openAction, SIGNAL(triggered()), this, SLOT(openRom()));
    connect(convertAction, SIGNAL(triggered()), this, SLOT(openConverter()));
    connect(refreshAction, SIGNAL(triggered()), this, SLOT(addRoms()));
    connect(downloadAction, SIGNAL(triggered()), this, SLOT(openDownloader()));
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
    connect(startAction, SIGNAL(triggered()), this, SLOT(runEmulatorFromMenu()));
    connect(stopAction, SIGNAL(triggered()), this, SLOT(stopEmulator()));
    connect(logAction, SIGNAL(triggered()), this, SLOT(openLog()));
    connect(configureAction, SIGNAL(triggered()), this, SLOT(openOptions()));
    connect(statusBarAction, SIGNAL(triggered()), this, SLOT(updateStatusBarView()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(openAbout()));
    connect(inputGroup, SIGNAL(triggered(QAction*)), this, SLOT(updateInputSetting()));
    connect(layoutGroup, SIGNAL(triggered(QAction*)), this, SLOT(updateLayoutSetting()));
}


void CEN64Qt::createRomView()
{
    //Create empty view
    emptyView = new QScrollArea(this);
    emptyView->setStyleSheet("QScrollArea { border: none; }");
    emptyView->setBackgroundRole(QPalette::Base);
    emptyView->setAutoFillBackground(true);
    emptyView->setHidden(true);

    emptyLayout = new QGridLayout(emptyView);

    icon = new QLabel(emptyView);
    icon->setPixmap(QPixmap(":/images/cen64.png"));

    emptyLayout->addWidget(icon, 1, 1);
    emptyLayout->setColumnStretch(0, 1);
    emptyLayout->setColumnStretch(2, 1);
    emptyLayout->setRowStretch(0, 1);
    emptyLayout->setRowStretch(2, 1);

    emptyView->setLayout(emptyLayout);


    //Create table view
    romTree = new QTreeWidget(this);
    romTree->setWordWrap(false);
    romTree->setAllColumnsShowFocus(true);
    romTree->setRootIsDecorated(false);
    romTree->setSortingEnabled(true);
    romTree->setStyleSheet("QTreeView { border: none; } QTreeView::item { height: 25px; }");

    headerView = new QHeaderView(Qt::Horizontal, this);
    romTree->setHeader(headerView);
    romTree->setHidden(true);


    //Create grid view
    gridView = new QScrollArea(this);
    gridView->setObjectName("gridView");
    gridView->setStyleSheet("#gridView { border: none; }");
    gridView->setBackgroundRole(QPalette::Dark);
    gridView->setAlignment(Qt::AlignHCenter);
    gridView->setHidden(true);

    gridView->verticalScrollBar()->setObjectName("vScrollBar");
    gridView->horizontalScrollBar()->setObjectName("hScrollBar");

    setGridBackground();


    gridWidget = new QWidget(gridView);
    gridWidget->setObjectName("gridWidget");
    gridView->setWidget(gridWidget);

    gridLayout = new QGridLayout(gridWidget);
    gridLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    gridLayout->setRowMinimumHeight(0, 10);

    gridWidget->setLayout(gridLayout);

    gridCurrent = false;
    currentGridRom = 0;


    //Create list view
    listView = new QScrollArea(this);
    listView->setStyleSheet("QScrollArea { border: none; }");
    listView->setBackgroundRole(QPalette::Base);
    listView->setWidgetResizable(true);
    listView->setHidden(true);

    listWidget = new QWidget(listView);
    listView->setWidget(listWidget);

    listLayout = new QVBoxLayout(listWidget);
    listLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    listWidget->setLayout(listLayout);

    listCurrent = false;
    currentListRom = 0;


    QString visibleLayout = SETTINGS.value("View/layout", "None").toString();

    if (visibleLayout == "Table View")
        romTree->setHidden(false);
    else if (visibleLayout == "Grid View")
        gridView->setHidden(false);
    else if (visibleLayout == "List View")
        listView->setHidden(false);
    else
        emptyView->setHidden(false);

    cachedRoms();

    connect(romTree, SIGNAL(clicked(QModelIndex)), this, SLOT(enableButtons()));
    connect(romTree, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(runEmulatorFromRomTree()));
    connect(headerView, SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
            this, SLOT(saveSortOrder(int,Qt::SortOrder)));
}


void CEN64Qt::downloadGameInfo(QString identifier, QString searchName, QString gameID, bool force)
{
    if (identifier != "") {
        bool updated = false;

        QString gameCache = getDataLocation() + "/cache/" + identifier;
        QDir cache(gameCache);

        if (!cache.exists()) {
            cache.mkpath(gameCache);
        }

        //Get game XML info from thegamesdb.net
        QString dataFile = gameCache + "/data.xml";
        QFile file(dataFile);

        if (!file.exists() || file.size() == 0 || force) {
            QUrl url;

            //Remove [!], (U), etc. from GoodName for searching
            searchName.remove(QRegExp("\\W*(\\(|\\[).+(\\)|\\])\\W*"));

            //Few game specific hacks
            //TODO: Contact thegamesdb.net and see if these can be fixed on their end
            if (searchName == "Legend of Zelda, The - Majora's Mask")
                searchName = "Majora's Mask";
            else if (searchName == "Legend of Zelda, The - Ocarina of Time - Master Quest")
                searchName = "Master Quest";
            else if (searchName.toLower() == "f-zero x")
                gameID = "10836";

            //If user submits gameID, use that
            if (gameID != "")
                url.setUrl("http://thegamesdb.net/api/GetGame.php?id="
                           + gameID + "&platform=Nintendo 64");
            else
                url.setUrl("http://thegamesdb.net/api/GetGame.php?name="
                           + searchName + "&platform=Nintendo 64");

            QString dom = getUrlContents(url);

            QDomDocument xml;
            xml.setContent(dom);
            QDomNode node = xml.elementsByTagName("Data").at(0).firstChildElement("Game");

            int count = 0, found = 0;

            while(!node.isNull())
            {
                QDomElement element = node.firstChildElement("GameTitle").toElement();

                if (force) { //from user dialog
                    QDomElement date = node.firstChildElement("ReleaseDate").toElement();

                    QString check = "Game: " + element.text();
                    check.remove(QRegExp(QString("[^A-Za-z 0-9 \\.,\\?'""!@#\\$%\\^&\\*\\")
                                         + "(\\)-_=\\+;:<>\\/\\\\|\\}\\{\\[\\]`~]*"));
                    if (date.text() != "") check += "\nReleased on: " + date.text();
                    check += "\n\nDoes this look correct?";

                    int answer = QMessageBox::question(this, tr("Game Information Download"),
                                                       check, QMessageBox::Yes | QMessageBox::No);

                    if (answer == QMessageBox::Yes) {
                        found = count;
                        updated = true;
                        break;
                    }
                } else {
                    //We only want one game, so search for a perfect match in the GameTitle element.
                    //Otherwise this will default to 0 (the first game found)
                    if(element.text() == searchName)
                        found = count;
                }

                node = node.nextSibling();
                count++;
            }

            if (!force || updated) {
                file.open(QIODevice::WriteOnly);
                QTextStream stream(&file);

                QDomNodeList gameList = xml.elementsByTagName("Game");
                gameList.at(found).save(stream, QDomNode::EncodingFromDocument);

                file.close();
            }

            if (force && !updated) {
                QString message;

                if (count == 0)
                    message = tr("No results found.");
                else
                    message = tr("No more results found.");

                QMessageBox::information(this, tr("Game Information Download"), message);
            }
        }


        //Get front cover
        QString boxartURL = "";
        QString coverFile = gameCache + "/boxart-front.jpg";
        QFile cover(coverFile);

        if (!cover.exists() || (force && updated)) {
            file.open(QIODevice::ReadOnly);
            QString dom = file.readAll();
            file.close();

            QDomDocument xml;
            xml.setContent(dom);
            QDomNode node = xml.elementsByTagName("Game").at(0).firstChildElement("Images").firstChild();

            while(!node.isNull())
            {
                QDomElement element = node.toElement();
                if(element.tagName() == "boxart" && element.attribute("side") == "front")
                    boxartURL = element.attribute("thumb");

                node = node.nextSibling();
            }

            if (boxartURL != "") {
                QUrl url("http://thegamesdb.net/banners/" + boxartURL);

                cover.open(QIODevice::WriteOnly);
                cover.write(getUrlContents(url));
                cover.close();
            }
        }

        if (updated) {
            QMessageBox::information(this, tr("Game Information Download"), tr("Download Complete!"));
            cachedRoms();
        }
    }
}


void CEN64Qt::enableButtons()
{
    toggleMenus(true);
}


QString CEN64Qt::getDataLocation()
{
    QString dataDir;

#ifdef Q_OS_WIN
    dataDir = QCoreApplication::applicationDirPath();
#else

#if QT_VERSION >= 0x050000
    dataDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                    .replace("CEN64/CEN64-Qt","cen64-qt");
#else
    dataDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation)
                    .remove("data/").replace("CEN64/CEN64-Qt","cen64-qt");
#endif

#endif

     QDir data(dataDir);
     if (!data.exists())
         data.mkpath(dataDir);

     return dataDir;
}


QString CEN64Qt::getCurrentRomInfo(int index)
{
    if (index < 3) {
        const char *infoChar;

        switch (index) {
            case 0:  infoChar = "fileName"; break;
            case 1:  infoChar = "search";   break;
            case 2:  infoChar = "romMD5";   break;
            default: infoChar = "";         break;
        }

        QString visibleLayout = SETTINGS.value("View/layout", "None").toString();

        if (visibleLayout == "Table View")
            return romTree->currentItem()->data(index, 0).toString();
        else if (visibleLayout == "Grid View" && gridCurrent)
            return gridLayout->itemAt(currentGridRom)->widget()->property(infoChar).toString();
        else if (visibleLayout == "List View" && listCurrent)
            return listLayout->itemAt(currentListRom)->widget()->property(infoChar).toString();
    }

    return "";
}


QColor CEN64Qt::getColor(QString color, int transparency)
{
    if (transparency <= 255) {
        if (color == "Black")           return QColor(0,   0,   0,   transparency);
        else if (color == "White")      return QColor(255, 255, 255, transparency);
        else if (color == "Light Gray") return QColor(200, 200, 200, transparency);
        else if (color == "Dark Gray")  return QColor(50,  50,  59,  transparency);
        else if (color == "Green")      return QColor(0,   255, 0,   transparency);
        else if (color == "Cyan")       return QColor(30,  175, 255, transparency);
        else if (color == "Blue")       return QColor(0,   0,   255, transparency);
        else if (color == "Purple")     return QColor(128, 0,   128, transparency);
        else if (color == "Red")        return QColor(255, 0,   0,   transparency);
        else if (color == "Pink")       return QColor(246, 96,  171, transparency);
        else if (color == "Orange")     return QColor(255, 165, 0,   transparency);
        else if (color == "Yellow")     return QColor(255, 255, 0,   transparency);
        else if (color == "Brown")      return QColor(127, 70,  44,  transparency);
    }

    return QColor(0, 0, 0, 255);
}


int CEN64Qt::getGridSize(QString which)
{
    QString size = SETTINGS.value("Grid/imagesize","Medium").toString();

    if (which == "height") {
        if (SETTINGS.value("Grid/label", "true").toString() == "true") {
            if (size == "Extra Small") return 65;
            if (size == "Small")       return 90;
            if (size == "Medium")      return 145;
            if (size == "Large")       return 190;
            if (size == "Extra Large") return 250;
        } else {
            if (size == "Extra Small") return 47;
            if (size == "Small")       return 71;
            if (size == "Medium")      return 122;
            if (size == "Large")       return 172;
            if (size == "Extra Large") return 224;
        }
    } else if (which == "width") {
        if (size == "Extra Small") return 60;
        if (size == "Small")       return 90;
        if (size == "Medium")      return 160;
        if (size == "Large")       return 225;
        if (size == "Extra Large") return 300;
    } else if (which == "font") {
        if (size == "Extra Small") return 5;
        if (size == "Small")       return 7;
        if (size == "Medium")      return 10;
        if (size == "Large")       return 12;
        if (size == "Extra Large") return 13;
    }
    return 0;
}


QSize CEN64Qt::getImageSize(QString view)
{
    QString size = SETTINGS.value(view+"/imagesize","Medium").toString();

    if (view == "Table") {
        if (size == "Extra Small") return QSize(33, 24);
        if (size == "Small")       return QSize(48, 35);
        if (size == "Medium")      return QSize(69, 50);
        if (size == "Large")       return QSize(103, 75);
        if (size == "Extra Large") return QSize(138, 100);
    } else if (view == "Grid" || view == "List") {
        if (size == "Extra Small") return QSize(48, 35);
        if (size == "Small")       return QSize(69, 50);
        if (size == "Medium")      return QSize(138, 100);
        if (size == "Large")       return QSize(203, 150);
        if (size == "Extra Large") return QSize(276, 200);
    }

    return QSize();
}


QString CEN64Qt::getRomInfo(QString identifier, const Rom *rom, bool removeWarn, bool sort)
{
    QString text = "";

    if (identifier == "GoodName")
        text = rom->goodName;
    else if (identifier == "Filename")
        text = rom->baseName;
    else if (identifier == "Filename (extension)")
        text = rom->fileName;
    else if (identifier == "Zip File")
        text = rom->zipFile;
    else if (identifier == "Internal Name")
        text = rom->internalName;
    else if (identifier == "Size")
        text = rom->size;
    else if (identifier == "MD5")
        text = rom->romMD5.toLower();
    else if (identifier == "CRC1")
        text = rom->CRC1.toLower();
    else if (identifier == "CRC2")
        text = rom->CRC2.toLower();
    else if (identifier == "Players")
        text = rom->players;
    else if (identifier == "Rumble")
        text = rom->rumble;
    else if (identifier == "Save Type")
        text = rom->saveType;
    else if (identifier == "Game Title")
        text = rom->gameTitle;
    else if (identifier == "Release Date")
        text = rom->releaseDate;
    else if (identifier == "Overview")
        text = rom->overview;
    else if (identifier == "ESRB")
        text = rom->esrb;
    else if (identifier == "Genre")
        text = rom->genre;
    else if (identifier == "Publisher")
        text = rom->publisher;
    else if (identifier == "Developer")
        text = rom->developer;
    else if (identifier == "Rating")
        text = rom->rating;

    if (!removeWarn)
        return text;
    else if (text == "Unknown ROM" || text == "Requires catalog file" || text == "Not found") {
        if (sort)
            return "ZZZ"; //Sort warnings at the end
        else
            return "";
    } else
        return text;
}


QGraphicsDropShadowEffect *CEN64Qt::getShadow(bool active)
{
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect;

    if (active) {
        shadow->setBlurRadius(25.0);
        shadow->setColor(getColor(SETTINGS.value("Grid/activecolor","Cyan").toString(), 255));
        shadow->setOffset(0);
    } else {
        shadow->setBlurRadius(10.0);
        shadow->setColor(getColor(SETTINGS.value("Grid/inactivecolor","Black").toString(), 200));
        shadow->setOffset(0);
    }

    return shadow;
}


QByteArray CEN64Qt::getUrlContents(QUrl url)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", "CEN64-Qt");
    QNetworkReply *reply = manager->get(request);

    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    return reply->readAll();
}


void CEN64Qt::highlightGridWidget(QWidget *current)
{
    //Set all to inactive shadow
    QLayoutItem *gridItem;
    for (int item = 0; (gridItem = gridLayout->itemAt(item)) != NULL; item++)
    {
        gridItem->widget()->setGraphicsEffect(getShadow(false));

        if (gridItem->widget() == current)
            currentGridRom = item;
    }

    //Set current to active shadow
    current->setGraphicsEffect(getShadow(true));

    gridCurrent = true;
    toggleMenus(true);
}


void CEN64Qt::highlightListWidget(QWidget *current)
{
    //Reset all margins
    QLayoutItem *listItem;
    for (int item = 0; (listItem = listLayout->itemAt(item)) != NULL; item++)
    {
        listItem->widget()->setContentsMargins(0, 0, 20, 0);

        if (listItem->widget() == current)
            currentListRom = item;
    }

    //Give current left margin to stand out
    current->setContentsMargins(20, 0, 0, 0);

    listCurrent = true;
    toggleMenus(true);
}


void CEN64Qt::initializeRom(Rom *currentRom, bool cached)
{
    QString catalogFile = SETTINGS.value("Paths/catalog", "").toString();

    //Default text for GoodName to notify user
    currentRom->goodName = "Requires catalog file";
    currentRom->imageExists = false;

    bool getGoodName = false;
    if (QFileInfo(catalogFile).exists()) {
        romCatalog = new QSettings(catalogFile, QSettings::IniFormat, this);
        getGoodName = true;
    }

    QFile file(romDir.absoluteFilePath(currentRom->fileName));

    currentRom->romMD5 = currentRom->romMD5.toUpper();
    currentRom->baseName = QFileInfo(file).completeBaseName();
    currentRom->size = tr("%1 MB").arg((currentRom->sortSize + 1023) / 1024 / 1024);

    if (getGoodName) {
        //Join GoodName on ", ", otherwise entries with a comma won't show
        QVariant goodNameVariant = romCatalog->value(currentRom->romMD5+"/GoodName","Unknown ROM");
        currentRom->goodName = goodNameVariant.toStringList().join(", ");

        QStringList CRC = romCatalog->value(currentRom->romMD5+"/CRC","").toString().split(" ");

        if (CRC.size() == 2) {
            currentRom->CRC1 = CRC[0];
            currentRom->CRC2 = CRC[1];
        }

        QString newMD5 = romCatalog->value(currentRom->romMD5+"/RefMD5","").toString();
        if (newMD5 == "")
            newMD5 = currentRom->romMD5;

        currentRom->players = romCatalog->value(newMD5+"/Players","").toString();
        currentRom->saveType = romCatalog->value(newMD5+"/SaveType","").toString();
        currentRom->rumble = romCatalog->value(newMD5+"/Rumble","").toString();
    }

    if (!cached && SETTINGS.value("Other/downloadinfo", "").toString() == "true") {
        if (currentRom->goodName != "Unknown ROM" && currentRom->goodName != "Requires catalog file") {
            downloadGameInfo(currentRom->romMD5.toLower(), currentRom->goodName);
        } else {
            //tweak internal name by adding spaces to get better results
            QString search = currentRom->internalName;
            search.replace(QRegExp("([a-z])([A-Z])"),"\\1 \\2");
            search.replace(QRegExp("([^ \\d])(\\d)"),"\\1 \\2");
            downloadGameInfo(currentRom->romMD5.toLower(), search);
        }

    }

    if (SETTINGS.value("Other/downloadinfo", "").toString() == "true") {
        QString cacheDir = getDataLocation() + "/cache";

        QString dataFile = cacheDir + "/" + currentRom->romMD5.toLower() + "/data.xml";
        QFile file(dataFile);

        file.open(QIODevice::ReadOnly);
        QString dom = file.readAll();
        file.close();

        QDomDocument xml;
        xml.setContent(dom);
        QDomNode game = xml.elementsByTagName("Game").at(0);

        //Remove any non-standard characters
        QString regex = "[^A-Za-z 0-9 \\.,\\?'""!@#\\$%\\^&\\*\\(\\)-_=\\+;:<>\\/\\\\|\\}\\{\\[\\]`~]*";

        currentRom->gameTitle = game.firstChildElement("GameTitle").text().remove(QRegExp(regex));
        if (currentRom->gameTitle == "") currentRom->gameTitle = "Not found";

        currentRom->releaseDate = game.firstChildElement("ReleaseDate").text();

        //Fix missing 0's in date
        currentRom->releaseDate.replace(QRegExp("^(\\d)/(\\d{2})/(\\d{4})"), "0\\1/\\2/\\3");
        currentRom->releaseDate.replace(QRegExp("^(\\d{2})/(\\d)/(\\d{4})"), "\\1/0\\2/\\3");
        currentRom->releaseDate.replace(QRegExp("^(\\d)/(\\d)/(\\d{4})"), "0\\1/0\\2/\\3");

        currentRom->sortDate = currentRom->releaseDate;
        currentRom->sortDate.replace(QRegExp("(\\d{2})/(\\d{2})/(\\d{4})"), "\\3-\\1-\\2");

        currentRom->overview = game.firstChildElement("Overview").text().remove(QRegExp(regex));
        currentRom->esrb = game.firstChildElement("ESRB").text();

        int count = 0;
        QDomNode genreNode = game.firstChildElement("Genres").firstChild();
        while(!genreNode.isNull())
        {
            if (count != 0)
                currentRom->genre += "/" + genreNode.toElement().text();
            else
                currentRom->genre = genreNode.toElement().text();

            genreNode = genreNode.nextSibling();
            count++;
        }

        currentRom->publisher = game.firstChildElement("Publisher").text();
        currentRom->developer = game.firstChildElement("Developer").text();
        currentRom->rating = game.firstChildElement("Rating").text();

        QString imageFile = getDataLocation() + "/cache/"
                            + currentRom->romMD5.toLower() + "/boxart-front.jpg";
        QFile cover(imageFile);

        if (cover.exists()) {
            currentRom->image.load(imageFile);
            currentRom->imageExists = true;
        }
    }
}


void CEN64Qt::openAbout()
{
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}


void CEN64Qt::openConverter()
{
    QString v64File = QFileDialog::getOpenFileName(this, tr("Open v64 File"), romPath,
                                                   tr("V64 ROMs (*.v64 *.n64);;All Files (*)"));

    if (v64File != "") {
        QString defaultFileName = QFileInfo(v64File).completeBaseName() + ".z64";
        QString defaultFile = romDir.absoluteFilePath(defaultFileName);
        QString saveFile = QFileDialog::getSaveFileName(this, tr("Save z64 File"), defaultFile,
                                                        tr("Z64 ROMs (*.z64);;All Files (*)"));

        if (saveFile != "")
            runConverter(v64File, saveFile);
    }
}


void CEN64Qt::openDownloader()
{
    QString fileText = getCurrentRomInfo(0);
    QString defaultText = getCurrentRomInfo(1);

    downloadDialog = new QDialog(this);
    downloadDialog->setWindowTitle(tr("Search Game Information"));

    downloadLayout = new QGridLayout(downloadDialog);

    fileLabel = new QLabel(tr("<b>File:</b> ") + fileText, downloadDialog);

    gameNameLabel = new QLabel(tr("Name of Game:"), downloadDialog);
    gameIDLabel = new QLabel(tr("or Game ID:"), downloadDialog);

    defaultText.remove(QRegExp("\\W*(\\(|\\[).+(\\)|\\])\\W*"));
    gameNameField = new QLineEdit(defaultText, downloadDialog);
    gameIDField = new QLineEdit(downloadDialog);

    gameIDField->setToolTip(tr("From thegamesdb.net URL of game"));

    downloadButtonBox = new QDialogButtonBox(Qt::Horizontal, downloadDialog);
    downloadButtonBox->addButton(tr("Search"), QDialogButtonBox::AcceptRole);
    downloadButtonBox->addButton(QDialogButtonBox::Cancel);

    downloadLayout->addWidget(fileLabel, 0, 0, 1, 2);
    downloadLayout->addWidget(gameNameLabel, 1, 0);
    downloadLayout->addWidget(gameIDLabel, 2, 0);
    downloadLayout->addWidget(gameNameField, 1, 1);
    downloadLayout->addWidget(gameIDField, 2, 1);
    downloadLayout->addWidget(downloadButtonBox, 4, 0, 1, 3);
    downloadLayout->setRowStretch(3,1);
    downloadLayout->setColumnStretch(1,1);

    downloadLayout->setColumnMinimumWidth(1, 300);
    downloadLayout->setRowMinimumHeight(0, 20);
    downloadLayout->setRowMinimumHeight(3, 20);

    connect(downloadButtonBox, SIGNAL(accepted()), this, SLOT(runDownloader()));
    connect(downloadButtonBox, SIGNAL(rejected()), downloadDialog, SLOT(close()));

    downloadDialog->setLayout(downloadLayout);

    downloadDialog->exec();
}


void CEN64Qt::openLog()
{
    if (lastOutput == "") {
        QMessageBox::information(this, "No Output", QString("There is no log. Either CEN64 has not ")
                                 + "yet run or there was no output from the last run.");
    } else {
        logDialog = new QDialog(this);
        logDialog->setWindowTitle(tr("CEN64 Log"));
        logDialog->setMinimumSize(600, 400);

        logLayout = new QGridLayout(logDialog);
        logLayout->setContentsMargins(5, 10, 5, 10);

        logArea = new QTextEdit(logDialog);
        logArea->setWordWrapMode(QTextOption::NoWrap);

        QFont font;
#ifdef Q_OS_LINUX
        font.setFamily("Monospace");
        font.setPointSize(9);
#else
        font.setFamily("Courier");
        font.setPointSize(10);
#endif
        font.setFixedPitch(true);
        logArea->setFont(font);

        logArea->setPlainText(lastOutput);

        logButtonBox = new QDialogButtonBox(Qt::Horizontal, logDialog);
        logButtonBox->addButton(tr("Close"), QDialogButtonBox::AcceptRole);

        logLayout->addWidget(logArea, 0, 0);
        logLayout->addWidget(logButtonBox, 1, 0);

        connect(logButtonBox, SIGNAL(accepted()), logDialog, SLOT(close()));

        logDialog->setLayout(logLayout);

        logDialog->exec();
    }
}


void CEN64Qt::openOptions()
{
    QString tableImageBefore = SETTINGS.value("Table/imagesize", "Medium").toString();
    QString columnsBefore = SETTINGS.value("Table/columns", "Filename|Size").toString();

    SettingsDialog settingsDialog(this, 0);
    settingsDialog.exec();

    QString tableImageAfter = SETTINGS.value("Table/imagesize", "Medium").toString();
    QString columnsAfter = SETTINGS.value("Table/columns", "Filename|Size").toString();

    //Reset columns widths if user has selected different columns to display
    if (columnsBefore != columnsAfter) {
        SETTINGS.setValue("Table/width", "");
        romTree->setColumnCount(3);
        romTree->setHeaderLabels(QStringList(""));
    }

    QString romSave = SETTINGS.value("Paths/roms","").toString();
    if (romPath != romSave) {
        romPath = romSave;
        romDir = QDir(romPath);
        addRoms();
    } else {
        if (tableImageBefore != tableImageAfter)
            cachedRoms(true);
        else
            cachedRoms(false);
    }

    setGridBackground();
    toggleMenus(true);
}


void CEN64Qt::openRom()
{
    openPath = QFileDialog::getOpenFileName(this, tr("Open ROM File"), romPath,
                                                tr("N64 ROMs (*.z64 *.n64 *.zip);;All Files (*)"));
    if (openPath != "") {
        if (QFileInfo(openPath).suffix() == "zip") {
            QuaZip zipFile(openPath);
            zipFile.open(QuaZip::mdUnzip);

            QStringList zippedFiles = zipFile.getFileNameList();

            QString last;
            int count = 0;

            foreach (QString file, zippedFiles) {
                QString ext = file.right(4).toLower();

                if (ext == ".z64" || ext == ".n64") {
                    last = file;
                    count++;
                }
            }

            if (count == 0)
                QMessageBox::information(this, tr("No ROMs"), tr("No ROMs found in ZIP file."));
            else if (count == 1)
                runEmulator(last, openPath);
            else { //More than one ROM in zip file, so let user select
                openZipDialog(zippedFiles);
            }
        } else
            runEmulator(openPath);
    }
}


void CEN64Qt::openZipDialog(QStringList zippedFiles)
{
    zipDialog = new QDialog(this);
    zipDialog->setWindowTitle(tr("Select ROM"));
    zipDialog->setMinimumSize(200, 150);
    zipDialog->resize(300, 150);

    zipLayout = new QGridLayout(zipDialog);
    zipLayout->setContentsMargins(5, 10, 5, 10);

    zipList = new QListWidget(zipDialog);
    foreach (QString file, zippedFiles) {
        QString ext = file.right(4);

        if (ext == ".z64" || ext == ".n64")
            zipList->addItem(file);
    }
    zipList->setCurrentRow(0);

    zipButtonBox = new QDialogButtonBox(Qt::Horizontal, zipDialog);
    zipButtonBox->addButton(tr("Launch"), QDialogButtonBox::AcceptRole);
    zipButtonBox->addButton(QDialogButtonBox::Cancel);

    zipLayout->addWidget(zipList, 0, 0);
    zipLayout->addWidget(zipButtonBox, 1, 0);

    connect(zipList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(runEmulatorFromZip()));
    connect(zipButtonBox, SIGNAL(accepted()), this, SLOT(runEmulatorFromZip()));
    connect(zipButtonBox, SIGNAL(rejected()), zipDialog, SLOT(close()));

    zipDialog->setLayout(zipLayout);

    zipDialog->exec();
}


void CEN64Qt::readCEN64Output()
{
    QString output = cen64proc->readAllStandardOutput();
    QStringList outputList = output.split("\n");

    int lastIndex = outputList.lastIndexOf(QRegExp("^.*VI/s.*MHz$"));

    if (lastIndex >= 0)
        statusBar->showMessage(outputList[lastIndex]);

    lastOutput.append(output);
}


void CEN64Qt::resetLayouts(QStringList tableVisible, bool imageUpdated)
{
    int hidden = 4;

    saveColumnWidths();
    QStringList widths = SETTINGS.value("Table/width", "").toString().split("|");

    headerLabels.clear();
    headerLabels << "" << "" << "" << "" << tableVisible; //First 4 blank for hidden columns

    //Remove Game Cover title for aesthetics
    for (int i = 0; i < headerLabels.size(); i++)
        if (headerLabels.at(i) == "Game Cover") headerLabels.replace(i, "");

    romTree->setColumnCount(headerLabels.size());
    romTree->setHeaderLabels(headerLabels);
    headerView->setSortIndicatorShown(false);

    int height = 0, width = 0;
    if (tableVisible.contains("Game Cover")) {
        QString size = SETTINGS.value("Table/imagesize","Medium").toString();
        if (size == "Extra Small") { height = 25;  width = 40;  }
        if (size == "Small")       { height = 38;  width = 55;  }
        if (size == "Medium")      { height = 55;  width = 75;  }
        if (size == "Large")       { height = 80;  width = 110; }
        if (size == "Extra Large") { height = 110; width = 150; }

        romTree->setStyleSheet("QTreeView { border: none; } QTreeView::item { height: "
                               + QString::number(height) + "px; }");
    } else
        romTree->setStyleSheet("QTreeView { border: none; } QTreeView::item { height: 25px; }");

    QStringList sort = SETTINGS.value("Table/sort", "").toString().split("|");
    if (sort.size() == 2) {
        if (sort[1] == "descending")
            headerView->setSortIndicator(tableVisible.indexOf(sort[0]) + hidden, Qt::DescendingOrder);
        else
            headerView->setSortIndicator(tableVisible.indexOf(sort[0]) + hidden, Qt::AscendingOrder);
    }

    romTree->setColumnHidden(0, true); //Hidden filename for launching emulator
    romTree->setColumnHidden(1, true); //Hidden goodname for searching
    romTree->setColumnHidden(2, true); //Hidden md5 for cache info
    romTree->setColumnHidden(3, true); //Hidden column for zip file

    int i = hidden;
    foreach (QString current, tableVisible)
    {
        if (i == hidden) {
            int c = i;
            if (current == "Game Cover") //If first column is game cover, use next column
                c++;

            if (SETTINGS.value("Table/stretchfirstcolumn", "true") == "true") {
#if QT_VERSION >= 0x050000
                romTree->header()->setSectionResizeMode(c, QHeaderView::Stretch);
#else
                romTree->header()->setResizeMode(c, QHeaderView::Stretch);
#endif
            } else {
#if QT_VERSION >= 0x050000
                romTree->header()->setSectionResizeMode(c, QHeaderView::Interactive);
#else
                romTree->header()->setResizeMode(c, QHeaderView::Interactive);
#endif
            }
        }

        if (widths.size() == tableVisible.size()) {
            romTree->setColumnWidth(i, widths[i - hidden].toInt());
        } else {
            if (current == "Overview")
                romTree->setColumnWidth(i, 400);
            else if (current == "GoodName" || current.left(8) == "Filename" || current == "Game Title")
                romTree->setColumnWidth(i, 300);
            else if (current == "MD5")
                romTree->setColumnWidth(i, 250);
            else if (current == "Internal Name" || current == "Publisher" || current == "Developer")
                romTree->setColumnWidth(i, 200);
            else if (current == "ESRB" || current == "Genre")
                romTree->setColumnWidth(i, 150);
            else if (current == "Save Type" || current == "Release Date")
                romTree->setColumnWidth(i, 100);
            else if (current == "CRC1" || current == "CRC2")
                romTree->setColumnWidth(i, 90);
            else if (current == "Size" || current == "Rumble" || current == "Players" || current == "Rating")
                romTree->setColumnWidth(i, 75);
            else if (current == "Game Cover")
                romTree->setColumnWidth(i, width);
        }

        //Overwrite saved value if switching image sizes
        if (imageUpdated && current == "Game Cover")
            romTree->setColumnWidth(i, width);

        i++;
    }


    //Reset grid view
    QLayoutItem *gridItem;
    while ((gridItem = gridLayout->takeAt(0)) != NULL)
    {
        delete gridItem->widget();
        delete gridItem;
    }

    gridCurrent = false;


    //Reset list view
    QLayoutItem *listItem;
    while ((listItem = listLayout->takeAt(0)) != NULL)
    {
        delete listItem->widget();
        delete listItem;
    }

    listCurrent = false;
}


void CEN64Qt::runConverter(QString v64File, QString saveFile)
{
    QFile v64(v64File);
    v64.open(QIODevice::ReadOnly);

    QString v64Check(v64.read(4).toHex()), message;
    if (v64Check != "37804012") {
        if (v64Check == "80371240")
            message = "\"" + QFileInfo(v64).fileName() + "\" already in z64 format!";
        else
            message = "\"" + QFileInfo(v64).fileName() + "\" is not a valid .v64 file!";

        QMessageBox::warning(this, tr("CEN64-Qt Converter"), message);
    } else {
        v64.seek(0);

        QFile z64(saveFile);
        z64.open(QIODevice::WriteOnly);

        QByteArray data;
        QByteArray flipped;

        while (!v64.atEnd())
        {
            data = v64.read(1024);

            for (int i = 0; i < data.size(); i+=2)
            {
                //Check to see if only one byte remaining (though byte count should always be even)
                if (i + 1 == data.size())
                    flipped.append(data[i]);
                else {
                    flipped.append(data[i + 1]);
                    flipped.append(data[i]);
                }
            }

            z64.write(flipped);

            flipped.truncate(0);
        }

        z64.close();
        QMessageBox::information(this, tr("CEN64-Qt Converter"), tr("Conversion complete!"));
    }

    v64.close();
}


void CEN64Qt::runDownloader()
{
    downloadDialog->close();
    downloadGameInfo(getCurrentRomInfo(2).toLower(), gameNameField->text(), gameIDField->text(), true);

    delete downloadDialog;
}


void CEN64Qt::runEmulator(QString romFileName, QString zipFileName)
{
    QString completeRomPath;
    bool zip = false;

    if (zipFileName != "") { //If zipped file, extract and write to temp location for loading
        zip = true;

        QString zipFile = romDir.absoluteFilePath(zipFileName);
        QuaZipFile zippedFile(zipFile, romFileName);

        zippedFile.open(QIODevice::ReadOnly);
        QByteArray romData;
        romData.append(zippedFile.readAll());
        zippedFile.close();

        QString tempDir = QDir::tempPath() + "/cen64-qt";
        QDir().mkdir(tempDir);
        completeRomPath = tempDir + "/temp.z64";

        QFile tempRom(completeRomPath);
        tempRom.open(QIODevice::WriteOnly);
        tempRom.write(romData);
        tempRom.close();
    } else
        completeRomPath = romDir.absoluteFilePath(romFileName);

    QString cen64Path = SETTINGS.value("Paths/cen64", "").toString();
    QString pifPath = SETTINGS.value("Paths/pifrom", "").toString();
    QString input = inputGroup->checkedAction()->data().toString();

    QFile cen64File(cen64Path);
    QFile pifFile(pifPath);
    QFile romFile(completeRomPath);


    //Sanity checks
    if (!cen64File.exists() || QFileInfo(cen64File).isDir() || !QFileInfo(cen64File).isExecutable()) {
        QMessageBox::warning(this, "Warning", "CEN64 executable not found.");
	if (zip) cleanTemp();
        return;
    }

    if (!pifFile.exists() || QFileInfo(pifFile).isDir()) {
        QMessageBox::warning(this, "Warning", "PIFdata file not found.");
	if (zip) cleanTemp();
        return;
    }

    if (!romFile.exists() || QFileInfo(romFile).isDir()) {
        QMessageBox::warning(this, "Warning", "ROM file not found.");
	if (zip) cleanTemp();
        return;
    }

    romFile.open(QIODevice::ReadOnly);
    QByteArray romCheck = romFile.read(4);
    romFile.close();

    if (romCheck.toHex() != "80371240") {
        QMessageBox::warning(this, "Warning", "Not a valid Z64 File.");
	if (zip) cleanTemp();
        return;
    }


    QStringList args;
    args << "-controller" << input;

    if (SETTINGS.value("Saves/individualsave", "").toString() == "true") {
        QString eepromPath = SETTINGS.value("Saves/eeprom", "").toString();
        QString sramPath = SETTINGS.value("Saves/sram", "").toString();

        if (eepromPath != "")
            args << "-eeprom" << eepromPath;

        if (sramPath != "")
            args << "-sram" << sramPath;
    } else {
        QString savesPath = SETTINGS.value("Saves/directory", "").toString();
        if (savesPath != "") {
            savesDir = QDir(savesPath);

            if (savesDir.exists()) {
                romFile.open(QIODevice::ReadOnly);
                romData = new QByteArray(romFile.readAll());
                romFile.close();

                QString romMD5 = QString(QCryptographicHash::hash(*romData,
                                                                  QCryptographicHash::Md5).toHex());

                QString romBaseName = QFileInfo(romFile).completeBaseName();
                QString eepromFileName = romBaseName + "." + romMD5 + ".eeprom";
                QString sramFileName = romBaseName + "." + romMD5 + ".sram";
                QString eepromPath = savesDir.absoluteFilePath(eepromFileName);
                QString sramPath = savesDir.absoluteFilePath(sramFileName);

                args << "-eeprom" << eepromPath << "-sram" << sramPath;

                delete romData;
            }
        }
    }

    args << pifPath << completeRomPath;

    toggleMenus(false);

    cen64proc = new QProcess(this);
    connect(cen64proc, SIGNAL(finished(int)), this, SLOT(enableButtons()));
    connect(cen64proc, SIGNAL(finished(int)), this, SLOT(checkStatus(int)));

    if (zip)
        connect(cen64proc, SIGNAL(finished(int)), this, SLOT(cleanTemp()));


    if (SETTINGS.value("Other/consoleoutput", "").toString() == "true")
        cen64proc->setProcessChannelMode(QProcess::ForwardedChannels);
    else {
        connect(cen64proc, SIGNAL(readyReadStandardOutput()), this, SLOT(readCEN64Output()));
        cen64proc->setProcessChannelMode(QProcess::MergedChannels);
    }

    //clear log
    lastOutput = "";

    cen64proc->start(cen64Path, args);

    statusBar->showMessage("Emulation started", 3000);
}


void CEN64Qt::runEmulatorFromMenu()
{
    QString visibleLayout = layoutGroup->checkedAction()->data().toString();

    if (visibleLayout == "Table View")
        runEmulatorFromRomTree();
    else if (visibleLayout == "Grid View" && gridCurrent)
        runEmulatorFromWidget(gridLayout->itemAt(currentGridRom)->widget());
    else if (visibleLayout == "List View" && listCurrent)
        runEmulatorFromWidget(listLayout->itemAt(currentListRom)->widget());
}


void CEN64Qt::runEmulatorFromRomTree()
{
    QString romFileName = QVariant(romTree->currentItem()->data(0, 0)).toString();
    QString zipFileName = QVariant(romTree->currentItem()->data(3, 0)).toString();
    runEmulator(romFileName, zipFileName);
}


void CEN64Qt::runEmulatorFromWidget(QWidget *current)
{
    QString romFileName = current->property("fileName").toString();
    QString zipFileName = current->property("zipFile").toString();
    runEmulator(romFileName, zipFileName);
}


void CEN64Qt::runEmulatorFromZip()
{
    QString fileName = zipList->currentItem()->text();
    zipDialog->close();

    runEmulator(fileName, openPath);
}


void CEN64Qt::saveColumnWidths()
{
    QStringList widths;

    for (int i = 4; i < romTree->columnCount(); i++)
    {
        widths << QString::number(romTree->columnWidth(i));
    }

    if (widths.size() > 0)
        SETTINGS.setValue("Table/width", widths.join("|"));
}


void CEN64Qt::saveSortOrder(int column, Qt::SortOrder order)
{
    QString columnName = headerLabels.value(column);

    if (order == Qt::DescendingOrder)
        SETTINGS.setValue("Table/sort", columnName + "|descending");
    else
        SETTINGS.setValue("Table/sort", columnName + "|ascending");
}


void CEN64Qt::setGridBackground()
{
    gridView->setStyleSheet("#gridView { border: none; }");

    QString background = SETTINGS.value("Grid/background", "").toString();
    if (background != "") {
        QFile backgroundFile(background);

        if (backgroundFile.exists() && !QFileInfo(backgroundFile).isDir())
            gridView->setStyleSheet(QString()
                + "#gridView { "
                    + "border: none; "
                    + "background: url(" + background + "); "
                    + "background-attachment: fixed; "
                    + "background-position: top center; "
                + "} #gridWidget { background: transparent; } "
            );
    }
}


void CEN64Qt::setGridPosition()
{
    gridView->horizontalScrollBar()->setValue(positionx);
    gridView->verticalScrollBar()->setValue(positiony);
}


void CEN64Qt::setListPosition()
{
    listView->horizontalScrollBar()->setValue(positionx);
    listView->verticalScrollBar()->setValue(positiony);
}


void CEN64Qt::setTablePosition()
{
    romTree->horizontalScrollBar()->setValue(positionx);
    romTree->verticalScrollBar()->setValue(positiony);
}


void CEN64Qt::setupDatabase()
{
    database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(getDataLocation() + "/cen64-qt.sqlite");

    if (!database.open())
            QMessageBox::warning(this, "Database Not Loaded",
                                 "Could not connect to Sqlite database. Application may misbehave.");

    QSqlQuery query(QString()
                    + "CREATE TABLE IF NOT EXISTS rom_collection ("
                        + "rom_id INTEGER PRIMARY KEY ASC, "
                        + "filename TEXT NOT NULL, "
                        + "md5 TEXT NOT NULL, "
                        + "internal_name TEXT, "
                        + "zip_file TEXT, "
                        + "size INTEGER)", database);

    database.close();
}


void CEN64Qt::setupProgressDialog(int size)
{
    progress = new QProgressDialog("Loading ROMs...", "Cancel", 0, size, this);
#if QT_VERSION >= 0x050000
    progress->setWindowFlags(progress->windowFlags() & ~Qt::WindowCloseButtonHint);
    progress->setWindowFlags(progress->windowFlags() & ~Qt::WindowMinimizeButtonHint);
    progress->setWindowFlags(progress->windowFlags() & ~Qt::WindowContextHelpButtonHint);
#else
    progress->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
#endif
    progress->setCancelButton(0);
    progress->setWindowModality(Qt::WindowModal);

    progress->show();
}


void CEN64Qt::stopEmulator()
{
    cen64proc->terminate();
}


void CEN64Qt::toggleMenus(bool active)
{
    foreach (QAction *next, menuEnable)
        next->setEnabled(active);

    foreach (QAction *next, menuDisable)
        next->setEnabled(!active);

    romTree->setEnabled(active);
    gridView->setEnabled(active);
    listView->setEnabled(active);

    if (romTree->currentItem() == NULL && !gridCurrent && !listCurrent) {
        downloadAction->setEnabled(false);
        startAction->setEnabled(false);
    }

    if (SETTINGS.value("Other/downloadinfo", "").toString() == "") {
        downloadAction->setEnabled(false);
    }
}


void CEN64Qt::updateInputSetting()
{
    SETTINGS.setValue("input", inputGroup->checkedAction()->data().toString());
}


void CEN64Qt::updateLayoutSetting()
{
    QString visibleLayout = layoutGroup->checkedAction()->data().toString();
    SETTINGS.setValue("View/layout", visibleLayout);

    emptyView->setHidden(true);
    romTree->setHidden(true);
    gridView->setHidden(true);
    listView->setHidden(true);

    cachedRoms();

    if (visibleLayout == "Table View")
        romTree->setHidden(false);
    else if (visibleLayout == "Grid View")
        gridView->setHidden(false);
    else if (visibleLayout == "List View")
        listView->setHidden(false);
    else
        emptyView->setHidden(false);


    startAction->setEnabled(false);
    downloadAction->setEnabled(false);
}


void CEN64Qt::updateStatusBarView()
{
    if(statusBarAction->isChecked()) {
        SETTINGS.setValue("View/statusbar", true);
        statusBar->show();
    } else {
        SETTINGS.setValue("View/statusbar", "");
        statusBar->hide();
    }
}


bool romSorter(const Rom &firstRom, const Rom &lastRom) {
    QString sort, direction;

    QString layout = SETTINGS.value("View/layout", "None").toString();
    if (layout == "Grid View") {
        sort = SETTINGS.value("Grid/sort", "Filename").toString();
        direction = SETTINGS.value("Grid/sortdirection", "ascending").toString();
    } else if (layout == "List View") {
        sort = SETTINGS.value("List/sort", "Filename").toString();
        direction = SETTINGS.value("List/sortdirection", "ascending").toString();
    } else //just return sort by filename
        return firstRom.fileName < lastRom.fileName;


    QString sortFirst = "", sortLast = "";

    if (sort == "Size") {
        int firstSize = firstRom.sortSize;
        int lastSize = lastRom.sortSize;

        if (direction == "descending")
            return firstSize > lastSize;
        else
            return firstSize < lastSize;
    } else if (sort == "Release Date") {
        sortFirst = firstRom.sortDate;
        sortLast = lastRom.sortDate;
    } else {
        sortFirst = CEN64Qt::getRomInfo(sort, &firstRom, true, true);
        sortLast = CEN64Qt::getRomInfo(sort, &lastRom, true, true);
    }

    if (sortFirst == sortLast) { //Equal so sort on filename
        sortFirst = firstRom.fileName;
        sortLast = lastRom.fileName;
    }

    if (direction == "descending")
        return sortFirst > sortLast;
    else
        return sortFirst < sortLast;
}
