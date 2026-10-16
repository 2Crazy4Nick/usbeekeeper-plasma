#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QList>
#include <QVariantList>
#include "UsbeehiveClient.h"

namespace WhatCable {

class DeviceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool showHubs READ showHubs WRITE setShowHubs NOTIFY showHubsChanged)
    Q_PROPERTY(bool showEmptyPorts READ showEmptyPorts WRITE setShowEmptyPorts NOTIFY showEmptyPortsChanged)
    Q_PROPERTY(bool showTopologyTree READ showTopologyTree WRITE setShowTopologyTree NOTIFY showTopologyTreeChanged)

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
        // usbeehive details
        DeviceClassRole,
        DeviceSubclassRole,
        PrimaryDriverRole,
        VendorRole,
        ProductRole,
        SerialRole,
        DisplayNameRole,
        DisplayVendorRole,
        IdentitySourceRole,
        RawProductRole,
        RawManufacturerRole,
        LinkSummaryRole,
        DataRateCapableRole,
        DataRateNegotiatedRole,
        DataRateSummaryRole,
        DataRateDetailRole,
        DataRateVerdictRole,
        DataRateWarningRole,
        DetailsRole,
        UsbCapableSpeedRole,
        UsbCapableGenRole,
        UsbLinkVerdictRole,
        PortIdRole,
        PortPeerIdRole,
        PortPeerStateRole,
        PortConnectTypeRole,
        HubPortsTotalRole,
        HubPortsUsedRole,
        HubPowerCommittedRole,
        PowerSourceRole,
        ProductDbRole,
        KernelQuirksRole,
        UsbAltmodeSvidsRole,
        UsbAltmodeStateRole,
        HasAltmodeRole,
        AltmodeNameRole,
        AltmodeSummaryRole,
        IsCompanionHubGroupRole,
        Usb2ProductRole,
        Usb2VendorIdRole,
        Usb2ProductIdRole,
        Usb2SpeedRole,
        Usb2SpeedLabelRole,
        Usb2PortIdRole,
        Usb2PortsTotalRole,
        Usb2PortsUsedRole,
        Usb3ProductRole,
        Usb3VendorIdRole,
        Usb3ProductIdRole,
        Usb3SpeedRole,
        Usb3SpeedLabelRole,
        Usb3PortIdRole,
        Usb3PortsTotalRole,
        Usb3PortsUsedRole,
        ReportedDownstreamPortsRole,
        ContainerIdRole,
        TopologyIdRole,
        TopologyParentIdRole,
        TopologyDepthRole,
        TopologyPortRole,
        TopologyPathRole,
        TopologyAliasesRole,
        TopologyResolvedRole,
        TopologyIssueRole,
        TopologyHasChildrenRole,
        TopologyIsLastChildRole,
        TopologyAncestorLastFlagsRole,
        PhysicalNameRole,
        PhysicalVendorRole,
        PhysicalIdentitySourceRole,
        PhysicalConfidenceRole,
        PhysicalProfileIdRole,
        PhysicalIsCompositeRole,
        PhysicalTypeRole,
        PhysicalGroupIdRole,
        PhysicalRootIdRole,
        PhysicalMemberIdsRole,
        PhysicalRelationshipTypeRole,
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
    bool showTopologyTree() const { return m_showTopologyTree; }
    void setShowTopologyTree(bool v);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startMonitoring();

signals:
    void countChanged();
    void showHubsChanged();
    void showEmptyPortsChanged();
    void showTopologyTreeChanged();

private:
    struct Device {
        QString headline, subtitle, icon, category, busPort;
        QStringList bullets;
        int status = 0, speed = 0, maxPower = 0, portNumber = -1, cableMaxWatts = 0, maxSourcePower = 0;
        QString vendorId, productId, speedLabel, usbVersion, dataRole, powerRole;
        QString deviceClass, deviceSubclass, primaryDriver, vendor, manufacturer, product, serial;
        QString displayName, displayVendor, identitySource;
        QString linkSummary, dataRateSummary, dataRateDetail, dataRateVerdict;
        int dataRateCapable = 0, dataRateNegotiated = 0;
        bool dataRateWarning = false;
        QVariantList details;
        QString usbCapableSpeed, usbCapableGen, usbLinkVerdict;
        QString portId, portPeerId, portPeerState, portConnectType;
        QString hubPortsTotal, hubPortsUsed, hubPowerCommitted;
        QString powerSource, productDb, kernelQuirks, usbAltmodeSvids, usbAltmodeState;
        bool hasAltmode = false;
        QString altmodeName, altmodeSummary;
        bool isCompanionHubGroup = false;
        QString usb2Product, usb2VendorId, usb2ProductId, usb2SpeedLabel, usb2PortId;
        int usb2Speed = 0, usb2PortsTotal = 0, usb2PortsUsed = 0;
        QString usb3Product, usb3VendorId, usb3ProductId, usb3SpeedLabel, usb3PortId;
        int usb3Speed = 0, usb3PortsTotal = 0, usb3PortsUsed = 0;
        int reportedDownstreamPorts = 0;
        QString containerId;
        QString logicalPath, logicalParentPath, topologyId, topologyParentId, topologyPath;
        QString topologyIssue;
        QStringList logicalPaths, logicalParentPaths, logicalPortNumbers, topologyAliases;
        QVariantList topologyAncestorLastFlags;
        int logicalRootBus = -1, logicalPortNumber = -1, topologyDepth = 0;
        QString topologyPort;
        bool topologyResolved = false, topologyHasChildren = false, topologyIsLastChild = false;
        QString physicalName, physicalVendor, physicalIdentitySource, physicalConfidence;
        QString physicalProfileId, physicalType, physicalGroupId, physicalRootId, physicalRelationshipType;
        QStringList physicalMemberIds;
        bool physicalIsComposite = false;
        bool transportUsb2 = false, transportUsb3 = false;
        QString cableSpeed, cableCurrent, cableType, cableVendor;
        bool isHub = false, isConnected = false, hasPowerDelivery = false;
        QJsonArray sourcePdos;
        bool hasChargingDiag = false, chargingIsWarning = false;
        QString chargingSummary, chargingDetail;
    };

    void loadSnapshot(const QString &json);
    void rebuildFiltered();
    void resolveTopology();
    void resolvePhysicalProfiles();

    UsbeehiveClient m_client;
    QList<Device> m_devices;
    QList<Device> m_filtered;
    bool m_showHubs = false;
    bool m_showEmptyPorts = false;
    bool m_showTopologyTree = false;
};

} // namespace WhatCable
