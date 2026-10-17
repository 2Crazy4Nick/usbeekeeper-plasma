#include "PhysicalDeviceProfile.h"

#include <QCoreApplication>

using namespace USBeeKeeper;

static PhysicalDeviceObservation child(const QString &id, const QString &vid,
                                       const QString &pid, const QString &port)
{
    PhysicalDeviceObservation result;
    result.topologyId = id;
    result.topologyParentId = QStringLiteral("root");
    result.vendorId = vid;
    result.productId = pid;
    result.topologyPort = port;
    return result;
}

static PhysicalDeviceObservation viaRoot(const QString &usb2, const QString &usb3)
{
    PhysicalDeviceObservation result;
    result.topologyId = QStringLiteral("root");
    result.containerId = QStringLiteral("container");
    result.isCompanionHubGroup = true;
    result.companionFunctions = {{QStringLiteral("0x2109"), usb2},
                                 {QStringLiteral("0x2109"), usb3}};
    return result;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    auto vlRoot = viaRoot(QStringLiteral("0x2813"), QStringLiteral("0x0813"));
    auto audio = child(QStringLiteral("audio"), QStringLiteral("0x0d8c"), QStringLiteral("0x0014"), QStringLiteral("4"));
    auto vlMatch = PhysicalDeviceProfileMatcher::resolve(vlRoot, {vlRoot, audio});
    if (vlMatch.physicalProfileId != QStringLiteral("generic.vl813-integrated-audio")
        || vlMatch.physicalChildMemberIds != QStringList{QStringLiteral("audio")}
        || vlMatch.physicalChildRelationshipType != QStringLiteral("integrated_function")) {
        std::fprintf(stderr, "VL813 mismatch: id=%s children=%s relationship=%s\n",
                     qPrintable(vlMatch.physicalProfileId), qPrintable(vlMatch.physicalChildMemberIds.join(",")),
                     qPrintable(vlMatch.physicalChildRelationshipType));
        return 1;
    }

    auto via = viaRoot(QStringLiteral("0x2817"), QStringLiteral("0x0817"));
    auto lan = child(QStringLiteral("lan"), QStringLiteral("0x0bda"), QStringLiteral("0x8153"), QStringLiteral("2"));
    auto card = child(QStringLiteral("card"), QStringLiteral("0x058f"), QStringLiteral("0x8468"), QStringLiteral("3"));
    auto external = child(QStringLiteral("external"), QStringLiteral("0x1234"), QStringLiteral("0x5678"), QStringLiteral("1"));
    auto viaMatch = PhysicalDeviceProfileMatcher::resolve(via, {via, lan, card, external});
    if (viaMatch.physicalProfileId != QStringLiteral("generic.via2817-integrated-lan-cardreader")
        || viaMatch.physicalChildMemberIds != QStringList{QStringLiteral("lan"), QStringLiteral("card")}
        || viaMatch.physicalMemberIds.contains(QStringLiteral("external"))) {
        std::fprintf(stderr, "VIA mismatch: id=%s children=%s members=%s\n",
                     qPrintable(viaMatch.physicalProfileId), qPrintable(viaMatch.physicalChildMemberIds.join(",")),
                     qPrintable(viaMatch.physicalMemberIds.join(",")));
        return 2;
    }

    auto wrongPort = child(QStringLiteral("wrong"), QStringLiteral("0x0bda"), QStringLiteral("0x8153"), QStringLiteral("4"));
    auto wrongMatch = PhysicalDeviceProfileMatcher::resolve(via, {via, wrongPort, card});
    if (!wrongMatch.physicalChildMemberIds.isEmpty()) {
        return 3;
    }

    PhysicalDeviceProfile parentProfile;
    parentProfile.profileId = QStringLiteral("test.parent-functions");
    const PhysicalVidPid primary{QStringLiteral("0x2109"), QStringLiteral("0x2813")};
    const PhysicalVidPid secondary{QStringLiteral("0x2109"), QStringLiteral("0x0813")};
    parentProfile.requiredParentFunctions = {primary};

    PhysicalDeviceObservation parent;
    parent.topologyId = QStringLiteral("root");
    parent.vendorId = primary.vendorId;
    parent.productId = primary.productId;
    auto candidate = child(QStringLiteral("candidate"), QStringLiteral("0x1234"),
                            QStringLiteral("0x5678"), QStringLiteral("1"));
    auto parentMatches = [&](const QList<PhysicalDeviceObservation> &devices) {
        return PhysicalDeviceProfileMatcher::resolveWithProfiles(candidate, devices, {parentProfile})
            .physicalProfileId == parentProfile.profileId;
    };
    if (!parentMatches({parent, candidate})) return 4;
    parentProfile.requiredParentFunctions = {secondary};
    if (parentMatches({parent, candidate})) return 5;
    // Companion observations must not be used for an ordinary parent.
    parent.companionFunctions = {secondary};
    if (parentMatches({parent, candidate})) return 6;

    parent.isCompanionHubGroup = true;
    parentProfile.requiredParentFunctions = {primary, secondary};
    if (!parentMatches({parent, candidate})) {
        std::fprintf(stderr, "Grouped parent must match primary and companion identities\n");
        return 7;
    }
    parent.companionFunctions.clear();
    if (parentMatches({parent, candidate})) return 8;

    auto sibling = child(QStringLiteral("sibling"), secondary.vendorId, secondary.productId,
                          QStringLiteral("2"));
    auto descendant = sibling;
    descendant.topologyId = QStringLiteral("descendant");
    descendant.topologyParentId = candidate.topologyId;
    PhysicalDeviceObservation unrelated = sibling;
    unrelated.topologyId = QStringLiteral("unrelated");
    unrelated.topologyParentId.clear();
    if (parentMatches({parent, candidate, sibling, descendant, unrelated})) return 9;

    return 0;
}
