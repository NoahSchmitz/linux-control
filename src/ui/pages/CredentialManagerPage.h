#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QList>
#include "PageId.h"

class QScrollArea;
class QVBoxLayout;

// The "Credential Manager" detail page, a Linux-flavoured take on the Windows 7
// Credential Manager page. This page provides access to stored credentials
// and password management.
class CredentialManagerPage : public QWidget {
    Q_OBJECT

public:
    explicit CredentialManagerPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

private:
    struct Credential {
        QString service;
        QString username;
        QString domain;
        QString lastUsed;
    };

    static QList<Credential> gatherCredentials();
};
