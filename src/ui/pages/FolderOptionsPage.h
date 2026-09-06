#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "Folder Options" detail page, a Linux-flavoured take on the Windows 7
// "Folder Options" screen. This page provides access to KDE file manager
// (Dolphin) settings.
class FolderOptionsPage : public QWidget {
    Q_OBJECT

public:
    explicit FolderOptionsPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct FolderSettings {
        QString viewMode;           // "Detailed", "Icon", "Compact"
        bool showHiddenFiles = false;
        bool showPreview = false;
        bool confirmDelete = true;
        bool openInSeparateWindow = false;
    };

    static FolderSettings gatherSettings();
};
