#include "AutoPlayPage.h"
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
#include <QPushButton>
#include <QDir>

// Sidebar
QList<SidebarLink> AutoPlayPage::sidebarLinks()
{
    return {
        Nav::command("Open File Manager", QStringList{"dolphin", "--help"}),
        Nav::plain("Change AutoPlay settings"),
        Nav::plain("Set defaults for all media types"),
    };
}

QList<SidebarLink> AutoPlayPage::sidebarSeeAlso()
{
    return {
        Nav::to("Control Panel Home", PageId::Home),
        Nav::to("Hardware and Sound", PageId::DevicesPrinters),
    };
}

AutoPlayPage::AutoPlaySettings AutoPlayPage::gatherSettings()
{
    AutoPlaySettings settings;

    // Read KDE configuration for AutoPlay behavior
    // Note: AutoPlay is largely deprecated in modern Linux, but we can check
    // for media handling settings in various places
    QSettings settingsFile(
        QDir::homePath() + "/.config/kdeglobals",
        QSettings::IniFormat);

    // Check for AutoPlay settings in KDE
    settings.autoPlayEnabled = !settingsFile.value("AutoRun/AutoPlayEnabled", false).toBool();
    settings.defaultAction = settingsFile.value("AutoRun/defaultAction", "Open").toString();
    if (settings.defaultAction.isEmpty()) settings.defaultAction = "Open";

    // Check for media-specific handlers
    settings.useAutoPlayForMusic = true;  // Default to enabled
    settings.useAutoPlayForPictures = true;
    settings.useAutoPlayForVideos = true;
    settings.useAutoPlayForSoftware = true;

    return settings;
}

// Page
AutoPlayPage::AutoPlayPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const AutoPlaySettings settings = gatherSettings();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("AutoPlay"));
    contentV->addSpacing(18);

    // Intro paragraph
    auto *intro = new QLabel(
        "AutoPlay lets you choose what happens when you insert a CD, DVD, "
        "or other media. You can set different actions for different types "
        "of content.");
    intro->setWordWrap(true);
    intro->setStyleSheet("color: #000000; background: transparent;");
    contentV->addWidget(intro);
    contentV->addSpacing(18);

    // Section heading
    auto addHeading = [&](const QString &text) {
        contentV->addLayout(
            Win7::sectionHeading(text, nullptr, nullptr, "#000000"));
        contentV->addSpacing(6);
    };

    // AutoPlay settings
    addHeading("AutoPlay settings");

    auto *mainGrid = new QGridLayout;
    mainGrid->setContentsMargins(14, 0, 0, 0);
    mainGrid->setHorizontalSpacing(16);
    mainGrid->setVerticalSpacing(7);

    int row = 0;

    // Use AutoPlay for all media
    mainGrid->addWidget(Win7::bodyLabel("Use AutoPlay for all media and devices", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.autoPlayEnabled);
        mainGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(mainGrid);
    contentV->addSpacing(16);

    // Default actions by content type
    addHeading("Default actions by content type");

    auto *actionsGrid = new QGridLayout;
    actionsGrid->setContentsMargins(14, 0, 0, 0);
    actionsGrid->setHorizontalSpacing(16);
    actionsGrid->setVerticalSpacing(7);

    row = 0;

    // Music files
    actionsGrid->addWidget(Win7::bodyLabel("Music files", true), row, 0, Qt::AlignLeft);
    {
        auto *combo = new QComboBox;
        combo->addItem("Open in music player");
        combo->addItem("Import music files");
        combo->addItem("Play music CD");
        combo->addItem("Ask me every time");
        combo->setCurrentIndex(0);
        actionsGrid->addWidget(combo, row++, 1, Qt::AlignLeft);
    }

    // Picture files
    actionsGrid->addWidget(Win7::bodyLabel("Picture files", true), row, 0, Qt::AlignLeft);
    {
        auto *combo = new QComboBox;
        combo->addItem("Open in picture viewer");
        combo->addItem("Import picture files");
        combo->addItem("Ask me every time");
        combo->setCurrentIndex(0);
        actionsGrid->addWidget(combo, row++, 1, Qt::AlignLeft);
    }

    // Video files
    actionsGrid->addWidget(Win7::bodyLabel("Video files", true), row, 0, Qt::AlignLeft);
    {
        auto *combo = new QComboBox;
        combo->addItem("Open in video player");
        combo->addItem("Import video files");
        combo->addItem("Play video CD/DVD");
        combo->addItem("Ask me every time");
        combo->setCurrentIndex(0);
        actionsGrid->addWidget(combo, row++, 1, Qt::AlignLeft);
    }

    // Software files
    actionsGrid->addWidget(Win7::bodyLabel("Software files", true), row, 0, Qt::AlignLeft);
    {
        auto *combo = new QComboBox;
        combo->addItem("Ask me every time");
        combo->addItem("Install or run software");
        combo->addItem("Do nothing");
        combo->setCurrentIndex(0);
        actionsGrid->addWidget(combo, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(actionsGrid);
    contentV->addSpacing(16);

    // More settings
    addHeading("More settings");

    auto *moreGrid = new QGridLayout;
    moreGrid->setContentsMargins(14, 0, 0, 0);
    moreGrid->setHorizontalSpacing(16);
    moreGrid->setVerticalSpacing(7);

    row = 0;

    // Check for music when inserted
    moreGrid->addWidget(Win7::bodyLabel("Check for music when inserted", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.useAutoPlayForMusic);
        moreGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    // Check for pictures when inserted
    moreGrid->addWidget(Win7::bodyLabel("Check for pictures when inserted", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.useAutoPlayForPictures);
        moreGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    // Check for videos when inserted
    moreGrid->addWidget(Win7::bodyLabel("Check for videos when inserted", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.useAutoPlayForVideos);
        moreGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(moreGrid);
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
