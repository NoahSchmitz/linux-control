#include "DefaultProgramsPage.h"
#include "IconHelper.h"
#include "Win7Ui.h"

#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QGroupBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QComboBox>
#include <QHeaderView>
#include <QApplication>

// Sidebar
QList<SidebarLink> DefaultProgramsPage::sidebarLinks()
{
    return {
        Nav::command("Open File Manager", QStringList{"dolphin", "--help"}),
        Nav::plain("Choose a default web browser"),
        Nav::plain("Choose a default email program"),
        Nav::plain("Associate a file type with a program"),
    };
}

QList<SidebarLink> DefaultProgramsPage::sidebarSeeAlso()
{
    return {
        Nav::to("Control Panel Home", PageId::Home),
        Nav::to("Programs", PageId::ProgramsFeatures),
    };
}

DefaultProgramsPage::DefaultPrograms DefaultProgramsPage::gatherDefaults()
{
    DefaultPrograms defaults;

    // Default to common Linux applications
    defaults.browser = "Firefox";
    defaults.emailClient = "Thunderbird";
    defaults.mediaPlayer = "VLC";
    defaults.photoViewer = "Image Viewer";

    return defaults;
}

// Page
DefaultProgramsPage::DefaultProgramsPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const DefaultPrograms defaults = gatherDefaults();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("Default Programs"));
    contentV->addSpacing(18);

    // Intro paragraph
    auto *intro = new QLabel(
        "Default Programs lets you choose which programs Windows uses for "
        "common tasks like browsing the web, viewing pictures, and playing music.");
    intro->setWordWrap(true);
    intro->setStyleSheet("color: #000000; background: transparent;");
    contentV->addWidget(intro);
    contentV->addSpacing(18);

    // Commands
    auto *commandLayout = new QHBoxLayout;
    commandLayout->setContentsMargins(14, 0, 0, 0);
    commandLayout->setSpacing(12);

    auto *browseBtn = new QPushButton("Browse for more programs");
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setIcon(themeIcon({"document-open", "folder"}));
    commandLayout->addWidget(browseBtn);

    auto *associateBtn = new QPushButton("Associate a file type or protocol with a program");
    associateBtn->setCursor(Qt::PointingHandCursor);
    associateBtn->setIcon(themeIcon({"document-new", "list-add"}));
    commandLayout->addWidget(associateBtn);

    auto *setDefaultBtn = new QPushButton("Set your default programs");
    setDefaultBtn->setCursor(Qt::PointingHandCursor);
    setDefaultBtn->setIcon(themeIcon({"system-run", "go-run"}));
    commandLayout->addWidget(setDefaultBtn);

    contentV->addLayout(commandLayout);
    contentV->addSpacing(16);

    // Default programs section
    auto *defaultGroup = new QGroupBox("Default programs");
    auto *defaultLayout = new QVBoxLayout;
    defaultLayout->setContentsMargins(14, 10, 14, 10);
    defaultLayout->setSpacing(12);

    // Web Browser
    auto *browserLayout = new QHBoxLayout;
    browserLayout->addWidget(new QLabel("Web browser:"));
    auto *browserCombo = new QComboBox;
    browserCombo->addItem("Firefox");
    browserCombo->addItem("Chrome");
    browserCombo->addItem("Chromium");
    browserCombo->addItem("Other");
    browserCombo->setCurrentText(defaults.browser);
    browserLayout->addWidget(browserCombo);
    browserLayout->addStretch(1);
    defaultLayout->addLayout(browserLayout);

    // Email client
    auto *emailLayout = new QHBoxLayout;
    emailLayout->addWidget(new QLabel("Email client:"));
    auto *emailCombo = new QComboBox;
    emailCombo->addItem("Thunderbird");
    emailCombo->addItem("Evolution");
    emailCombo->addItem("Claws Mail");
    emailCombo->addItem("Other");
    emailCombo->setCurrentText(defaults.emailClient);
    emailLayout->addWidget(emailCombo);
    emailLayout->addStretch(1);
    defaultLayout->addLayout(emailLayout);

    // Media player
    auto *mediaLayout = new QHBoxLayout;
    mediaLayout->addWidget(new QLabel("Music player:"));
    auto *mediaCombo = new QComboBox;
    mediaCombo->addItem("VLC");
    mediaCombo->addItem("Rhythmbox");
    mediaCombo->addItem("Audacious");
    mediaCombo->addItem("Other");
    mediaCombo->setCurrentText(defaults.mediaPlayer);
    mediaLayout->addWidget(mediaCombo);
    mediaLayout->addStretch(1);
    defaultLayout->addLayout(mediaLayout);

    // Photo viewer
    auto *photoLayout = new QHBoxLayout;
    photoLayout->addWidget(new QLabel("Photo viewer:"));
    auto *photoCombo = new QComboBox;
    photoCombo->addItem("Image Viewer");
    photoCombo->addItem("GIMP");
    photoCombo->addItem("KolourPaint");
    photoCombo->addItem("Other");
    photoCombo->setCurrentText(defaults.photoViewer);
    photoLayout->addWidget(photoCombo);
    photoLayout->addStretch(1);
    defaultLayout->addLayout(photoLayout);

    defaultGroup->setLayout(defaultLayout);
    contentV->addWidget(defaultGroup);
    contentV->addSpacing(16);

    // File type associations
    auto *assocGroup = new QGroupBox("File type and protocol associations");
    auto *assocLayout = new QVBoxLayout;
    assocLayout->setContentsMargins(14, 10, 14, 10);
    assocLayout->setSpacing(8);

    // Tree widget for associations
    auto *assocTree = new QTreeWidget;
    assocTree->setColumnCount(2);
    assocTree->setHeaderHidden(false);
    assocTree->setRootIsDecorated(false);
    assocTree->setIndentation(0);
    assocTree->setFrameShape(QFrame::NoFrame);
    assocTree->setStyleSheet("QTreeWidget { border: 1px solid #d0d0d0; }");

    // Header
    assocTree->header()->setStretchLastSection(true);
    assocTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    assocTree->setMinimumHeight(120);

    // Example associations
    QStringList headers;
    headers << "File type" << "Default program";
    assocTree->setHeaderLabels(headers);

    auto *item1 = new QTreeWidgetItem(assocTree);
    item1->setText(0, ".htm, .html");
    item1->setText(1, "Firefox");

    auto *item2 = new QTreeWidgetItem(assocTree);
    item2->setText(0, ".jpg, .jpeg");
    item2->setText(1, "Image Viewer");

    auto *item3 = new QTreeWidgetItem(assocTree);
    item3->setText(0, ".mp3");
    item3->setText(1, "VLC");

    auto *item4 = new QTreeWidgetItem(assocTree);
    item4->setText(0, ".mp4");
    item4->setText(1, "VLC");

    auto *item5 = new QTreeWidgetItem(assocTree);
    item5->setText(0, ".pdf");
    item5->setText(1, "PDF Viewer");

    contentV->addWidget(assocTree);
    contentV->addSpacing(10);

    // Association buttons
    auto *assocBtnLayout = new QHBoxLayout;
    assocBtnLayout->setContentsMargins(14, 0, 0, 0);
    assocBtnLayout->setSpacing(8);

    auto *changeBtn = new QPushButton("Change program");
    changeBtn->setCursor(Qt::PointingHandCursor);
    assocBtnLayout->addWidget(changeBtn);

    auto *newBtn = new QPushButton("New");
    newBtn->setCursor(Qt::PointingHandCursor);
    assocBtnLayout->addWidget(newBtn);

    auto *deleteBtn = new QPushButton("Delete");
    deleteBtn->setCursor(Qt::PointingHandCursor);
    assocBtnLayout->addWidget(deleteBtn);

    assocLayout->addLayout(assocBtnLayout);
    assocGroup->setLayout(assocLayout);
    contentV->addWidget(assocGroup);

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
