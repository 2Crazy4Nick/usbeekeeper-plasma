#pragma once

#include <QObject>
#include <QString>
#include <QtTypes>

class QDBusPendingCallWatcher;

namespace USBeeKeeper {

struct RequestToken {
    quint64 ownerEpoch = 0;
    quint64 sequence = 0;
};

class RequestSequencer {
public:
    void ownerChanged() { ++m_ownerEpoch; }
    RequestToken nextSnapshot() { return {m_ownerEpoch, ++m_snapshotSequence}; }
    RequestToken nextRefresh() { return {m_ownerEpoch, ++m_refreshSequence}; }
    bool isCurrentSnapshot(const RequestToken &token) const
    {
        return token.ownerEpoch == m_ownerEpoch && token.sequence == m_snapshotSequence;
    }
    bool isCurrentRefresh(const RequestToken &token) const
    {
        return token.ownerEpoch == m_ownerEpoch && token.sequence == m_refreshSequence;
    }

private:
    quint64 m_ownerEpoch = 0;
    quint64 m_snapshotSequence = 0;
    quint64 m_refreshSequence = 0;
};

class UsbeehiveClient : public QObject {
    Q_OBJECT

public:
    explicit UsbeehiveClient(QObject *parent = nullptr);

    void snapshot();
    void refresh();

signals:
    void snapshotReady(const QString &json);
    void serviceAvailableChanged(bool available);
    void snapshotFailed(const QString &error);

private:
    Q_SLOT void onDeviceAdded(const QString &id, const QString &headline);
    Q_SLOT void onDeviceRemoved(const QString &id);
    Q_SLOT void onDeviceChanged(const QString &id);
    Q_SLOT void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                                      const QString &newOwner);
    void handleSnapshot(QDBusPendingCallWatcher *watcher, const RequestToken &token);
    bool serviceIsRegistered() const;

    RequestSequencer m_requests;
};

} // namespace USBeeKeeper
