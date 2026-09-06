#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "Backup and Restore" detail page, a Linux-flavoured take on the Windows 7
// "Back up your computer settings and files" screen.
//
// Since Linux doesn't have a native backup application like Windows, this page
// provides a simple interface to rsync-based backup operations. It shows the
// last backup status and provides quick access to backup configuration.
class BackupAndRestorePage : public QWidget {
    Q_OBJECT

public:
    explicit BackupAndRestorePage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct BackupInfo {
        bool hasBackup = false;
        QString lastBackup;
        QString backupLocation;
        int backupSize = 0;  // in KB
    };

    static BackupInfo gatherInfo();
};
