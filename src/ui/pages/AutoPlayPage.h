#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "AutoPlay" detail page, a Linux-flavoured take on the Windows 9x/2000
// AutoPlay settings page. This page provides access to auto-play behavior
// configuration for different media types.
class AutoPlayPage : public QWidget {
    Q_OBJECT

public:
    explicit AutoPlayPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct AutoPlaySettings {
        bool autoPlayEnabled = true;
        QString defaultAction;      // "Open", "Play", "Backup", "Import"
        bool useAutoPlayForMusic = true;
        bool useAutoPlayForPictures = true;
        bool useAutoPlayForVideos = true;
        bool useAutoPlayForSoftware = true;
    };

    static AutoPlaySettings gatherSettings();
};
