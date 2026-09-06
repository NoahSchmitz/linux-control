#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "HomeGroup" detail page, a Linux-flavoured take on the Windows 7
// HomeGroup page. This page allows users to share files and printers
// with other computers on the home network.
class HomeGroupPage : public QWidget {
    Q_OBJECT

public:
    explicit HomeGroupPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct HomeGroupSettings {
        bool homeGroupEnabled = false;
        QString homeGroupName;
        bool sharePictures = true;
        bool shareMusic = true;
        bool shareVideos = true;
        bool sharePrinters = true;
    };

    static HomeGroupSettings gatherSettings();
};
