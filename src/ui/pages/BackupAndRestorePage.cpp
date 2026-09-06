#include "BackupAndRestorePage.h"
#include "IconHelper.h"
#include "Win7Ui.h"

#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QProcess>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

// Sidebar
QList<SidebarLink> BackupAndRestorePage::sidebarLinks()
{
    return {
        Nav::command("Back up your computer", QStringList{"rsync", "--help"}),
        Nav::command("Restore files from backup", QStringList{"rsync", "--help"}),
        Nav::plain("Change settings"),
        Nav::plain("Create a system repair disc"),
    };
}

QList<SidebarLink> BackupAndRestorePage::sidebarSeeAlso()
{
    return {
        Nav::to("Action Center", PageId::ActionCenter),
        Nav::to("System", PageId::System),
    };
}

BackupAndRestorePage::BackupInfo BackupAndRestorePage::gatherInfo()
{
    BackupInfo info;

    // Check for common backup locations
    QStringList backupDirs = {
        QDir::homePath() + "/.backup",
        QDir::homePath() + "/backups",
        "/backup",
        "/var/backup"
    };

    for (const QString &dir : backupDirs) {
        QFileInfo fi(dir);
        if (fi.exists() && fi.isDir()) {
            info.hasBackup = true;
            info.backupLocation = dir;
            info.lastBackup = fi.lastModified().toString("yyyy-MM-dd HH:mm:ss");
            break;
        }
    }

    // Try to get last backup from backup logs
    QStringList logFiles = {
        QDir::homePath() + "/.backup/backup.log",
        QDir::homePath() + "/backups/backup.log",
        "/var/log/backup.log"
    };

    for (const QString &log : logFiles) {
        QFile f(log);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.contains("backup", Qt::CaseInsensitive)) {
                    info.lastBackup = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                    break;
                }
            }
            f.close();
            break;
        }
    }

    return info;
}

// Page
BackupAndRestorePage::BackupAndRestorePage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const BackupInfo info = gatherInfo();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("Back up your computer settings and files"));
    contentV->addSpacing(18);

    // Section heading
    auto addHeading = [&](const QString &text) {
        contentV->addLayout(
            Win7::sectionHeading(text, nullptr, nullptr, "#000000"));
        contentV->addSpacing(6);
    };

    // Backup status
    addHeading("Backup status");

    auto *statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(14, 0, 0, 0);
    statusRow->setSpacing(12);

    auto *icon = new QLabel;
    icon->setFixedSize(48, 48);
    icon->setPixmap(themeIcon({"system-save", "document-save"}).pixmap(48, 48));
    icon->setStyleSheet("background: transparent;");
    statusRow->addWidget(icon, 0, Qt::AlignTop);

    auto *textCol = new QVBoxLayout;
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    if (info.hasBackup) {
        textCol->addWidget(Win7::label("Back up is turned on"));
        textCol->addWidget(Win7::bodyLabel("Your computer is being backed up to:"));
        textCol->addWidget(Win7::bodyLabel(info.backupLocation, true));
        textCol->addWidget(Win7::bodyLabel(QString("Last backup: %1").arg(info.lastBackup), false));
    } else {
        textCol->addWidget(Win7::label("Back up is turned off"));
        textCol->addWidget(Win7::bodyLabel("You have no active backup."));
        textCol->addWidget(Win7::bodyLabel("Set up back up", true));
    }

    statusRow->addLayout(textCol, 1);
    contentV->addLayout(statusRow);
    contentV->addSpacing(16);

    // Restore status
    addHeading("Restore");

    auto *restoreRow = new QHBoxLayout;
    restoreRow->setContentsMargins(14, 0, 0, 0);
    restoreRow->setSpacing(12);

    auto *restoreIcon = new QLabel;
    restoreIcon->setFixedSize(48, 48);
    restoreIcon->setPixmap(themeIcon({"edit-undo", "document-revert"}).pixmap(48, 48));
    restoreIcon->setStyleSheet("background: transparent;");
    restoreRow->addWidget(restoreIcon, 0, Qt::AlignTop);

    auto *restoreTextCol = new QVBoxLayout;
    restoreTextCol->setContentsMargins(0, 0, 0, 0);
    restoreTextCol->setSpacing(2);

    restoreTextCol->addWidget(Win7::label("Restore files from a backup"));
    restoreTextCol->addWidget(Win7::bodyLabel("Restore your files from a previous backup."));
    restoreTextCol->addWidget(Win7::bodyLabel("Restore my files", true));

    restoreRow->addLayout(restoreTextCol, 1);
    contentV->addLayout(restoreRow);
    contentV->addSpacing(16);

    // More tasks
    addHeading("More tasks");

    auto *taskGrid = new QGridLayout;
    taskGrid->setContentsMargins(14, 0, 0, 0);
    taskGrid->setHorizontalSpacing(16);
    taskGrid->setVerticalSpacing(7);

    int row = 0;
    taskGrid->addWidget(Win7::bodyLabel("Change settings", true), row, 0, Qt::AlignLeft);
    taskGrid->addWidget(Win7::bodyLabel("Create a system repair disc", true), ++row, 0, Qt::AlignLeft);
    taskGrid->addWidget(Win7::bodyLabel("View backups", true), ++row, 0, Qt::AlignLeft);
    taskGrid->addWidget(Win7::bodyLabel("Manage space", true), ++row, 0, Qt::AlignLeft);

    contentV->addLayout(taskGrid);
    contentV->addStretch(1);
}
