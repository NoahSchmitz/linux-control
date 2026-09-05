#pragma once

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkInterface>
#include <QMenu>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QStyle>
#include <QProcess>
#include <QRadioButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QTabWidget>
#include <QButtonGroup>
#include <QMap>

// Dialog replicating the "Internet Protocol Version 4 (TCP/IPv4) Properties" window
class IPv4PropertiesDialog : public QDialog {
public:
    IPv4PropertiesDialog(const QString &ifaceName, QWidget *parent = nullptr) 
        : QDialog(parent), m_iface(ifaceName) {
        setWindowTitle("Internet Protocol Version 4 (TCP/IPv4) Properties");
        resize(420, 480);

        auto *mainLayout = new QVBoxLayout(this);
        auto *tabWidget = new QTabWidget(this);
        auto *generalTab = new QWidget(tabWidget);
        auto *generalLayout = new QVBoxLayout(generalTab);

        auto *descLabel = new QLabel("You can get IP settings assigned automatically if your network supports\nthis capability. Otherwise, you need to ask your network administrator\nfor the appropriate IP settings.", generalTab);
        generalLayout->addWidget(descLabel);
        generalLayout->addSpacing(10);

        m_dhcpRadio = new QRadioButton("Obtain an IP address automatically", generalTab);
        m_staticRadio = new QRadioButton("Use the following IP address:", generalTab);
        
        // Group IP radios so they don't conflict with DNS radios
        auto *ipGroup = new QButtonGroup(this);
        ipGroup->addButton(m_dhcpRadio);
        ipGroup->addButton(m_staticRadio);

        generalLayout->addWidget(m_dhcpRadio);
        generalLayout->addWidget(m_staticRadio);

        auto *ipLayout = new QFormLayout();
        ipLayout->setContentsMargins(20, 0, 0, 0);
        m_ipEdit = new QLineEdit(generalTab);
        m_maskEdit = new QLineEdit(generalTab);
        m_gatewayEdit = new QLineEdit(generalTab);
        ipLayout->addRow("IP address:", m_ipEdit);
        ipLayout->addRow("Subnet mask:", m_maskEdit);
        ipLayout->addRow("Default gateway:", m_gatewayEdit);
        generalLayout->addLayout(ipLayout);

        generalLayout->addSpacing(15);

        m_dnsDhcpRadio = new QRadioButton("Obtain DNS server address automatically", generalTab);
        m_dnsStaticRadio = new QRadioButton("Use the following DNS server addresses:", generalTab);
        
        // Group DNS radios
        auto *dnsGroup = new QButtonGroup(this);
        dnsGroup->addButton(m_dnsDhcpRadio);
        dnsGroup->addButton(m_dnsStaticRadio);

        generalLayout->addWidget(m_dnsDhcpRadio);
        generalLayout->addWidget(m_dnsStaticRadio);

        auto *dnsLayout = new QFormLayout();
        dnsLayout->setContentsMargins(20, 0, 0, 0);
        m_prefDnsEdit = new QLineEdit(generalTab);
        m_altDnsEdit = new QLineEdit(generalTab);
        dnsLayout->addRow("Preferred DNS server:", m_prefDnsEdit);
        dnsLayout->addRow("Alternate DNS server:", m_altDnsEdit);
        generalLayout->addLayout(dnsLayout);

        generalLayout->addStretch();
        tabWidget->addTab(generalTab, "General");
        tabWidget->addTab(new QWidget(), "Alternate Configuration");
        mainLayout->addWidget(tabWidget);

        auto *buttonBox = new QHBoxLayout();
        buttonBox->addStretch();
        auto *okBtn = new QPushButton("OK", this);
        auto *cancelBtn = new QPushButton("Cancel", this);
        buttonBox->addWidget(okBtn);
        buttonBox->addWidget(cancelBtn);
        mainLayout->addLayout(buttonBox);

        connect(m_dhcpRadio, &QRadioButton::toggled, this, &IPv4PropertiesDialog::toggleIpFields);
        connect(m_dnsDhcpRadio, &QRadioButton::toggled, this, &IPv4PropertiesDialog::toggleDnsFields);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, this, &IPv4PropertiesDialog::applySettings);

