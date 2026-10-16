#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QList>
#include "UsbeehiveClient.h"

namespace WhatCable {

class DeviceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool showHubs READ showHubs WRITE setShowHubs NOTIFY showHubsChanged)
    Q_PROPERTY(bool showEmptyPorts READ showEmptyPorts WRITE setShowEmptyPorts NOTIFY showEmptyPortsChanged)

public:
    enum Roles {
        HeadlineRole = Qt::UserRole + 1,
        SubtitleRole,
        IconRole,
        CategoryRole,
        BulletsRole,
        StatusRole,
        // USB device fields
        VendorIdRole,
        ProductIdRole,
        SpeedRole,
        SpeedLabelRole,
        UsbVersionRole,
        MaxPowerRole,
        IsHubRole,
        BusPortRole,
        // Type-C fields
        PortNumberRole,
        DataRoleRole,
        PowerRoleRole,
        IsConnectedRole,
        // Cable
        CableSpeedRole,
        CableCurrentRole,
        CableMaxWattsRole,
        CableTypeRole,
        CableVendorRole,
        // Power delivery
        HasPowerDeliveryRole,
        SourcePdosRole,
        MaxSourcePowerRole,
        // Charging
        HasChargingDiagRole,
        ChargingSummaryRole,
        ChargingDetailRole,
        ChargingIsWarningRole,
    };
    Q_ENUM(Roles)

    explicit DeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool showHubs() const { return m_showHubs; }
    void setShowHubs(bool v);
    bool showEmptyPorts() const { return m_showEmptyPorts; }
    void setShowEmptyPorts(bool v);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startMonitoring();

signals:
    void countChanged();
    void showHubsChanged();
    void showEmptyPortsChanged();

private:
    struct Device {
        QString headline, subtitle, icon, category, busPort;
        QStringList bullets;
        int status = 0, speed = 0, maxPower = 0, portNumber = -1, cableMaxWatts = 0, maxSourcePower = 0;
        QString vendorId, productId, speedLabel, usbVersion, dataRole, powerRole;
        QString cableSpeed, cableCurrent, cableType, cableVendor;
        bool isHub = false, isConnected = false, hasPowerDelivery = false;
        QJsonArray sourcePdos;
        bool hasChargingDiag = false, chargingIsWarning = false;
        QString chargingSummary, chargingDetail;
    };

    void loadSnapshot(const QString &json);
    void rebuildFiltered();

    UsbeehiveClient m_client;
    QList<Device> m_devices;
    QList<Device> m_filtered;
    bool m_showHubs = false;
    bool m_showEmptyPorts = false;
};

} // namespace WhatCable
