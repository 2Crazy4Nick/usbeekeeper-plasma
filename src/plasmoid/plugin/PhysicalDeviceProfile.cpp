#include "PhysicalDeviceProfile.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QDebug>
#include <algorithm>

namespace WhatCable {

namespace {

bool sameId(const PhysicalVidPid &left, const PhysicalVidPid &right);

const QRegularExpression kHexId(QStringLiteral("^[0-9a-f]{4}$"));
const QRegularExpression kProfileId(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));

bool objectKeysKnown(const QJsonObject &object, const QSet<QString> &allowed,
                     const QString &filename)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!allowed.contains(it.key())) {
            qWarning().noquote() << filename << "unknown profile field" << it.key();
            return false;
        }
    }
    return true;
}

bool parseId(const QJsonValue &value, QString *result, const QString &filename,
             const QString &field)
{
    if (!value.isString() || !kHexId.match(value.toString()).hasMatch()) {
        qWarning().noquote() << filename << "invalid" << field << "(expected four lowercase hex digits)";
        return false;
    }
    *result = QStringLiteral("0x") + value.toString();
    return true;
}

bool parseVidPid(const QJsonObject &object, PhysicalVidPid *result,
                 const QString &filename)
{
    if (!objectKeysKnown(object, {QStringLiteral("vid"), QStringLiteral("pid")}, filename)
        || !object.contains(QStringLiteral("vid")) || !object.contains(QStringLiteral("pid"))) {
        qWarning().noquote() << filename << "VID/PID matcher requires vid and pid";
        return false;
    }
    return parseId(object.value(QStringLiteral("vid")), &result->vendorId, filename, QStringLiteral("vid"))
        && parseId(object.value(QStringLiteral("pid")), &result->productId, filename, QStringLiteral("pid"));
}

bool parseArrayOfIds(const QJsonValue &value, QList<PhysicalVidPid> *result,
                     const QString &filename, const QString &field)
{
    if (!value.isArray()) {
        qWarning().noquote() << filename << field << "must be an array";
        return false;
    }
    for (const auto &entry : value.toArray()) {
        PhysicalVidPid id;
        if (!entry.isObject() || !parseVidPid(entry.toObject(), &id, filename)) return false;
        if (std::any_of(result->cbegin(), result->cend(), [&id](const auto &existing) {
                return sameId(existing, id);
            })) {
            qWarning().noquote() << filename << "duplicate" << field << "VID/PID";
            return false;
        }
        result->append(id);
    }
    return true;
}