        // Fetch current system settings and populate the dialog
        loadCurrentSettings();
    }

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

    void toggleIpFields() {
        bool isStatic = m_staticRadio->isChecked();
        m_ipEdit->setEnabled(isStatic);
        m_maskEdit->setEnabled(isStatic);
        m_gatewayEdit->setEnabled(isStatic);
    }

    void toggleDnsFields() {
        bool isStatic = m_dnsStaticRadio->isChecked();
        m_prefDnsEdit->setEnabled(isStatic);
        m_altDnsEdit->setEnabled(isStatic);
    }

    QString prefixToSubnetMask(int prefix) {
        if (prefix < 0 || prefix > 32) return "";
        uint32_t mask = (prefix == 0) ? 0 : (~0U << (32 - prefix));
        return QString("%1.%2.%3.%4").arg((mask >> 24) & 0xFF).arg((mask >> 16) & 0xFF).arg((mask >> 8) & 0xFF).arg(mask & 0xFF);
    }

    int subnetMaskToPrefix(const QString &mask) {
        QStringList parts = mask.split('.');
        if (parts.size() != 4) return 24; 
        uint32_t m = (parts[0].toUInt() << 24) | (parts[1].toUInt() << 16) | (parts[2].toUInt() << 8) | parts[3].toUInt();
        int prefix = 0;
        for (int i = 31; i >= 0; --i) {
            if (m & (1U << i)) prefix++;
            else break; 
        }
        return prefix;
    }

    void loadCurrentSettings() {
        QProcess proc;
        proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_iface});
        proc.waitForFinished();
        QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        
        if (conName.isEmpty()) {
            m_dhcpRadio->setChecked(true);
            m_dnsDhcpRadio->setChecked(true);
            return;
        }

        proc.start("nmcli", {"-t", "-f", "ipv4.method,ipv4.addresses,ipv4.gateway,ipv4.dns", "con", "show", conName});
        proc.waitForFinished();
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        
        QMap<QString, QString> settings;
        for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
            int colonIdx = line.indexOf(':');
            if (colonIdx != -1) {
                settings[line.left(colonIdx)] = line.mid(colonIdx + 1).trimmed();
            }
        }

        QString method = settings["ipv4.method"];
        QString addresses = settings["ipv4.addresses"];
        QString gateway = settings["ipv4.gateway"];
        QString dns = settings["ipv4.dns"];

        // Apply IP Configuration
        if (method == "manual") {
            m_staticRadio->setChecked(true);
            QString firstAddress = addresses.split(',').first().trimmed();
            if (firstAddress.contains('/')) {
                QStringList parts = firstAddress.split('/');
                m_ipEdit->setText(parts[0]);
                m_maskEdit->setText(prefixToSubnetMask(parts[1].toInt()));
            } else {
                m_ipEdit->setText(firstAddress);
                m_maskEdit->setText("255.255.255.0"); // Fallback
            }
            m_gatewayEdit->setText(gateway);
        } else {
            m_dhcpRadio->setChecked(true);
        }

        // Apply DNS Configuration
        if (!dns.isEmpty() && (method == "manual" || method == "auto")) {
            m_dnsStaticRadio->setChecked(true);
            QStringList dnsList = dns.split(',');
            if (dnsList.size() > 0) m_prefDnsEdit->setText(dnsList[0].trimmed());
            if (dnsList.size() > 1) m_altDnsEdit->setText(dnsList[1].trimmed());
        } else {
            m_dnsDhcpRadio->setChecked(true);
        }

        toggleIpFields();
        toggleDnsFields();
    }

    void applySettings() {
        QProcess proc;
        proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_iface});
        proc.waitForFinished();
        QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        
        if (conName.isEmpty()) {
            accept();
            return;
        }

        QStringList args;
        args << "nmcli" << "con" << "modify" << conName;

        if (m_dhcpRadio->isChecked()) {
            args << "ipv4.method" << "auto" << "ipv4.addresses" << "" << "ipv4.gateway" << "";
        } else {
            int prefix = subnetMaskToPrefix(m_maskEdit->text());
            QString ipCidr = QString("%1/%2").arg(m_ipEdit->text()).arg(prefix);
            args << "ipv4.method" << "manual" << "ipv4.addresses" << ipCidr << "ipv4.gateway" << m_gatewayEdit->text();
        }

        if (m_dnsDhcpRadio->isChecked()) {
            args << "ipv4.dns" << "" << "ipv4.ignore-auto-dns" << "no";
        } else {
            QStringList dnsList;
            if (!m_prefDnsEdit->text().isEmpty()) dnsList << m_prefDnsEdit->text();
            if (!m_altDnsEdit->text().isEmpty()) dnsList << m_altDnsEdit->text();
            args << "ipv4.dns" << dnsList.join(",") << "ipv4.ignore-auto-dns" << "yes";
        }

        // Execute changes via PolicyKit 
        QProcess::execute("pkexec", args);
        
        // Bounce the connection for changes to take effect immediately
        QProcess::execute("pkexec", {"nmcli", "con", "up", conName});

        accept();
    }
};

