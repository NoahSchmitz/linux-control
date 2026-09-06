#include "BitLockerPage.h"
#include "IconHelper.h"
#include "Win7Ui.h"

#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Sidebar
QList<SidebarLink> BitLockerPage::sidebarLinks()
{
    return {
        Nav::command("Turn on BitLocker", QStringList{"cryptsetup", "--help"}),
        Nav::plain("Manage BitLocker"),
        Nav::plain("Back up recovery key"),
    };
}

QList<SidebarLink> BitLockerPage::sidebarSeeAlso()
{
    return {
        Nav::to("Action Center", PageId::ActionCenter),
        Nav::to("System", PageId::System),
    };
}

QList<BitLockerPage::DiskInfo> BitLockerPage::gatherDiskInfo()
{
    QList<DiskInfo> disks;

    // Get block device info using lsblk
    QProcess process;
    process.start("lsblk", {"-J", "-o", "NAME,TYPE,MOUNTPOINT,FSTYPE,SIZE"});
    process.waitForFinished();

    if (process.exitCode() == 0) {
        QJsonDocument doc = QJsonDocument::fromJson(process.readAllStandardOutput());
        QJsonArray blocks = doc.object()["blockdevices"].toArray();

        for (const QJsonValue &block : blocks) {
            QString name = block["name"].toString();
            QString type = block["type"].toString();
            QString mountPoint = block["mountpoint"].toString();
            QString fsType = block["fstype"].toString();
            QString size = block["size"].toString();

            // Skip loop devices and partitions
            if (type == "loop" || name.contains("nvme") || name.contains("sd")) {
                DiskInfo info;
                info.diskName = name;
                info.size = size;

                // Check if encrypted (crypt_LUKS)
                if (fsType == "crypto_LUKS") {
                    info.encrypted = true;
                    info.encryptionType = "LUKS";
                    info.mountPoint = mountPoint;
                } else if (type == "crypt") {
                    info.encrypted = true;
                    info.encryptionType = "LUKS (open)";
                    info.mountPoint = mountPoint;
                }

                disks.append(info);
            }
        }
    }

    return disks;
}

// Page
BitLockerPage::BitLockerPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const QList<DiskInfo> disks = gatherDiskInfo();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("BitLocker Drive Encryption"));
    contentV->addSpacing(18);

    // Section heading
    auto addHeading = [&](const QString &text) {
        contentV->addLayout(
            Win7::sectionHeading(text, nullptr, nullptr, "#000000"));
        contentV->addSpacing(6);
    };

    // Drive status
    addHeading("Drive status");

    for (const DiskInfo &disk : disks) {
        auto *diskRow = new QHBoxLayout;
        diskRow->setContentsMargins(14, 0, 0, 0);
        diskRow->setSpacing(12);

        auto *icon = new QLabel;
        icon->setFixedSize(48, 48);
        const char *iconType = disk.encrypted ? "drive-harddisk-encrypted" : "drive-harddisk";
        icon->setPixmap(themeIcon({iconType, "drive-harddisk"}).pixmap(48, 48));
        icon->setStyleSheet("background: transparent;");
        diskRow->addWidget(icon, 0, Qt::AlignTop);

        auto *textCol = new QVBoxLayout;
        textCol->setContentsMargins(0, 0, 0, 0);
        textCol->setSpacing(2);

        textCol->addWidget(Win7::label(disk.diskName));
        textCol->addWidget(Win7::bodyLabel(QString("Size: %1").arg(disk.size)));

        if (disk.encrypted) {
            textCol->addWidget(Win7::label("Protection: On"));
            textCol->addWidget(Win7::bodyLabel(QString("Encryption: %1").arg(disk.encryptionType)));
            textCol->addWidget(Win7::bodyLabel(QString("Mount point: %1").arg(disk.mountPoint)));
            textCol->addWidget(Win7::bodyLabel("Manage BitLocker", true));
        } else {
            textCol->addWidget(Win7::label("Protection: Off"));
            textCol->addWidget(Win7::bodyLabel("Turn on BitLocker", true));
        }

        diskRow->addLayout(textCol, 1);
        contentV->addLayout(diskRow);
        contentV->addSpacing(8);
    }

    contentV->addSpacing(16);

    // More tasks
    addHeading("More tasks");

    auto *taskGrid = new QGridLayout;
    taskGrid->setContentsMargins(14, 0, 0, 0);
    taskGrid->setHorizontalSpacing(16);
    taskGrid->setVerticalSpacing(7);

    int row = 0;
    taskGrid->addWidget(Win7::bodyLabel("Change settings", true), row, 0, Qt::AlignLeft);
    taskGrid->addWidget(Win7::bodyLabel("Create a recovery key", true), ++row, 0, Qt::AlignLeft);
    taskGrid->addWidget(Win7::bodyLabel("Back up recovery key", true), ++row, 0, Qt::AlignLeft);

    contentV->addLayout(taskGrid);
    contentV->addStretch(1);
}
