#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "Taskbar and Start Menu" detail page, a Linux-flavoured take on the
// Windows 7 "Taskbar and Start Menu Properties" screen. This page provides
// access to KDE desktop panel and menu settings.
class TaskbarAndStartMenuPage : public QWidget {
    Q_OBJECT

public:
    explicit TaskbarAndStartMenuPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct PanelSettings {
        bool autoHide = false;
        bool locked = false;
        QString position;           // "Top", "Bottom", "Left", "Right"
        bool showTasks = true;
        bool showClock = true;
        bool showSystemTray = true;
    };

    static PanelSettings gatherSettings();
};
