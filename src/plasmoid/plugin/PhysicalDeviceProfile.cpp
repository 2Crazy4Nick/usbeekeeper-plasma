#include "PhysicalDeviceProfile.h"

namespace WhatCable {

namespace {

bool sameId(const PhysicalVidPid &left, const PhysicalVidPid &right)
{
    return left.vendorId.compare(right.vendorId, Qt::CaseInsensitive) == 0
        && left.productId.compare(right.productId, Qt::CaseInsensitive) == 0;
}

bool hasFunction(const QList<PhysicalVidPid> &functions, const PhysicalVidPid &required)
{
    for (const auto &function : functions) {
        if (sameId(function, required)) return true;
    }
    return false;
}

bool matchesChild(const PhysicalChildRequirement &required,
                  const PhysicalDeviceObservation &child)
{
    if (!sameId(required.identity, {child.vendorId, child.productId})) return false;
    if (!required.logicalPort.isEmpty() && child.topologyPort != required.logicalPort) return false;
    return required.interfaceClass.isEmpty()
        || child.interfaceClasses.contains(required.interfaceClass, Qt::CaseInsensitive);
}

} // namespace

QList<PhysicalDeviceProfile> PhysicalDeviceProfileMatcher::profiles()
{
    // This generic profile describes only a deterministic physical type.  It
    // deliberately makes no retail-product claim.
    return {{
        QStringLiteral("generic.companion-usb-hub"),
        QStringLiteral("hub"),
        {},
        {},
        {},
        {},
        true,
        true,
        {},
        {},
        {},
        {},
        {},
    }};
}

bool PhysicalDeviceProfileMatcher::matches(const PhysicalDeviceProfile &profile,
                                           const PhysicalDeviceObservation &candidate,
                                           const QList<PhysicalDeviceObservation> &allDevices)
{
    if (profile.requireCompanionGroup && !candidate.isCompanionHubGroup) return false;
    if (profile.requireContainerId && candidate.containerId.isEmpty()) return false;
    if (!profile.requiredManufacturer.isEmpty()
        && candidate.vendor.compare(profile.requiredManufacturer, Qt::CaseInsensitive) != 0)
        return false;
    if (!profile.requiredProductDb.isEmpty()
        && candidate.productDb.compare(profile.requiredProductDb, Qt::CaseInsensitive) != 0)
        return false;
    if (!profile.requiredInterfaceClass.isEmpty()
        && !candidate.interfaceClasses.contains(profile.requiredInterfaceClass, Qt::CaseInsensitive))
        return false;
    for (const auto &required : profile.requiredCompanionFunctions) {
        if (!hasFunction(candidate.companionFunctions, required)) return false;
    }

    for (const auto &requiredParent : profile.requiredParentFunctions) {
        bool parentMatched = false;
        for (const auto &parent : allDevices) {
            if (parent.topologyId == candidate.topologyParentId
                && sameId(requiredParent, {parent.vendorId, parent.productId})) {
                parentMatched = true;
                break;
            }
        }
        if (!parentMatched) return false;
    }

    for (const auto &requiredChild : profile.requiredChildren) {
        bool childMatched = false;
        for (const auto &child : allDevices) {
            if (child.topologyParentId == candidate.topologyId && matchesChild(requiredChild, child)) {
                childMatched = true;
                break;
            }
        }
        if (!childMatched) return false;
    }
    return true;
}

PhysicalDeviceMatch PhysicalDeviceProfileMatcher::resolve(
    const PhysicalDeviceObservation &candidate,
    const QList<PhysicalDeviceObservation> &allDevices)
{
    QList<PhysicalDeviceProfile> matchesFound;
    for (const auto &profile : profiles()) {
        if (matches(profile, candidate, allDevices)) matchesFound.append(profile);
    }

    PhysicalDeviceMatch result;
    if (matchesFound.size() > 1) {
        result.ambiguous = true;
        return result;
    }
    if (matchesFound.size() == 1) {
        const auto &profile = matchesFound.first();
        result.physicalName = profile.physicalName;
        result.physicalVendor = profile.physicalVendor;
        result.physicalIdentitySource = profile.physicalName.isEmpty()
            ? QStringLiteral("deterministic") : QStringLiteral("profile");
        result.physicalProfileId = profile.profileId;
        result.physicalType = profile.physicalType;
        result.physicalIsComposite = candidate.isCompanionHubGroup || !profile.requiredChildren.isEmpty();
        if (result.physicalIsComposite) {
            result.physicalGroupId = QStringLiteral("physical:%1").arg(candidate.topologyId);
            result.physicalRootId = candidate.topologyId;
            result.physicalMemberIds = {candidate.topologyId};
            result.physicalRelationshipType = QStringLiteral("root");
            result.physicalChildRelationshipType = profile.requiredChildRelationshipType;
            for (const auto &child : allDevices) {
                for (const auto &requiredChild : profile.requiredChildren) {
                    if (child.topologyParentId == candidate.topologyId && matchesChild(requiredChild, child)) {
                        result.physicalChildMemberIds.append(child.topologyId);
                        result.physicalMemberIds.append(child.topologyId);
                        break;
                    }
                }
            }
        }
    }
    return result;
}

} // namespace WhatCable
