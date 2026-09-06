#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "Internet Options" detail page, a Linux-flavoured take on the Windows 9x/2000
// Internet Options page. This page provides access to browser and network
// settings configuration.
class InternetOptionsPage : public QWidget {
    Q_OBJECT

public:
    explicit InternetOptionsPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct InternetSettings {
        QString browserType;        // "Firefox", "Chrome", etc.
        QString homePage;           // Current home page URL
        bool enableCache = true;
        bool enableCookies = true;
        bool enableJavaScript = true;
    };

    static InternetSettings gatherSettings();
};
