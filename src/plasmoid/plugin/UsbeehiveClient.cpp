#include "UsbeehiveClient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>

namespace {
constexpr auto kService = "org.usbeehive.Devices";
constexpr auto kPath = "/org/usbeehive/Devices";
constexpr auto kInterface = "org.usbeehive.Devices5";
}

namespace WhatCable {

UsbeehiveClient::UsbeehiveClient(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
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

void UsbeehiveClient::snapshot()
{
    auto *interface = new QDBusInterface(kService, kPath, kInterface,
                                         QDBusConnection::sessionBus(), this);
    auto *watcher = new QDBusPendingCallWatcher(interface->asyncCall(QStringLiteral("SnapshotJson")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, interface](QDBusPendingCallWatcher *call) {
                handleSnapshot(call);
                interface->deleteLater();
            });
}

void UsbeehiveClient::refresh()
{
    auto *interface = new QDBusInterface(kService, kPath, kInterface,
                                         QDBusConnection::sessionBus(), this);
    auto *watcher = new QDBusPendingCallWatcher(interface->asyncCall(QStringLiteral("Refresh")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, interface](QDBusPendingCallWatcher *call) {
                if (!call->isError())
                    snapshot();
                call->deleteLater();
                interface->deleteLater();
            });
}

void UsbeehiveClient::handleSnapshot(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QString> reply = *watcher;
    if (!reply.isError())
        emit snapshotReady(reply.value());
    watcher->deleteLater();
}

} // namespace WhatCable