bool parseProfile(const QByteArray &data, const QString &filename,
                  PhysicalDeviceProfile *profile)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning().noquote() << filename << "invalid JSON:" << error.errorString();
        return false;
    }
    const auto object = document.object();
    if (!objectKeysKnown(object, {QStringLiteral("schemaVersion"), QStringLiteral("id"),
                                  QStringLiteral("description"), QStringLiteral("physicalType"),
                                  QStringLiteral("physicalName"), QStringLiteral("physicalVendor"),
                                  QStringLiteral("physicalIdentitySource"), QStringLiteral("physicalConfidence"),
                                  QStringLiteral("relationshipSource"), QStringLiteral("root"),
                                  QStringLiteral("members")}, filename)) return false;
    if (object.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) {
        qWarning().noquote() << filename << "unsupported schemaVersion" << object.value(QStringLiteral("schemaVersion"));
        return false;
    }
    const auto id = object.value(QStringLiteral("id")).toString();
    if (id.isEmpty() || !kProfileId.match(id).hasMatch()) {
        qWarning().noquote() << filename << "invalid profile id" << id;
        return false;
    }

    const auto root = object.value(QStringLiteral("root")).toObject();
    if (root.isEmpty() || !objectKeysKnown(root, {QStringLiteral("requireCompanionGroup"),
        QStringLiteral("requireContainerId"), QStringLiteral("exactCompanionFunctions"),
        QStringLiteral("companionFunctions"), QStringLiteral("parentFunctions"),
        QStringLiteral("manufacturer"), QStringLiteral("productDb"),
        QStringLiteral("interfaceClass")}, filename)) return false;

    PhysicalDeviceProfile parsed;
    parsed.profileId = id;
    parsed.physicalType = object.value(QStringLiteral("physicalType")).toString();
    parsed.physicalName = object.value(QStringLiteral("physicalName")).toString();
    parsed.physicalVendor = object.value(QStringLiteral("physicalVendor")).toString();
    parsed.physicalIdentitySource = object.value(QStringLiteral("physicalIdentitySource")).toString(QStringLiteral("deterministic"));
    parsed.physicalConfidence = object.value(QStringLiteral("physicalConfidence")).toString(QStringLiteral("none"));
    parsed.relationshipSource = object.value(QStringLiteral("relationshipSource")).toString(QStringLiteral("profile"));
    parsed.requireCompanionGroup = root.value(QStringLiteral("requireCompanionGroup")).toBool(false);
    parsed.requireContainerId = root.value(QStringLiteral("requireContainerId")).toBool(false);
    parsed.requireExactCompanionFunctions = root.value(QStringLiteral("exactCompanionFunctions")).toBool(false);
    parsed.requiredManufacturer = root.value(QStringLiteral("manufacturer")).toString();
    parsed.requiredProductDb = root.value(QStringLiteral("productDb")).toString();
    parsed.requiredInterfaceClass = root.value(QStringLiteral("interfaceClass")).toString();
    if (root.contains(QStringLiteral("companionFunctions"))
        && !parseArrayOfIds(root.value(QStringLiteral("companionFunctions")),
                            &parsed.requiredCompanionFunctions, filename, QStringLiteral("companionFunctions"))) return false;
    if (parsed.requireExactCompanionFunctions && parsed.requiredCompanionFunctions.size() != 2) {
        qWarning().noquote() << filename << "exactCompanionFunctions requires exactly two companion functions";
        return false;
    }
    if (root.contains(QStringLiteral("parentFunctions"))
        && !parseArrayOfIds(root.value(QStringLiteral("parentFunctions")),
                            &parsed.requiredParentFunctions, filename, QStringLiteral("parentFunctions"))) return false;
    if (!parsed.requireCompanionGroup && parsed.requiredCompanionFunctions.isEmpty()
        && parsed.requiredParentFunctions.isEmpty() && parsed.requiredManufacturer.isEmpty()
        && parsed.requiredProductDb.isEmpty() && parsed.requiredInterfaceClass.isEmpty()
        && !parsed.requireContainerId) {
        qWarning().noquote() << filename << "root matcher is empty";
        return false;
    }

    const auto members = object.value(QStringLiteral("members"));
    if (!members.isArray()) {
        qWarning().noquote() << filename << "members must be an array";
        return false;
    }
    QSet<QString> memberKeys;
    for (const auto &entry : members.toArray()) {
        if (!entry.isObject() || !objectKeysKnown(entry.toObject(),
            {QStringLiteral("match"), QStringLiteral("relationship"), QStringLiteral("required")}, filename)) return false;
        const auto member = entry.toObject();
        if (!member.value(QStringLiteral("required")).toBool(false)) {
            qWarning().noquote() << filename << "members must be required";
            return false;
        }
        if (member.value(QStringLiteral("relationship")).toString() != QStringLiteral("integrated_function")) {
            qWarning().noquote() << filename << "unsupported relationship"
                                  << member.value(QStringLiteral("relationship"));
            return false;
        }
        const auto match = member.value(QStringLiteral("match")).toObject();
        if (match.isEmpty() || !objectKeysKnown(match, {QStringLiteral("vid"), QStringLiteral("pid"),
            QStringLiteral("logicalPort"), QStringLiteral("interfaceClass")}, filename)) return false;
        PhysicalChildRequirement child;
        if (!parseVidPid(QJsonObject{{QStringLiteral("vid"), match.value(QStringLiteral("vid"))},
                                    {QStringLiteral("pid"), match.value(QStringLiteral("pid"))}},
                         &child.identity, filename)) return false;
        child.logicalPort = match.value(QStringLiteral("logicalPort")).toString();
        bool portOk = false;
        if (!child.logicalPort.isEmpty()) {
            const int port = child.logicalPort.toInt(&portOk);
            if (!portOk || port <= 0) {
                qWarning().noquote() << filename << "invalid logicalPort" << child.logicalPort;
                return false;
            }
        }
        child.interfaceClass = match.value(QStringLiteral("interfaceClass")).toString();
        const auto key = child.identity.vendorId + QLatin1Char(':') + child.identity.productId
            + QLatin1Char(':') + child.logicalPort;
        if (memberKeys.contains(key)) {
            qWarning().noquote() << filename << "duplicate member definition";
            return false;
        }
        memberKeys.insert(key);
        parsed.requiredChildren.append(child);
    }
    *profile = parsed;
    return true;
}

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
    static const auto loaded = [] {
        QList<PhysicalDeviceProfile> result;
        QSet<QString> ids;
        const QDir directory(QStringLiteral(":/org/kde/whatcable/profiles"));
        for (const auto &filename : directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
            if (filename == QStringLiteral("schema.json")) continue;
            QFile file(directory.filePath(filename));
            if (!file.open(QIODevice::ReadOnly)) {
                qWarning().noquote() << filename << "could not be opened";
                continue;
            }
            PhysicalDeviceProfile profile;
            if (!parseProfile(file.readAll(), filename, &profile)) continue;
            if (ids.contains(profile.profileId)) {
                qWarning().noquote() << filename << "duplicate profile id" << profile.profileId;
                continue;
            }
            ids.insert(profile.profileId);
            result.append(profile);
        }
        return result;
    }();
    return loaded;
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