// Dialog replicating the "Ethernet Properties" / "Networking" tab window
class AdapterPropertiesDialog : public QDialog {
public:
    AdapterPropertiesDialog(const QString &ifaceName, const QString &hardwareName, QWidget *parent = nullptr) 
        : QDialog(parent), m_ifaceName(ifaceName) {
        setWindowTitle(QString("%1 Properties").arg(ifaceName));
        resize(400, 520);

        auto *mainLayout = new QVBoxLayout(this);
        
        auto *tabWidget = new QTabWidget(this);
        auto *netTab = new QWidget(tabWidget);
        auto *netLayout = new QVBoxLayout(netTab);

        netLayout->addWidget(new QLabel("Connect using:", netTab));
        
        auto *adapterRow = new QHBoxLayout();
        auto *iconLabel = new QLabel(netTab);
        iconLabel->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(24, 24));
        adapterRow->addWidget(iconLabel);
        adapterRow->addWidget(new QLabel(hardwareName, netTab), 1);
        auto *configBtn = new QPushButton("Configure...", netTab);
        adapterRow->addWidget(configBtn);
        netLayout->addLayout(adapterRow);

        netLayout->addSpacing(10);
        netLayout->addWidget(new QLabel("This connection uses the following items:", netTab));

        m_itemsList = new QListWidget(netTab);
        addListItem("Client for Microsoft Networks");
        addListItem("File and Printer Sharing for Microsoft Networks");
        addListItem("QoS Packet Scheduler");
        addListItem("Internet Protocol Version 4 (TCP/IPv4)");
        addListItem("Microsoft Network Adapter Multiplexor Protocol", false);
        addListItem("Microsoft LLDP Protocol Driver");
        addListItem("Internet Protocol Version 6 (TCP/IPv6)");
        netLayout->addWidget(m_itemsList);

        auto *listBtns = new QHBoxLayout();
        auto *installBtn = new QPushButton("Install...", netTab);
        auto *uninstallBtn = new QPushButton("Uninstall", netTab);
        m_propsBtn = new QPushButton("Properties", netTab);
        m_propsBtn->setEnabled(false); 

        listBtns->addWidget(installBtn);
        listBtns->addWidget(uninstallBtn);
        listBtns->addStretch();
        listBtns->addWidget(m_propsBtn);
        netLayout->addLayout(listBtns);

        auto *descGroup = new QGroupBox("Description", netTab);
        auto *descLayout = new QVBoxLayout(descGroup);
        m_descLabel = new QLabel("Select an item to see its description.", descGroup);
        m_descLabel->setWordWrap(true);
        m_descLabel->setMinimumHeight(50);
        m_descLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        descLayout->addWidget(m_descLabel);
        netLayout->addWidget(descGroup);

        tabWidget->addTab(netTab, "Networking");
        tabWidget->addTab(new QWidget(), "Sharing");
        mainLayout->addWidget(tabWidget);

        auto *bottomBtns = new QHBoxLayout();
        bottomBtns->addStretch();
        auto *okBtn = new QPushButton("OK", this);
        auto *cancelBtn = new QPushButton("Cancel", this);
        bottomBtns->addWidget(okBtn);
        bottomBtns->addWidget(cancelBtn);
        mainLayout->addLayout(bottomBtns);

        connect(m_itemsList, &QListWidget::itemSelectionChanged, this, &AdapterPropertiesDialog::onSelectionChanged);
        connect(m_propsBtn, &QPushButton::clicked, this, &AdapterPropertiesDialog::openItemProperties);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    }

private:
    QString m_ifaceName;
    QListWidget *m_itemsList;
    QPushButton *m_propsBtn;
    QLabel *m_descLabel;

    void addListItem(const QString &text, bool checked = true) {
        auto *item = new QListWidgetItem(text, m_itemsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }

    void onSelectionChanged() {
        auto items = m_itemsList->selectedItems();
        if (items.isEmpty()) return;
        
        QString text = items.first()->text();
        if (text == "Internet Protocol Version 4 (TCP/IPv4)") {
            m_propsBtn->setEnabled(true);
            m_descLabel->setText("Transmission Control Protocol/Internet Protocol. The default wide area network protocol that provides communication across diverse interconnected networks.");
        } else {
            m_propsBtn->setEnabled(false);
            m_descLabel->setText("Description for " + text + " is not available.");
        }
    }

    void openItemProperties() {
        auto items = m_itemsList->selectedItems();
        if (items.isEmpty()) return;

        if (items.first()->text() == "Internet Protocol Version 4 (TCP/IPv4)") {
            IPv4PropertiesDialog dlg(m_ifaceName, this);
            dlg.exec();
        }
    }
};

