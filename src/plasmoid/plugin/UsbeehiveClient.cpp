#include "UsbeehiveClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusServiceWatcher>

namespace {
constexpr auto kService = "org.usbeehive.Devices";
constexpr auto kPath = "/org/usbeehive/Devices";
constexpr auto kInterface = "org.usbeehive.Devices5";
}

namespace USBeeKeeper {

UsbeehiveClient::UsbeehiveClient(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
    auto *serviceWatcher = new QDBusServiceWatcher(QString::fromLatin1(kService), bus,
                                                    QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &UsbeehiveClient::onServiceOwnerChanged);
    bus.connect(kService, kPath, kInterface, "DeviceAdded", this,
                SLOT(onDeviceAdded(QString,QString)));
    bus.connect(kService, kPath, kInterface, "DeviceRemoved", this,
                SLOT(onDeviceRemoved(QString)));
    bus.connect(kService, kPath, kInterface, "DeviceChanged", this,
                SLOT(onDeviceChanged(QString)));
}

void UsbeehiveClient::onDeviceAdded(const QString &, const QString &) { snapshot(); }
void UsbeehiveClient::onDeviceRemoved(const QString &) { snapshot(); }
void UsbeehiveClient::onDeviceChanged(const QString &) { snapshot(); }

void UsbeehiveClient::onServiceOwnerChanged(const QString &, const QString &oldOwner,
                                            const QString &newOwner)
{
    m_requests.ownerChanged();
    const bool oldOwnerMustBeCleared = !oldOwner.isEmpty() && oldOwner != newOwner;
    if (oldOwnerMustBeCleared)
        emit serviceAvailableChanged(false);
    if (newOwner.isEmpty()) {
        if (!oldOwnerMustBeCleared)
            emit serviceAvailableChanged(false);
    } else {
        emit serviceAvailableChanged(true);
        snapshot();
    }
}

void UsbeehiveClient::snapshot()
{
    const auto token = m_requests.nextSnapshot();
    auto *interface = new QDBusInterface(kService, kPath, kInterface,
                                         QDBusConnection::sessionBus(), this);
    auto *watcher = new QDBusPendingCallWatcher(interface->asyncCall(QStringLiteral("SnapshotJson")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, interface, token](QDBusPendingCallWatcher *call) {
                handleSnapshot(call, token);
                interface->deleteLater();
            });
}

void UsbeehiveClient::refresh()
{
    const auto token = m_requests.nextRefresh();
    auto *interface = new QDBusInterface(kService, kPath, kInterface,
                                         QDBusConnection::sessionBus(), this);
    auto *watcher = new QDBusPendingCallWatcher(interface->asyncCall(QStringLiteral("Refresh")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, interface, token](QDBusPendingCallWatcher *call) {
                if (!m_requests.isCurrentRefresh(token)) {
                    call->deleteLater();
                    interface->deleteLater();
                    return;
                }
                if (!call->isError()) {
                    snapshot();
                } else if (serviceIsRegistered()) {
                    if (m_requests.acceptRefreshFailure(token))
                        emit snapshotFailed(call->error().message());
                } else {
                    m_requests.ownerChanged();
                    emit serviceAvailableChanged(false);
                }
                call->deleteLater();
                interface->deleteLater();
            });
}

void UsbeehiveClient::handleSnapshot(QDBusPendingCallWatcher *watcher, const RequestToken &token)
{
    if (!m_requests.isCurrentSnapshot(token)) {
        watcher->deleteLater();
        return;
    }
    QDBusPendingReply<QString> reply = *watcher;
    if (!reply.isError()) {
        emit snapshotReady(reply.value());
    } else if (serviceIsRegistered()) {
        emit snapshotFailed(reply.error().message());
    } else {
        m_requests.ownerChanged();
        emit serviceAvailableChanged(false);
    }
    watcher->deleteLater();
}

bool UsbeehiveClient::serviceIsRegistered() const
{
    const auto *interface = QDBusConnection::sessionBus().interface();
    if (!interface) return false;
    const auto reply = interface->isServiceRegistered(QString::fromLatin1(kService));
    return reply.isValid() && reply.value();
}

} // namespace USBeeKeeper
