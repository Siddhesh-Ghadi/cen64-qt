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

#ifndef CEN64QT_H
#define CEN64QT_H

#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QLineEdit>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTextStream>
#include <QTime>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtXml/QDomDocument>

#include "clickablewidget.h"
#include "treewidgetitem.h"


typedef struct {
    QString fileName;
    QString romMD5;
    QString internalName;

    QString baseName;
    QString size;
    int sortSize;

    QString goodName;
    QString CRC1;
    QString CRC2;
    QString players;
    QString saveType;
    QString rumble;

    QString gameTitle;
    QString releaseDate;
    QString sortDate;
    QString overview;
    QString esrb;
    QString genre;
    QString publisher;
    QString developer;
    QString rating;

    QPixmap image;

    int count;
    bool imageExists;
} Rom;

bool romSorter(const Rom &firstRom, const Rom &lastRom);


class CEN64Qt : public QMainWindow
{
    Q_OBJECT

public:
    CEN64Qt(QWidget *parent = 0);

protected:
    void closeEvent(QCloseEvent *event);

private:
    void addToGridView(Rom *currentRom, int count);
    void addToListView(Rom *currentRom);
    void addToTableView(Rom *currentRom);
    void cacheGameInfo(QString identifier, QString searchName, QString gameID = "", bool force = false);
    void cachedRoms(bool imageUpdated = false);
    void createMenu();
    void createRomView();
    void initializeRom(Rom *currentRom, bool cached);
    void resetLayouts(QStringList tableVisible, bool imageUpdated = false);
    void runConverter(QString v64File, QString saveFile);
    void runEmulator(QString completeRomPath);
    void saveColumnWidths();
    void setGridBackground();
    void setupProgressDialog(QStringList item);
    void toggleMenus(bool active);

    QByteArray byteswap(QByteArray romData);
    QByteArray getUrlContents(QUrl url);
    QColor getColor(QString color, int transparency = 255);
    QGraphicsDropShadowEffect *getShadow(bool active);
    QSize getImageSize(QString view);
    QString getCacheLocation();
    QString getCurrentRomInfo(int index);

    int currentGridRom;
    int currentListRom;
    int getGridSize(QString which);
    bool gridCurrent;
    bool listCurrent;

    QDir romDir;
    QDir savesDir;
    QString romPath;
    QStringList headerLabels;

    QAction *aboutAction;
    QAction *configureAction;
    QAction *convertAction;
    QAction *downloadAction;
    QAction *logAction;
    QAction *openAction;
    QAction *quitAction;
    QAction *refreshAction;
    QAction *startAction;
    QAction *statusBarAction;
    QAction *stopAction;
    QActionGroup *inputGroup;
    QActionGroup *layoutGroup;
    QByteArray *romData;
    QDialog *downloadDialog;
    QDialog *logDialog;
    QDialogButtonBox *downloadButtonBox;
    QDialogButtonBox *logButtonBox;
    QGridLayout *downloadLayout;
    QGridLayout *emptyLayout;
    QGridLayout *gridLayout;
    QGridLayout *logLayout;
    QHeaderView *headerView;
    QLabel *fileLabel;
    QLabel *gameNameLabel;
    QLabel *gameIDLabel;
    QLabel *icon;
    QLineEdit *gameNameField;
    QLineEdit *gameIDField;
    QList<QAction*> menuEnable;
    QList<QAction*> menuDisable;
    QMenu *emulationMenu;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QMenu *inputMenu;
    QMenu *layoutMenu;
    QMenu *settingsMenu;
    QMenu *viewMenu;
    QMenuBar *menuBar;
    QProcess *cen64proc;
    QProgressDialog *progress;
    QScrollArea *emptyView;
    QScrollArea *listView;
    QScrollArea *gridView;
    QSettings *romCatalog;
    QStatusBar *statusBar;
    QString lastOutput;
    QTextEdit *logArea;
    QTreeWidget *romTree;
    TreeWidgetItem *fileItem;
    QVBoxLayout *layout;
    QVBoxLayout *listLayout;
    QWidget *listWidget;
    QWidget *gridContainer;
    QWidget *gridWidget;
    QWidget *widget;

private slots:
    void addRoms();
    void checkStatus(int status);
    void enableButtons();
    void highlightGridWidget(QWidget *current);
    void highlightListWidget(QWidget *current);
    void openAbout();
    void openConverter();
    void openDownloader();
    void openLog();
    void openOptions();
    void openRom();
    void readCEN64Output();
    void runDownloader();
    void runEmulatorFromMenu();
    void runEmulatorFromRomTree();
    void runEmulatorFromWidget(QWidget *current);
    void saveSortOrder(int column, Qt::SortOrder order);
    void stopEmulator();
    void updateInputSetting();
    void updateLayoutSetting();
    void updateStatusBarView();

};

#endif // CEN64QT_H
