#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace USBeeKeeper {

bool isCanonicalLogicalPort(const QString &port);

struct PhysicalVidPid {
    QString vendorId;
    QString productId;
};

struct PhysicalChildRequirement {
    PhysicalVidPid identity;
    QString logicalPort;
    QString interfaceClass;
};

struct PhysicalDeviceObservation {
    QString topologyId;
    QString topologyParentId;
    QString topologyPort;
    QString vendorId;
    QString productId;
    QString manufacturer;
    QString vendor;
    QString productDb;
    QString containerId;
    QString deviceClass;
    QString deviceSubclass;
    QStringList interfaceClasses;
    bool isCompanionHubGroup = false;
    QList<PhysicalVidPid> companionFunctions;
};

struct PhysicalDeviceProfile {
    QString profileId;
    QString physicalType;
    QString physicalName;
    QString physicalVendor;
    QString physicalIdentitySource = QStringLiteral("deterministic");
    QString physicalConfidence = QStringLiteral("none");
    QString requiredManufacturer;
    QString requiredProductDb;
    bool requireContainerId = false;
    bool requireCompanionGroup = false;
    QList<PhysicalVidPid> requiredParentFunctions;
    QList<PhysicalVidPid> requiredCompanionFunctions;
    bool requireExactCompanionFunctions = false;
    QList<PhysicalChildRequirement> requiredChildren;
    QString requiredChildRelationshipType = QStringLiteral("integrated_function");
    QString relationshipSource = QStringLiteral("profile");
    QString requiredInterfaceClass;
};

struct PhysicalDeviceMatch {
    QString physicalName;
    QString physicalVendor;
    QString physicalIdentitySource = QStringLiteral("none");
    QString physicalConfidence = QStringLiteral("none");
    QString physicalProfileId;
    QString physicalType;
    bool physicalIsComposite = false;
    QString physicalGroupId;
    QString physicalRootId;
    QStringList physicalMemberIds;
    QStringList physicalChildMemberIds;
    QString physicalChildRelationshipType;
    QString physicalRelationshipSource;
    QString physicalRelationshipEvidence;
    QString physicalRelationshipType;
    bool ambiguous = false;
};

class PhysicalDeviceProfileMatcher {
public:
    static bool parseProfile(const QByteArray &data, const QString &filename,
                             PhysicalDeviceProfile *profile);
    static QList<PhysicalDeviceProfile> rejectDuplicateProfileIds(
        const QList<PhysicalDeviceProfile> &profiles);
    static PhysicalDeviceMatch resolve(const PhysicalDeviceObservation &candidate,
                                       const QList<PhysicalDeviceObservation> &allDevices);
    static PhysicalDeviceMatch resolveWithProfiles(const PhysicalDeviceObservation &candidate,
                                                   const QList<PhysicalDeviceObservation> &allDevices,
                                                   const QList<PhysicalDeviceProfile> &profiles);

private:
    static QList<PhysicalDeviceProfile> profiles();
    static bool matches(const PhysicalDeviceProfile &profile,
                        const PhysicalDeviceObservation &candidate,
                        const QList<PhysicalDeviceObservation> &allDevices);
};

} // namespace USBeeKeeper
