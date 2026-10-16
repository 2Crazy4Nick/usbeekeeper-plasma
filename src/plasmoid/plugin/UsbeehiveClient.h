#pragma once

#include <QObject>
#include <QString>

class QDBusPendingCallWatcher;

namespace WhatCable {

class UsbeehiveClient : public QObject {
    Q_OBJECT

public:
    explicit UsbeehiveClient(QObject *parent = nullptr);

    void snapshot();
    void refresh();

signals:
    void snapshotReady(const QString &json);

private:
    Q_SLOT void onDeviceAdded(const QString &id, const QString &headline);
    Q_SLOT void onDeviceRemoved(const QString &id);
    Q_SLOT void onDeviceChanged(const QString &id);
    void handleSnapshot(QDBusPendingCallWatcher *watcher);
};

} // namespace WhatCable
