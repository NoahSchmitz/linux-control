#include "HomeGroupPage.h"
#include "IconHelper.h"
#include "Win7Ui.h"

#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QGroupBox>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QApplication>

// Sidebar
QList<SidebarLink> HomeGroupPage::sidebarLinks()
{
    return {
        Nav::command("Network Settings", QStringList{"systemctl", "restart", "network"}),
        Nav::plain("Change HomeGroup settings"),
        Nav::plain("Leave the HomeGroup"),
    };
}

QList<SidebarLink> HomeGroupPage::sidebarSeeAlso()
{
    return {
        Nav::to("Control Panel Home", PageId::Home),
        Nav::to("Network and Internet", PageId::NetworkSharing),
    };
}

HomeGroupPage::HomeGroupSettings HomeGroupPage::gatherSettings()
{
    HomeGroupSettings settings;

    // HomeGroup is a Windows-specific feature not available on Linux
    // We simulate it with generic network sharing settings
    settings.homeGroupEnabled = false;
    settings.homeGroupName = "MyHomeGroup";

    // Default sharing settings
    settings.sharePictures = true;
    settings.shareMusic = true;
    settings.shareVideos = true;
    settings.sharePrinters = true;

    return settings;
}

// Page
HomeGroupPage::HomeGroupPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const HomeGroupSettings settings = gatherSettings();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("HomeGroup"));
    contentV->addSpacing(18);

    // Intro paragraph
    auto *intro = new QLabel(
        "HomeGroup makes it easy to share files and printers between computers "
        "running Windows on your home network. Note: This feature is Windows-specific. "
        "On Linux, you can use Samba or other network sharing protocols instead.");
    intro->setWordWrap(true);
    intro->setStyleSheet("color: #000000; background: transparent;");
    contentV->addWidget(intro);
    contentV->addSpacing(18);

    // Status section
    auto *statusGroup = new QGroupBox("HomeGroup status");
    auto *statusLayout = new QVBoxLayout;
    statusLayout->setContentsMargins(14, 10, 14, 10);
    statusLayout->setSpacing(8);

    auto *statusRow = new QHBoxLayout;
    auto *statusIcon = new QLabel;
    statusIcon->setPixmap(themeIcon({"network-disconnect", "dialog-warning"}).pixmap(32, 32));
    statusRow->addWidget(statusIcon);

    auto *statusText = new QVBoxLayout;
    statusText->addWidget(new QLabel("HomeGroup is not joined"));
    statusText->addWidget(new QLabel("This computer is not part of a HomeGroup."));
    statusRow->addLayout(statusText);
    statusLayout->addLayout(statusRow);

    statusGroup->setLayout(statusLayout);
    contentV->addWidget(statusGroup);
    contentV->addSpacing(16);

    // Actions
    auto *actionLayout = new QHBoxLayout;
    actionLayout->setContentsMargins(14, 0, 0, 0);
    actionLayout->setSpacing(12);

    auto *createBtn = new QPushButton("Create a HomeGroup");
    createBtn->setCursor(Qt::PointingHandCursor);
    createBtn->setIcon(themeIcon({"document-new", "list-add"}));
    actionLayout->addWidget(createBtn);

    auto *joinBtn = new QPushButton("Join a HomeGroup");
    joinBtn->setCursor(Qt::PointingHandCursor);
    joinBtn->setIcon(themeIcon({"network-workgroup", "folder-sync"}));
    actionLayout->addWidget(joinBtn);

    contentV->addLayout(actionLayout);
    contentV->addSpacing(16);

    // HomeGroup settings
    auto *settingsGroup = new QGroupBox("HomeGroup settings");
    auto *settingsLayout = new QVBoxLayout;
    settingsLayout->setContentsMargins(14, 10, 14, 10);
    settingsLayout->setSpacing(12);

    // Home group name
    auto *nameLayout = new QHBoxLayout;
    nameLayout->addWidget(new QLabel("Home group name:"));
    auto *nameEdit = new QLineEdit(settings.homeGroupName);
    nameLayout->addWidget(nameEdit);
    nameLayout->addStretch(1);
    settingsLayout->addLayout(nameLayout);

    // Shared content
    auto *sharePictures = new QCheckBox("Share pictures");
    sharePictures->setChecked(settings.sharePictures);
    settingsLayout->addWidget(sharePictures);

    auto *shareMusic = new QCheckBox("Share music");
    shareMusic->setChecked(settings.shareMusic);
    settingsLayout->addWidget(shareMusic);

    auto *shareVideos = new QCheckBox("Share videos");
    shareVideos->setChecked(settings.shareVideos);
    settingsLayout->addWidget(shareVideos);

    auto *sharePrinters = new QCheckBox("Share printers");
    sharePrinters->setChecked(settings.sharePrinters);
    settingsLayout->addWidget(sharePrinters);

    settingsGroup->setLayout(settingsLayout);
    contentV->addWidget(settingsGroup);
    contentV->addSpacing(16);

    // Other computers in HomeGroup
    auto *computersGroup = new QGroupBox("Computers in this HomeGroup");
    auto *computersLayout = new QVBoxLayout;
    computersLayout->setContentsMargins(14, 10, 14, 10);
    computersLayout->setSpacing(8);

    auto *computersTree = new QTreeWidget;
    computersTree->setColumnCount(2);
    computersTree->setHeaderHidden(false);
    computersTree->setRootIsDecorated(false);
    computersTree->setIndentation(0);
    computersTree->setFrameShape(QFrame::NoFrame);
    computersTree->setStyleSheet("QTreeWidget { border: 1px solid #d0d0d0; }");

    // Header
    QStringList headers;
    headers << "Computer name" << "Status";
    computersTree->setHeaderLabels(headers);

    computersTree->header()->setStretchLastSection(true);
    computersTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    computersTree->setMinimumHeight(100);

    // Example computers
    auto *item1 = new QTreeWidgetItem(computersTree);
    item1->setText(0, "WORKSTATION1");
    item1->setText(1, "Online");

    auto *item2 = new QTreeWidgetItem(computersTree);
    item2->setText(0, "LAPTOP-01");
    item2->setText(1, "Online");

    computersLayout->addWidget(computersTree);

    auto *refreshBtn = new QPushButton("Refresh");
    refreshBtn->setCursor(Qt::PointingHandCursor);
    computersLayout->addWidget(refreshBtn);

    computersGroup->setLayout(computersLayout);
    contentV->addWidget(computersGroup);

    contentV->addSpacing(16);

    // More information
    auto *infoGroup = new QGroupBox("More information");
    auto *infoLayout = new QVBoxLayout;
    infoLayout->setContentsMargins(14, 10, 14, 10);
    infoLayout->setSpacing(8);

    auto *infoLabel = new QLabel(
        "<a href='#'>What is a HomeGroup?</a><br/>"
        "<a href='#'>How do I join a HomeGroup?</a><br/>"
        "<a href='#'>How do I share files and printers?</a>");
    infoLabel->setOpenExternalLinks(false);
    infoLabel->setStyleSheet("QLabel { color: #1F4E99; background: transparent; }");
    infoLayout->addWidget(infoLabel);

    infoGroup->setLayout(infoLayout);
    contentV->addWidget(infoGroup);

    contentV->addSpacing(16);

    // OK/Cancel
    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(14, 0, 0, 0);
    buttonRow->addStretch(1);

    auto *okBtn = new QPushButton("OK");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setIcon(themeIcon({"dialog-ok", "dialog-apply"}));
    buttonRow->addWidget(okBtn);

    auto *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setIcon(themeIcon({"dialog-cancel", "dialog-close"}));
    buttonRow->addWidget(cancelBtn);

    contentV->addLayout(buttonRow);
    contentV->addStretch(1);
}
