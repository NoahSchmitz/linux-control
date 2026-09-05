#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QNetworkInterface>
#include <QMenu>
#include <QContextMenuEvent>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QStyle>

class NetworkConnectionsWindow : public QWidget {
    Q_OBJECT
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

            bool isUp = interface.flags().testFlag(QNetworkInterface::IsUp) &&
                        interface.flags().testFlag(QNetworkInterface::IsRunning);

            QString statusText = isUp ? "Connected" : "Disconnected";
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

        QMenu contextMenu(this);
        QAction *statusAction = contextMenu.addAction("Status");
        QAction *propAction = contextMenu.addAction("Properties");
        contextMenu.addSeparator();
        QAction *refreshAction = contextMenu.addAction("Refresh");

        QAction *selectedAction = contextMenu.exec(m_listWidget->mapToGlobal(pos));
        if (selectedAction == statusAction) {
            showStatus(ifaceName);
        } else if (selectedAction == propAction) {
            showProperties(item);
        } else if (selectedAction == refreshAction) {
            refreshInterfaces();
        }
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

        QDialog dlg(this);
        dlg.setWindowTitle(QString("Properties: %1").arg(interface.humanReadableName()));
        dlg.resize(360, 240);

        auto *layout = new QVBoxLayout(&dlg);
        layout->addWidget(new QLabel(QString("Properties for adapter: <b>%1</b>").arg(interface.humanReadableName()), &dlg));
        layout->addWidget(new QLabel(QString("System Name: %1").arg(interface.name()), &dlg));
        layout->addWidget(new QLabel(QString("Hardware Address: %1").arg(interface.hardwareAddress()), &dlg));

        auto *closeBtn = new QPushButton("Close", &dlg);
        connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        layout->addWidget(closeBtn);

        dlg.exec();
    }
};