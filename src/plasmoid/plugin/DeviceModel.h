#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QVariantList>
#include "UsbeehiveClient.h"

namespace USBeeKeeper {

bool topologySiblingLess(int leftPort, bool leftKnown, int rightPort, bool rightKnown);
bool isTrustworthyUsbSerial(const QString &serial);
QString selectCompanionSerial(const QString &usb2Serial, const QString &usb3Serial);

class DeviceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool showHubs READ showHubs WRITE setShowHubs NOTIFY showHubsChanged)
    Q_PROPERTY(bool showEmptyPorts READ showEmptyPorts WRITE setShowEmptyPorts NOTIFY showEmptyPortsChanged)
    Q_PROPERTY(bool showTopologyTree READ showTopologyTree WRITE setShowTopologyTree NOTIFY showTopologyTreeChanged)
    Q_PROPERTY(QString userAliasesJson READ userAliasesJson WRITE setUserAliasesJson NOTIFY userAliasesChanged)
    Q_PROPERTY(bool backendAvailable READ backendAvailable NOTIFY backendAvailableChanged)
    Q_PROPERTY(bool hasSnapshot READ hasSnapshot NOTIFY hasSnapshotChanged)
    Q_PROPERTY(QString snapshotError READ snapshotError NOTIFY snapshotErrorChanged)

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
        PresentationTreeParentIdRole,
        PresentationTreeDepthRole,
        PresentationTreePortRole,
        PresentationTreeHasChildrenRole,
        PresentationTreeIsLastChildRole,
        PresentationTreeAncestorLastFlagsRole,
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
        PhysicalIntegratedMembersRole,
        PhysicalRelationshipTypeRole,
        PhysicalRelationshipSourceRole,
        PhysicalRelationshipEvidenceRole,
        PhysicalInstanceKeyRole,
        PhysicalInstanceKeySourceRole,
        PhysicalAliasIdentityAmbiguousRole,
        DeviceInformationRole,
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
    QString userAliasesJson() const { return m_userAliasesJson; }
    void setUserAliasesJson(const QString &value);
    bool backendAvailable() const { return m_backendAvailable; }
    bool hasSnapshot() const { return m_hasSnapshot; }
    QString snapshotError() const { return m_snapshotError; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startMonitoring();

signals:
    void countChanged();
    void showHubsChanged();
    void showEmptyPortsChanged();
    void showTopologyTreeChanged();
    void userAliasesChanged();
    void backendAvailableChanged();
    void hasSnapshotChanged();
    void snapshotErrorChanged();

private:
    struct Device {
        QString headline, subtitle, icon, category, busPort;
        QStringList bullets;
        int status = 0, speed = 0, maxPower = 0, portNumber = -1, cableMaxWatts = 0, maxSourcePower = 0;
        QString vendorId, productId, speedLabel, usbVersion, dataRole, powerRole;
        QString deviceClass, deviceSubclass, primaryDriver, vendor, manufacturer, product, serial;
        QStringList interfaceClasses;
        QString displayName, displayVendor, identitySource;
        QString linkSummary, dataRateSummary, dataRateDetail, dataRateVerdict;
        int dataRateCapable = 0, dataRateNegotiated = 0;
        bool dataRateWarning = false;
        QVariantList details, deviceInformation;
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
        QString presentationTreeParentId, presentationTreePort;
        QVariantList presentationTreeAncestorLastFlags;
        int presentationTreeDepth = 0;
        bool presentationTreeHasChildren = false, presentationTreeIsLastChild = false;
        QString physicalName, physicalVendor, physicalIdentitySource, physicalConfidence;
        QString physicalProfileId, physicalType, physicalGroupId, physicalRootId, physicalRelationshipType;
        QString physicalRelationshipSource, physicalRelationshipEvidence;
        QStringList physicalMemberIds;
        QVariantList physicalIntegratedMembers;
        bool physicalIsComposite = false;
        QString physicalInstanceKey, physicalInstanceKeySource;
        bool physicalAliasIdentityAmbiguous = false;
        QString profilePhysicalName, profilePhysicalVendor, profilePhysicalIdentitySource, profilePhysicalConfidence;
        bool transportUsb2 = false, transportUsb3 = false;
        QString cableSpeed, cableCurrent, cableType, cableVendor;
        bool isHub = false, isConnected = false, hasPowerDelivery = false;
        QJsonArray sourcePdos;
        bool hasChargingDiag = false, chargingIsWarning = false;
        QString chargingSummary, chargingDetail;
    };

    void loadSnapshot(const QString &json);
    static QVariantMap informationSection(const Device &device, const QString &title = {});
    void clearSnapshot(const QString &error, bool available);
    void reportSnapshotError(const QString &error, bool available);
    void rebuildFiltered();
    void resolveTopology();
    void resolvePresentationTree();
    void resolvePhysicalProfiles();
    void resolvePhysicalInstances();

    UsbeehiveClient m_client;
    QList<Device> m_devices;
    QList<Device> m_filtered;
    QList<Device> m_visible;
    bool m_showHubs = false;
    bool m_showEmptyPorts = false;
    bool m_showTopologyTree = false;
    QString m_userAliasesJson = QStringLiteral("{}");
    QHash<QString, QString> m_userAliases;
    bool m_backendAvailable = false;
    bool m_hasSnapshot = false;
    QString m_snapshotError;
};

} // namespace USBeeKeeper
