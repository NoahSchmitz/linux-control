#pragma once

#include <QDialog>
#include <QWidget>
#include <QString>
#include <QListWidget>
#include <QScrollArea>

#include "PageId.h"
#include "Win7Ui.h"
#include "Commands.h"

// Forward declarations
class QRadioButton;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QTimer;
class QListWidgetItem;

class IPv4PropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit IPv4PropertiesDialog(const QString &ifaceName, QWidget *parent = nullptr);

private:
    QString m_iface;
    QRadioButton *m_dhcpRadio;
    QRadioButton *m_staticRadio;
    QLineEdit *m_ipEdit;
    QLineEdit *m_maskEdit;
    QLineEdit *m_gatewayEdit;

    QRadioButton *m_dnsDhcpRadio;
    QRadioButton *m_dnsStaticRadio;
    QLineEdit *m_prefDnsEdit;
    QLineEdit *m_altDnsEdit;

    void toggleIpFields();
    void toggleDnsFields();
    QString prefixToSubnetMask(int prefix);
    int subnetMaskToPrefix(const QString &mask);
    void loadCurrentSettings();
    void applySettings();
};

class IPv6PropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit IPv6PropertiesDialog(const QString &ifaceName, QWidget *parent = nullptr);

private:
    QString m_iface;
    QRadioButton *m_dhcpRadio;
    QRadioButton *m_staticRadio;
    QRadioButton *m_dnsDhcpRadio;
    QRadioButton *m_dnsStaticRadio;
    QLineEdit *m_ipEdit;
    QLineEdit *m_prefixEdit;
    QLineEdit *m_gatewayEdit;
    QLineEdit *m_prefDnsEdit;
    QLineEdit *m_altDnsEdit;

    void toggleFields();
    QString getConnectionName();
    void loadCurrentSettings();
    void applySettings();
};

class QoSPropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit QoSPropertiesDialog(const QString &ifaceName, QWidget *parent = nullptr);

private:
    QString m_iface;
    QCheckBox *m_enableCb;
    QSpinBox *m_rateSpinBox;

    void loadQos();
    void applyQos();
};

class AdapterPropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdapterPropertiesDialog(const QString &ifaceName, const QString &hardwareName, QWidget *parent = nullptr);

private:
    QString m_ifaceName;
    QListWidget *m_itemsList;
    QPushButton *m_propsBtn;
    QLabel *m_descLabel;

    void addListItem(const QString &text, bool checked = true, const QIcon &icon = QIcon());
    bool getLldpState();
    bool getQosState();
    bool getIpv4State();
    bool getIpv6State();
    void onSelectionChanged();
    void openItemProperties();
    void applySettings();
};

class ConnectionStatusDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionStatusDialog(const QString &ifaceName, QWidget *parent = nullptr);

private:
    QString m_iface;
    bool m_isWifi;
    int m_secondsConnected;
    QTimer *m_timer;

    QLabel *m_statusLabel;
    QLabel *m_durationLabel;
    QLabel *m_speedLabel;
    QLabel *m_networkLabel;
    QProgressBar *m_signalBar;
    QLabel *m_sentLabel;
    QLabel *m_recvLabel;

    QLabel *m_addrTypeLabel;
    QLabel *m_ipLabel;
    QLabel *m_maskLabel;
    QLabel *m_gatewayLabel;

    void checkIfaceType();
    quint64 readSysStat(const QString &stat);
    void updateMetrics();
    void loadSupportData();
    void showDetailsDialog();
};

class QTreeWidget;

class WirelessNetworksDialog : public QDialog {
    Q_OBJECT
public:
    explicit WirelessNetworksDialog(const QString &ifaceName, QWidget *parent = nullptr);

private:
    QString m_iface;
    QTreeWidget *m_tree;
    QPushButton *m_connectBtn;

    void refreshNetworks();
    void connectToNetwork();
};

class NetworkConnectionsPage : public QWidget {
    Q_OBJECT
public:
    explicit NetworkConnectionsPage(QScrollArea *sidebar, QWidget *parent = nullptr);

    static QList<SidebarLink> sidebarLinks();
    static QList<SidebarLink> sidebarSeeAlso();

    void disableSelected();
    void showSelectedStatus();
    void showSelectedProperties();
    void showWirelessNetworks();
    static void openWirelessNetworksDialog(QWidget *parent, QString ifaceToUse = QString());
    static void openConnectionStatusDialog(const QString &ifaceName, QWidget *parent);

private:
    QListWidget *m_listWidget;
    QScrollArea *m_sidebar;
    
    void refreshInterfaces();
    void showContextMenu(const QPoint &pos);
    void toggleInterface(const QString &ifaceName, bool enable);
    void showStatus(const QString &ifaceName);
    void showProperties(QListWidgetItem *item);
    void updateSidebar();
};