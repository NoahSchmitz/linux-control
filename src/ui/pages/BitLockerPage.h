#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "BitLocker Drive Encryption" detail page, a Linux-flavoured take on the
// Windows 7 "Turn on BitLocker" screen. Since Linux uses LUKS instead of BitLocker,
// this page shows LUKS encryption status and provides access to encryption tools.
class BitLockerPage : public QWidget {
    Q_OBJECT

public:
    explicit BitLockerPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct DiskInfo {
        QString diskName;
        bool encrypted = false;
        QString encryptionType;
        QString mountPoint;
        QString size;
    };

    static QList<DiskInfo> gatherDiskInfo();
};