// Main Network Connections Window
class NetworkConnectionsWindow : public QWidget {
public:
    NetworkConnectionsWindow(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("Network Connections");
        resize(520, 360);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);

        m_listWidget = new QListWidget(this);
        m_listWidget->setViewMode(QListWidget::IconMode);
        m_listWidget->setIconSize(QSize(32, 32));
        m_listWidget->setGridSize(QSize(150, 70));
        m_listWidget->setResizeMode(QListWidget::Adjust);
        m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(m_listWidget, &QListWidget::customContextMenuRequested, this, &NetworkConnectionsWindow::showContextMenu);
        connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item){ showProperties(item); });

        layout->addWidget(m_listWidget);
        refreshInterfaces();
    }

private:
    QListWidget *m_listWidget;

    void refreshInterfaces() {
        m_listWidget->clear();
        const auto interfaces = QNetworkInterface::allInterfaces();
        for (const auto &interface : interfaces) {
            if (interface.flags().testFlag(QNetworkInterface::IsLoopBack))
                continue;

            QString name = interface.humanReadableName();
            if (name.isEmpty())
                name = interface.name();

            bool isUp = interface.flags().testFlag(QNetworkInterface::IsUp);
            bool isRunning = interface.flags().testFlag(QNetworkInterface::IsRunning);

            QString statusText = (isUp && isRunning) ? "Connected" : "Disconnected";
            QString displayText = QString("%1\n%2").arg(name, statusText);

            auto *item = new QListWidgetItem(displayText, m_listWidget);
            item->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
            item->setData(Qt::UserRole, interface.name());
        }
    }

    void showContextMenu(const QPoint &pos) {
        auto *item = m_listWidget->itemAt(pos);
        if (!item) return;

        QString ifaceName = item->data(Qt::UserRole).toString();
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);
        bool isUp = interface.flags().testFlag(QNetworkInterface::IsUp);

        QMenu contextMenu(this);
        QAction *toggleAction = contextMenu.addAction(isUp ? "Disable" : "Enable");
        contextMenu.addSeparator();
        QAction *statusAction = contextMenu.addAction("Status");
        QAction *propAction = contextMenu.addAction("Properties");
        contextMenu.addSeparator();
        QAction *refreshAction = contextMenu.addAction("Refresh");

        QAction *selectedAction = contextMenu.exec(m_listWidget->mapToGlobal(pos));
        if (selectedAction == toggleAction) {
            toggleInterface(ifaceName, !isUp);
        } else if (selectedAction == statusAction) {
            showStatus(ifaceName);
        } else if (selectedAction == propAction) {
            showProperties(item);
        } else if (selectedAction == refreshAction) {
            refreshInterfaces();
        }
    }

    void toggleInterface(const QString &ifaceName, bool enable) {
        QString state = enable ? "up" : "down";
        QString cmd = QString("pkexec ip link set %1 %2").arg(ifaceName, state);
        QProcess::execute("/bin/sh", QStringList() << "-c" << cmd);
        refreshInterfaces();
    }

    void showStatus(const QString &ifaceName) {
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);
        QDialog dlg(this);
        dlg.setWindowTitle(QString("Status: %1").arg(interface.humanReadableName()));
        dlg.resize(300, 220);

        auto *layout = new QFormLayout(&dlg);
        layout->addRow("Interface:", new QLabel(interface.name(), &dlg));
        layout->addRow("MAC Address:", new QLabel(interface.hardwareAddress(), &dlg));
        
        QString ipAddresses;
        for (const auto &entry : interface.addressEntries()) {
            ipAddresses += entry.ip().toString() + "\n";
        }
        layout->addRow("IP Address:", new QLabel(ipAddresses.trimmed(), &dlg));

        dlg.exec();
    }

    void showProperties(QListWidgetItem *item) {
        QString ifaceName = item->data(Qt::UserRole).toString();
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);

        AdapterPropertiesDialog dlg(ifaceName, interface.humanReadableName(), this);
        dlg.exec();
    }
};