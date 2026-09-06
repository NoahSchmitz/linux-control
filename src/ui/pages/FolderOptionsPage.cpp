#include "FolderOptionsPage.h"
#include "IconHelper.h"
#include "Win7Ui.h"

#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QSettings>
#include <QComboBox>
#include <QCheckBox>

// Sidebar
QList<SidebarLink> FolderOptionsPage::sidebarLinks()
{
    return {
        Nav::command("Open File Explorer", QStringList{"dolphin", "--help"}),
        Nav::plain("Customize folders"),
        Nav::plain("Reset folder options"),
    };
}

QList<SidebarLink> FolderOptionsPage::sidebarSeeAlso()
{
    return {
        Nav::to("Control Panel Home", PageId::Home),
        Nav::to("Appearance and Personalization", PageId::Personalization),
    };
}

FolderOptionsPage::FolderSettings FolderOptionsPage::gatherSettings()
{
    FolderSettings settings;

    // Read KDE Dolphin configuration
    QSettings settingsFile(
        QDir::homePath() + "/.config/kdeglobals",
        QSettings::IniFormat);

    // Get view mode
    settings.viewMode = settingsFile.value("General/ViewMode", "Detailed").toString();
    if (settings.viewMode.isEmpty()) settings.viewMode = "Detailed";

    // Check for hidden files
    settings.showHiddenFiles = settingsFile.value("General/ShowHiddenFiles", false).toBool();

    // Check for preview
    settings.showPreview = settingsFile.value("PreviewSettings/ShowPreview", true).toBool();

    // Check for confirm delete
    settings.confirmDelete = !settingsFile.value("Confirmations/ConfirmDelete", false).toBool();

    // Check for separate window
    settings.openInSeparateWindow = settingsFile.value("General/OpenFolderInSeparateWindow", false).toBool();

    return settings;
}

// Page
FolderOptionsPage::FolderOptionsPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const FolderSettings settings = gatherSettings();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("Folder Options"));
    contentV->addSpacing(18);

    // Section heading
    auto addHeading = [&](const QString &text) {
        contentV->addLayout(
            Win7::sectionHeading(text, nullptr, nullptr, "#000000"));
        contentV->addSpacing(6);
    };

    // General settings
    addHeading("General");

    auto *generalGrid = new QGridLayout;
    generalGrid->setContentsMargins(14, 0, 0, 0);
    generalGrid->setHorizontalSpacing(16);
    generalGrid->setVerticalSpacing(7);

    int row = 0;
    generalGrid->addWidget(Win7::bodyLabel("Browse folders", true), row, 0, Qt::AlignLeft);
    {
        auto *combo = new QComboBox;
        combo->addItem("Open each folder in the same window");
        combo->addItem("Open each folder in a separate window");
        combo->setCurrentIndex(settings.openInSeparateWindow ? 1 : 0);
        generalGrid->addWidget(combo, row++, 1, Qt::AlignLeft);
    }

    generalGrid->addWidget(Win7::bodyLabel("Show hidden files", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.showHiddenFiles);
        generalGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(generalGrid);
    contentV->addSpacing(16);

    // View settings
    addHeading("View");

    auto *viewGrid = new QGridLayout;
    viewGrid->setContentsMargins(14, 0, 0, 0);
    viewGrid->setHorizontalSpacing(16);
    viewGrid->setVerticalSpacing(7);

    row = 0;
    viewGrid->addWidget(Win7::bodyLabel("Show preview handlers", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.showPreview);
        viewGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    viewGrid->addWidget(Win7::bodyLabel("Confirm file delete", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.confirmDelete);
        viewGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(viewGrid);
    contentV->addSpacing(16);

    // Folder types
    addHeading("Folder types");

    auto *typeGrid = new QGridLayout;
    typeGrid->setContentsMargins(14, 0, 0, 0);
    typeGrid->setHorizontalSpacing(16);
    typeGrid->setVerticalSpacing(7);

    row = 0;
    typeGrid->addWidget(Win7::bodyLabel("Folder type", true), row, 0, Qt::AlignLeft);
    {
        auto *combo = new QComboBox;
        combo->addItem("General items");
        combo->addItem("Documents");
        combo->addItem("Pictures");
        combo->addItem("Music");
        combo->addItem("Videos");
        typeGrid->addWidget(combo, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(typeGrid);
    contentV->addSpacing(16);

    // Reset and apply
    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(14, 0, 0, 0);
    buttonRow->addStretch(1);

    auto *applyBtn = new QPushButton("Apply");
    applyBtn->setCursor(Qt::PointingHandCursor);
    applyBtn->setIcon(themeIcon({"dialog-ok", "dialog-apply"}));
    buttonRow->addWidget(applyBtn);

    auto *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setIcon(themeIcon({"dialog-cancel", "dialog-close"}));
    buttonRow->addWidget(cancelBtn);

    contentV->addLayout(buttonRow);
    contentV->addStretch(1);
}
