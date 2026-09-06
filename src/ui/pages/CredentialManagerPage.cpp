#include "CredentialManagerPage.h"
#include "IconHelper.h"
#include "Win7Ui.h"

#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QGroupBox>
#include <QFormLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLineEdit>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>

// Sidebar
QList<SidebarLink> CredentialManagerPage::sidebarLinks()
{
    return {
        Nav::command("Open Wallet", QStringList{"gnome-keyring-manager"}),
        Nav::plain("Add a Windows credential"),
        Nav::plain("Add a generic credential"),
        Nav::plain("Back up credentials"),
    };
}

QList<SidebarLink> CredentialManagerPage::sidebarSeeAlso()
{
    return {
        Nav::to("Control Panel Home", PageId::Home),
        Nav::to("User Accounts", PageId::UserAccounts),
        Nav::to("Network and Internet", PageId::NetworkSharing),
    };
}

QList<CredentialManagerPage::Credential> CredentialManagerPage::gatherCredentials()
{
    QList<Credential> credentials;

    // In a Linux environment, we can't truly read Windows credentials
    // This is a placeholder showing where the real implementation would go
    // For demonstration, we show some example credentials that might be stored

    Credential cred1;
    cred1.service = "Windows Network";
    cred1.username = "user@example.com";
    cred1.domain = "EXAMPLE";
    cred1.lastUsed = "Today";
    credentials << cred1;

    Credential cred2;
    cred2.service = "Web Server";
    cred2.username = "admin";
    cred2.domain = "";
    cred2.lastUsed = "Yesterday";
    credentials << cred2;

    Credential cred3;
    cred3.service = "Database";
    cred3.username = "dbuser";
    cred3.domain = "";
    cred3.lastUsed = "Last week";
    credentials << cred3;

    return credentials;
}

// Page
CredentialManagerPage::CredentialManagerPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const QList<Credential> credentials = gatherCredentials();

    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("Credential Manager"));
    contentV->addSpacing(18);

    // Intro paragraph
    auto *intro = new QLabel(
        "Credential Manager stores user names and passwords for websites, "
        "network resources, and applications. When you sign in to a resource, "
        "Credential Manager can provide the appropriate user name and password.");
    intro->setWordWrap(true);
    intro->setStyleSheet("color: #000000; background: transparent;");
    contentV->addWidget(intro);
    contentV->addSpacing(18);

    // Command bar
    auto *commandBar = new QFrame;
    commandBar->setFixedHeight(28);
    commandBar->setStyleSheet("background: #f5f5f5; border: 1px solid #d0d0d0;");
    auto *commandLayout = new QHBoxLayout(commandBar);
    commandLayout->setContentsMargins(8, 0, 8, 0);
    commandLayout->setSpacing(6);

    auto *addBtn = new QPushButton("Add Windows credential");
    addBtn->setCursor(Qt::PointingHandCursor);
    commandLayout->addWidget(addBtn);

    auto *addGenericBtn = new QPushButton("Add a generic credential");
    addGenericBtn->setCursor(Qt::PointingHandCursor);
    commandLayout->addWidget(addGenericBtn);

    contentV->addWidget(commandBar);

    // Credentials list
    auto *listHeader = new QFrame;
    listHeader->setFixedHeight(24);
    listHeader->setStyleSheet("background: #f0f0f0; border: 1px solid #d0d0d0;");
    auto *listHeaderLayout = new QHBoxLayout(listHeader);
    listHeaderLayout->setContentsMargins(8, 0, 8, 0);
    listHeaderLayout->setSpacing(4);

    auto *serviceLabel = new QLabel("Item");
    serviceLabel->setStyleSheet("font-weight: bold; color: #000000;");
    listHeaderLayout->addWidget(serviceLabel);

    auto *userLabel = new QLabel("User name");
    userLabel->setStyleSheet("font-weight: bold; color: #000000;");
    listHeaderLayout->addWidget(userLabel);

    auto *lastUsedLabel = new QLabel("Last used");
    lastUsedLabel->setStyleSheet("font-weight: bold; color: #000000;");
    listHeaderLayout->addWidget(lastUsedLabel);

    contentV->addWidget(listHeader);

    // Tree widget for credentials
    auto *treeWidget = new QTreeWidget;
    treeWidget->setColumnCount(3);
    treeWidget->setHeaderHidden(true);
    treeWidget->setRootIsDecorated(false);
    treeWidget->setIndentation(0);
    treeWidget->setFrameShape(QFrame::NoFrame);
    treeWidget->setStyleSheet("QTreeWidget { border: 1px solid #d0d0d0; }");

    for (const Credential &cred : credentials) {
        auto *item = new QTreeWidgetItem(treeWidget);
        item->setText(0, cred.service);
        item->setText(1, cred.username);
        item->setText(2, cred.lastUsed);
        item->setFirstColumnSpanned(false);
    }

    treeWidget->header()->setStretchLastSection(true);
    treeWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    treeWidget->setMinimumHeight(150);

    contentV->addWidget(treeWidget);

    // Command bar below list
    auto *listCommandBar = new QFrame;
    listCommandBar->setFixedHeight(28);
    listCommandBar->setStyleSheet("background: #f5f5f5; border: 1px solid #d0d0d0;");
    auto *listCommandLayout = new QHBoxLayout(listCommandBar);
    listCommandLayout->setContentsMargins(8, 0, 8, 0);
    listCommandLayout->setSpacing(6);

    auto *editBtn = new QPushButton("Edit");
    editBtn->setCursor(Qt::PointingHandCursor);
    listCommandLayout->addWidget(editBtn);

    auto *removeBtn = new QPushButton("Remove");
    removeBtn->setCursor(Qt::PointingHandCursor);
    listCommandLayout->addWidget(removeBtn);

    auto *viewBtn = new QPushButton("View");
    viewBtn->setCursor(Qt::PointingHandCursor);
    listCommandLayout->addWidget(viewBtn);

    contentV->addWidget(listCommandBar);

    contentV->addSpacing(16);

    // Details section
    auto *detailsGroup = new QGroupBox("Selected item details");
    auto *detailsLayout = new QFormLayout;
    detailsLayout->setContentsMargins(14, 10, 14, 10);
    detailsLayout->setVerticalSpacing(8);

    auto *serviceEdit = new QLineEdit;
    serviceEdit->setReadOnly(true);
    detailsLayout->addRow("Item:", serviceEdit);

    auto *userEdit = new QLineEdit;
    userEdit->setReadOnly(true);
    detailsLayout->addRow("User name:", userEdit);

    auto *domainEdit = new QLineEdit;
    domainEdit->setReadOnly(true);
    detailsLayout->addRow("Domain:", domainEdit);

    auto *passwordEdit = new QLineEdit;
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setReadOnly(true);
    detailsLayout->addRow("Password:", passwordEdit);

    auto *viewPasswordLayout = new QHBoxLayout;
    auto *showPasswordBtn = new QPushButton("Show");
    showPasswordBtn->setCursor(Qt::PointingHandCursor);
    viewPasswordLayout->addWidget(showPasswordBtn);
    viewPasswordLayout->addStretch(1);
    detailsLayout->addRow(viewPasswordLayout);

    detailsGroup->setLayout(detailsLayout);
    contentV->addWidget(detailsGroup);

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
