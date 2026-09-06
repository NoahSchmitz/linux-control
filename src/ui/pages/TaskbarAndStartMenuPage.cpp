#include "TaskbarAndStartMenuPage.h"
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

// Sidebar
QList<SidebarLink> TaskbarAndStartMenuPage::sidebarLinks()
{
    return {
        Nav::command("Taskbar settings", QStringList{"plasmashell", "--replace"}),
        Nav::plain("Start menu settings"),
        Nav::plain("Notification area settings"),
    };
}

QList<SidebarLink> TaskbarAndStartMenuPage::sidebarSeeAlso()
{
    return {
        Nav::to("Control Panel Home", PageId::Home),
        Nav::to("Appearance and Personalization", PageId::Personalization),
    };
}

TaskbarAndStartMenuPage::PanelSettings TaskbarAndStartMenuPage::gatherSettings()
{
    PanelSettings settings;

    // Read KDE panel configuration
    QSettings settingsFile(
        QDir::homePath() + "/.config/plasma-org.kde.plasma.desktop-appletsrc",
        QSettings::IniFormat);

    // Check for auto-hide
    settings.autoHide = settingsFile.value("Panel/AutoHide", false).toBool();

    // Check for locked state
    settings.locked = settingsFile.value("Panel/Locked", false).toBool();

    // Get position
    QString position = settingsFile.value("Panel/Location", "bottom").toString();
    if (position == "top") settings.position = "Top";
    else if (position == "bottom") settings.position = "Bottom";
    else if (position == "left") settings.position = "Left";
    else if (position == "right") settings.position = "Right";
    else settings.position = "Bottom";

    // Get panel items
    QString panelItems = settingsFile.value("Panel/PanelItems", "").toString();
    settings.showTasks = !panelItems.contains("org.kde.plasma.taskmanager");
    settings.showClock = panelItems.contains("org.kde.plasma.digitalclock");
    settings.showSystemTray = panelItems.contains("org.kde.plasma.systemtray");

    return settings;
}

// Page
TaskbarAndStartMenuPage::TaskbarAndStartMenuPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const PanelSettings settings = gatherSettings();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("Taskbar and Start Menu Properties"));
    contentV->addSpacing(18);

    // Section heading
    auto addHeading = [&](const QString &text) {
        contentV->addLayout(
            Win7::sectionHeading(text, nullptr, nullptr, "#000000"));
        contentV->addSpacing(6);
    };

    // Taskbar appearance
    addHeading("Taskbar");

    auto *appearanceGrid = new QGridLayout;
    appearanceGrid->setContentsMargins(14, 0, 0, 0);
    appearanceGrid->setHorizontalSpacing(16);
    appearanceGrid->setVerticalSpacing(7);

    int row = 0;
    appearanceGrid->addWidget(Win7::bodyLabel("Taskbar location", true), row, 0, Qt::AlignLeft);
    {
        auto *combo = new QComboBox;
        combo->addItem("Top");
        combo->addItem("Bottom");
        combo->addItem("Left");
        combo->addItem("Right");
        combo->setCurrentText(settings.position);
        appearanceGrid->addWidget(combo, row++, 1, Qt::AlignLeft);
    }

    appearanceGrid->addWidget(Win7::bodyLabel("Auto-hide taskbar", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.autoHide);
        appearanceGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    appearanceGrid->addWidget(Win7::bodyLabel("Lock the taskbar", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.locked);
        appearanceGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(appearanceGrid);
    contentV->addSpacing(16);

    // Start menu
    addHeading("Start menu");

    auto *menuGrid = new QGridLayout;
    menuGrid->setContentsMargins(14, 0, 0, 0);
    menuGrid->setHorizontalSpacing(16);
    menuGrid->setVerticalSpacing(7);

    row = 0;
    menuGrid->addWidget(Win7::bodyLabel("Show recent items", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(true);
        menuGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    menuGrid->addWidget(Win7::bodyLabel("Show common locations", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(true);
        menuGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(menuGrid);
    contentV->addSpacing(16);

    // Notification area
    addHeading("Notification area");

    auto *notifyGrid = new QGridLayout;
    notifyGrid->setContentsMargins(14, 0, 0, 0);
    notifyGrid->setHorizontalSpacing(16);
    notifyGrid->setVerticalSpacing(7);

    row = 0;
    notifyGrid->addWidget(Win7::bodyLabel("Show clock", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.showClock);
        notifyGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    notifyGrid->addWidget(Win7::bodyLabel("Show system tray", true), row, 0, Qt::AlignLeft);
    {
        auto *checkbox = new QCheckBox;
        checkbox->setChecked(settings.showSystemTray);
        notifyGrid->addWidget(checkbox, row++, 1, Qt::AlignLeft);
    }

    notifyGrid->addWidget(Win7::bodyLabel("Show taskbar button combining", true), ++row, 0, Qt::AlignLeft);
    {
        auto *combo = new QComboBox;
        combo->addItem("Always combine, hide labels");
        combo->addItem("Combine when taskbar is full");
        combo->addItem("Never combine");
        combo->setCurrentIndex(1);
        notifyGrid->addWidget(combo, row++, 1, Qt::AlignLeft);
    }

    contentV->addLayout(notifyGrid);
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
