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
    PhysicalDeviceProfile generic;
    generic.profileId = QStringLiteral("generic.companion-usb-hub");
    generic.physicalType = QStringLiteral("hub");
    generic.requireContainerId = true;
    generic.requireCompanionGroup = true;

    PhysicalDeviceProfile vl813;
    vl813.profileId = QStringLiteral("generic.vl813-integrated-audio");
    vl813.physicalType = QStringLiteral("hub_audio");
    vl813.physicalIdentitySource = QStringLiteral("profile");
    vl813.physicalConfidence = QStringLiteral("exact");
    vl813.requireContainerId = true;
    vl813.requireCompanionGroup = true;
    vl813.requireExactCompanionFunctions = true;
    vl813.requiredCompanionFunctions = {
        {QStringLiteral("0x2109"), QStringLiteral("0x2813")},
        {QStringLiteral("0x2109"), QStringLiteral("0x0813")},
    };
    vl813.requiredChildren = {{
        {QStringLiteral("0x0d8c"), QStringLiteral("0x0014")},
        QStringLiteral("4"),
        {},
    }};

    PhysicalDeviceProfile via2817;
    via2817.profileId = QStringLiteral("generic.via2817-integrated-lan-cardreader");
    via2817.physicalType = QStringLiteral("hub_composite");
    via2817.physicalIdentitySource = QStringLiteral("profile");
    via2817.physicalConfidence = QStringLiteral("exact");
    via2817.requireContainerId = true;
    via2817.requireCompanionGroup = true;
    via2817.requireExactCompanionFunctions = true;
    via2817.requiredCompanionFunctions = {
        {QStringLiteral("0x2109"), QStringLiteral("0x2817")},
        {QStringLiteral("0x2109"), QStringLiteral("0x0817")},
    };
    via2817.requiredChildren = {
        {{QStringLiteral("0x0bda"), QStringLiteral("0x8153")}, QStringLiteral("2"), {}},
        {{QStringLiteral("0x058f"), QStringLiteral("0x8468")}, QStringLiteral("3"), {}},
    };

    return {generic, vl813, via2817};
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
    if (profile.requireExactCompanionFunctions
        && candidate.companionFunctions.size() != profile.requiredCompanionFunctions.size())
        return false;

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
    int bestSpecificity = -1;
    for (const auto &profile : matchesFound) {
        int specificity = 0;
        specificity += profile.requireCompanionGroup ? 1 : 0;
        specificity += profile.requireContainerId ? 1 : 0;
        specificity += profile.requireExactCompanionFunctions ? 1 : 0;
        specificity += profile.requiredCompanionFunctions.size();
        specificity += profile.requiredParentFunctions.size();
        specificity += profile.requiredChildren.size();
        specificity += !profile.requiredManufacturer.isEmpty() ? 1 : 0;
        specificity += !profile.requiredProductDb.isEmpty() ? 1 : 0;
        specificity += !profile.requiredInterfaceClass.isEmpty() ? 1 : 0;
        bestSpecificity = qMax(bestSpecificity, specificity);
    }
    QList<PhysicalDeviceProfile> bestMatches;
    for (const auto &profile : matchesFound) {
        int specificity = 0;
        specificity += profile.requireCompanionGroup ? 1 : 0;
        specificity += profile.requireContainerId ? 1 : 0;
        specificity += profile.requireExactCompanionFunctions ? 1 : 0;
        specificity += profile.requiredCompanionFunctions.size();
        specificity += profile.requiredParentFunctions.size();
        specificity += profile.requiredChildren.size();
        specificity += !profile.requiredManufacturer.isEmpty() ? 1 : 0;
        specificity += !profile.requiredProductDb.isEmpty() ? 1 : 0;
        specificity += !profile.requiredInterfaceClass.isEmpty() ? 1 : 0;
        if (specificity == bestSpecificity) bestMatches.append(profile);
    }
    if (bestMatches.size() > 1) {
        result.ambiguous = true;
        return result;
    }
    if (bestMatches.size() == 1) {
        const auto &profile = bestMatches.first();
        result.physicalName = profile.physicalName;
        result.physicalVendor = profile.physicalVendor;
        result.physicalIdentitySource = profile.physicalIdentitySource;
        // "exact" describes an exact structural-profile match, never a
        // claim that a retail product/model has been identified.
        result.physicalConfidence = profile.physicalConfidence;
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
        result.physicalRelationshipSource = profile.relationshipSource;
        if (!result.physicalChildRelationshipType.isEmpty()) {
            QStringList evidence;
            for (const auto &requiredChild : profile.requiredChildren) {
                evidence.append(QStringLiteral("Matched %1:%2 on logical downstream port %3")
                                    .arg(requiredChild.identity.vendorId,
                                         requiredChild.identity.productId,
                                         requiredChild.logicalPort));
            }
            result.physicalRelationshipEvidence = evidence.join(QStringLiteral("; "));
        }
    }
    return result;
}

} // namespace WhatCable
