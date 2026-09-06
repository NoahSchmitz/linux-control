#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "Default Programs" detail page, a Linux-flavoured take on the Windows 7
// Default Programs page. This page allows users to set default applications
// for different file types and protocols.
class DefaultProgramsPage : public QWidget {
    Q_OBJECT

public:
    explicit DefaultProgramsPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct DefaultPrograms {
        QString browser;
        QString emailClient;
        QString mediaPlayer;
        QString photoViewer;
    };

    static DefaultPrograms gatherDefaults();
};
