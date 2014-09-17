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
#include <QHeaderView>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtXml/QDomDocument>

#include "treewidgetitem.h"

#include <QDebug>


class CEN64Qt : public QMainWindow
{
    Q_OBJECT

public:
    CEN64Qt(QWidget *parent = 0);

protected:
    void closeEvent(QCloseEvent *event);

private:
    void addToRomTree(QString fileName, QString romMD5, QString internalName, QStringList visible, bool cached);
    void cacheGameInfo(QString goodName, QString searchName = "", QString gameID = "", bool force = false);
    void cachedRoms();
    void createMenu();
    void createRomView();
    void openOptions(int activeTab);
    void resetRomTreeLayout(QStringList visible);
    void runConverter(QString v64File, QString saveFile);
    void runEmulator(QString completeRomPath);
    void saveColumnWidths();
    void toggleMenus(bool active);

    QByteArray byteswap(QByteArray romData);
    QByteArray getUrlContents(QUrl url);
    QString getCacheLocation();

    QDir romDir;
    QDir savesDir;
    QString romPath;
    QStringList headerLabels;

    QAction *aboutAction;
    QAction *columnsAction;
    QAction *convertAction;
    QAction *downloadAction;
    QAction *openAction;
    QAction *pathsAction;
    QAction *quitAction;
    QAction *refreshAction;
    QAction *startAction;
    QAction *statusBarAction;
    QAction *stopAction;
    QActionGroup *inputGroup;
    QByteArray *romData;
    QDialog *downloadDialog;
    QDialogButtonBox *downloadButtonBox;
    QGridLayout *downloadLayout;
    QHeaderView *headerView;
    QLabel *searchLabel;
    QLabel *gameNameLabel;
    QLabel *gameIDLabel;
    QLineEdit *gameNameField;
    QLineEdit *gameIDField;
    QList<QAction*> menuEnable;
    QList<QAction*> menuDisable;
    QMenu *emulationMenu;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QMenu *inputMenu;
    QMenu *settingsMenu;
    QMenu *viewMenu;
    QMenuBar *menuBar;
    QProcess *cen64proc;
    QSettings *romCatalog;
    QStatusBar *statusBar;
    QTreeWidget *romTree;
    TreeWidgetItem *fileItem;
    QVBoxLayout *layout;
    QWidget *widget;

private slots:
    void addRoms();
    void checkStatus(int status);
    void enableButtons();
    void openAbout();
    void openColumns();
    void openConverter();
    void openDownloader();
    void openPaths();
    void openRom();
    void readCEN64Output();
    void runDownloader();
    void runEmulatorFromRomTree();
    void saveSortOrder(int column, Qt::SortOrder order);
    void stopEmulator();
    void updateInputSetting();
    void updateStatusBarView();

};

#endif // CEN64QT_H
