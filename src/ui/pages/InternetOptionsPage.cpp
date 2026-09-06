#include "InternetOptionsPage.h"
#include "IconHelper.h"
#include "Win7Ui.h"

#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QGroupBox>
#include <QSettings>
#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QDir>

// Sidebar
QList<SidebarLink> InternetOptionsPage::sidebarLinks()
{
    return {
        Nav::command("Open Firefox", QStringList{"firefox", "--help"}),
        Nav::command("Open Chrome", QStringList{"google-chrome", "--help"}),
        Nav::plain("Manage add-ons"),
        Nav::plain("Reset Internet settings"),
    };
}

QList<SidebarLink> InternetOptionsPage::sidebarSeeAlso()
{
    return {
        Nav::to("Control Panel Home", PageId::Home),
        Nav::to("Network and Internet", PageId::NetworkSharing),
    };
}

InternetOptionsPage::InternetSettings InternetOptionsPage::gatherSettings()
{
    InternetSettings settings;

    // Default to common browsers
    settings.browserType = "Firefox";
    settings.homePage = "about:home";
    settings.enableCache = true;
    settings.enableCookies = true;
    settings.enableJavaScript = true;

    return settings;
}

// Page
InternetOptionsPage::InternetOptionsPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const InternetSettings settings = gatherSettings();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("Internet Options"));
    contentV->addSpacing(18);

    // Tab widget for Windows 9x/2000 style interface
    auto *tabWidget = new QTabWidget;
    tabWidget->setTabPosition(QTabWidget::North);

    // General tab
    auto *generalTab = new QWidget;
    auto *generalLayout = new QVBoxLayout;
    generalLayout->setContentsMargins(14, 14, 14, 14);
    generalLayout->setSpacing(12);

    // Home page section
    auto *homeGroup = new QGroupBox("Home page");
    auto *homeLayout = new QVBoxLayout;
    homeLayout->setContentsMargins(0, 0, 0, 0);

    auto *urlLayout = new QHBoxLayout;
    urlLayout->addWidget(new QLabel("Address (URL):"));
    auto *urlEdit = new QLineEdit(settings.homePage);
    urlLayout->addWidget(urlEdit);
    homeLayout->addLayout(urlLayout);

    auto *homeBtns = new QHBoxLayout;
    homeBtns->addStretch(1);
    auto *useBtn = new QPushButton("Use current");
    homeBtns->addWidget(useBtn);
    auto *clearBtn = new QPushButton("Clear");
    homeBtns->addWidget(clearBtn);
    homeLayout->addLayout(homeBtns);

    homeGroup->setLayout(homeLayout);
    generalLayout->addWidget(homeGroup);

    // Browsing history section
    auto *historyGroup = new QGroupBox("Browsing history");
    auto *historyLayout = new QVBoxLayout;
    historyLayout->setContentsMargins(0, 0, 0, 0);

    auto *historyBtns = new QHBoxLayout;
    historyBtns->addStretch(1);
    auto *deleteBtn = new QPushButton("Delete");
    historyBtns->addWidget(deleteBtn);
    auto *settingsBtn = new QPushButton("Settings");
    historyBtns->addWidget(settingsBtn);
    historyLayout->addLayout(historyBtns);

    historyGroup->setLayout(historyLayout);
    generalLayout->addWidget(historyGroup);

    // Internet temporary files section
    auto *tempFilesGroup = new QGroupBox("Temporary internet files");
    auto *tempLayout = new QVBoxLayout;
    tempLayout->setContentsMargins(0, 0, 0, 0);

    auto *tempInfo = new QLabel("Current location: ~/.cache");
    tempInfo->setStyleSheet("color: #666666;");
    tempLayout->addWidget(tempInfo);

    auto *tempBtns = new QHBoxLayout;
    tempBtns->addStretch(1);
    auto *deleteTempBtn = new QPushButton("Delete");
    tempBtns->addWidget(deleteTempBtn);
    auto *moveBtn = new QPushButton("Move");
    tempBtns->addWidget(moveBtn);
    tempLayout->addLayout(tempBtns);

    auto *checkBtn = new QCheckBox("Check for newer versions of stored pages");
    tempLayout->addWidget(checkBtn);

    tempFilesGroup->setLayout(tempLayout);
    generalLayout->addWidget(tempFilesGroup);

    generalTab->setLayout(generalLayout);
    tabWidget->addTab(generalTab, "General");

    // Security tab
    auto *securityTab = new QWidget;
    auto *securityLayout = new QVBoxLayout;
    securityLayout->setContentsMargins(14, 14, 14, 14);
    securityLayout->setSpacing(12);

    auto *securityGroup = new QGroupBox("Security level");
    auto *securityBoxLayout = new QVBoxLayout;

    auto *zoneLayout = new QHBoxLayout;
    zoneLayout->addWidget(new QLabel("Security zones:"));
    auto *zoneCombo = new QComboBox;
    zoneCombo->addItem("Internet");
    zoneCombo->addItem("Local intranet");
    zoneCombo->addItem("Trusted sites");
    zoneCombo->addItem("Restricted sites");
    zoneCombo->setCurrentIndex(0);
    zoneLayout->addWidget(zoneCombo);
    securityBoxLayout->addLayout(zoneLayout);

    auto *levelLayout = new QHBoxLayout;
    levelLayout->addWidget(new QLabel("Security level:"));
    auto *levelCombo = new QComboBox;
    levelCombo->addItem("High");
    levelCombo->addItem("Medium-High");
    levelCombo->addItem("Medium");
    levelCombo->addItem("Low");
    levelCombo->setCurrentIndex(2);
    levelLayout->addWidget(levelCombo);
    securityBoxLayout->addLayout(levelLayout);

    securityGroup->setLayout(securityBoxLayout);
    securityLayout->addWidget(securityGroup);

    securityTab->setLayout(securityLayout);
    tabWidget->addTab(securityTab, "Security");

    // Privacy tab
    auto *privacyTab = new QWidget;
    auto *privacyLayout = new QVBoxLayout;
    privacyLayout->setContentsMargins(14, 14, 14, 14);
    privacyLayout->setSpacing(12);

    auto *privacyGroup = new QGroupBox("Privacy");
    auto *privacyBoxLayout = new QVBoxLayout;

    auto *popupLayout = new QHBoxLayout;
    popupLayout->addWidget(new QLabel("Pop-up blocker:"));
    auto *popupCombo = new QComboBox;
    popupCombo->addItem("High (Block all pop-ups)");
    popupCombo->addItem("Medium (Block pop-ups with exceptions)");
    popupCombo->addItem("Low (Allow most pop-ups)");
    popupCombo->setCurrentIndex(1);
    popupLayout->addWidget(popupCombo);
    privacyBoxLayout->addLayout(popupLayout);

    auto *cookieLayout = new QHBoxLayout;
    cookieLayout->addWidget(new QLabel("Cookies:"));
    auto *cookieCombo = new QComboBox;
    cookieCombo->addItem("Allow");
    cookieCombo->addItem("Ask each time");
    cookieCombo->addItem("Block");
    cookieCombo->setCurrentIndex(0);
    cookieLayout->addWidget(cookieCombo);
    privacyBoxLayout->addLayout(cookieLayout);

    privacyGroup->setLayout(privacyBoxLayout);
    privacyLayout->addWidget(privacyGroup);

    privacyTab->setLayout(privacyLayout);
    tabWidget->addTab(privacyTab, "Privacy");

    // Connections tab
    auto *connectionsTab = new QWidget;
    auto *connectionsLayout = new QVBoxLayout;
    connectionsLayout->setContentsMargins(14, 14, 14, 14);
    connectionsLayout->setSpacing(12);

    auto *connectionsGroup = new QGroupBox("LAN Settings");
    auto *connectionsBoxLayout = new QVBoxLayout;

    auto *proxyLayout = new QHBoxLayout;
    proxyLayout->addWidget(new QLabel("Proxy server:"));
    auto *proxyEdit = new QLineEdit;
    proxyLayout->addWidget(proxyEdit);
    connectionsBoxLayout->addLayout(proxyLayout);

    auto *proxyPortLayout = new QHBoxLayout;
    proxyPortLayout->addWidget(new QLabel("Port:"));
    auto *portEdit = new QLineEdit;
    portEdit->setText("8080");
    proxyPortLayout->addWidget(portEdit);
    connectionsBoxLayout->addLayout(proxyPortLayout);

    connectionsGroup->setLayout(connectionsBoxLayout);
    connectionsLayout->addWidget(connectionsGroup);

    connectionsTab->setLayout(connectionsLayout);
    tabWidget->addTab(connectionsTab, "Connections");

    // Programs tab
    auto *programsTab = new QWidget;
    auto *programsLayout = new QVBoxLayout;
    programsLayout->setContentsMargins(14, 14, 14, 14);
    programsLayout->setSpacing(12);

    auto *programsGroup = new QGroupBox("Default web browser");
    auto *programsBoxLayout = new QVBoxLayout;

    auto *browserLayout = new QHBoxLayout;
    browserLayout->addWidget(new QLabel("Current browser:"));
    auto *browserCombo = new QComboBox;
    browserCombo->addItem("Firefox");
    browserCombo->addItem("Chrome");
    browserCombo->addItem("Chromium");
    browserCombo->addItem("Other");
    browserCombo->setCurrentText(settings.browserType);
    browserLayout->addWidget(browserCombo);
    programsBoxLayout->addLayout(browserLayout);

    auto *makeDefaultBtn = new QPushButton("Make Default");
    programsBoxLayout->addWidget(makeDefaultBtn);

    programsGroup->setLayout(programsBoxLayout);
    programsLayout->addWidget(programsGroup);

    // Internet options
    auto *internetOptionsGroup = new QGroupBox("Internet options");
    auto *internetBoxLayout = new QVBoxLayout;

    auto *javaScriptCheck = new QCheckBox("Enable JavaScript");
    javaScriptCheck->setChecked(settings.enableJavaScript);
    internetBoxLayout->addWidget(javaScriptCheck);

    auto *activeXCheck = new QCheckBox("Run ActiveX controls and plugins");
    internetBoxLayout->addWidget(activeXCheck);

    auto *scriptingCheck = new QCheckBox("Allow scripting of Internet Explorer WebBrowser control");
    internetBoxLayout->addWidget(scriptingCheck);

    internetOptionsGroup->setLayout(internetBoxLayout);
    programsLayout->addWidget(internetOptionsGroup);

    programsTab->setLayout(programsLayout);
    tabWidget->addTab(programsTab, "Programs");

    // Advanced tab
    auto *advancedTab = new QWidget;
    auto *advancedLayout = new QVBoxLayout;
    advancedLayout->setContentsMargins(14, 14, 14, 14);
    advancedLayout->setSpacing(12);

    auto *advancedGroup = new QGroupBox("Advanced settings");
    auto *advancedBoxLayout = new QVBoxLayout;

    auto *cacheCheck = new QCheckBox("Automatically detect settings");
    cacheCheck->setChecked(settings.enableCache);
    advancedBoxLayout->addWidget(cacheCheck);

    auto *cookiesCheck = new QCheckBox("Allow cookies");
    cookiesCheck->setChecked(settings.enableCookies);
    advancedBoxLayout->addWidget(cookiesCheck);

    auto *smoothScrollingCheck = new QCheckBox("Use smooth scrolling");
    advancedBoxLayout->addWidget(smoothScrollingCheck);

    advancedGroup->setLayout(advancedBoxLayout);
    advancedLayout->addWidget(advancedGroup);

    advancedTab->setLayout(advancedLayout);
    tabWidget->addTab(advancedTab, "Advanced");

    contentV->addWidget(tabWidget);
    contentV->addSpacing(16);

    // OK/Cancel/Apply
    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(14, 0, 0, 0);
    buttonRow->addStretch(1);

    auto *okBtn = new QPushButton("OK");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setIcon(themeIcon({"dialog-ok", "dialog-apply"}));
    buttonRow->addWidget(okBtn);

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
