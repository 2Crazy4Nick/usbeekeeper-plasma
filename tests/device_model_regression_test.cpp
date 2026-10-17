#include "DeviceModel.h"
#include "PhysicalDeviceProfile.h"
#include "UsbeehiveClient.h"

#include <algorithm>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <vector>

using namespace USBeeKeeper;

static QJsonObject usbDevice(const QString &headline, const QString &vendor,
                             const QString &product, const QString &portId,
                             const QString &peerId, const QString &speed,
                             bool hub)
{
    QJsonObject object{
        {QStringLiteral("category"), hub ? QStringLiteral("hub") : QStringLiteral("usb")},
        {QStringLiteral("headline"), headline},
        {QStringLiteral("vendor"), vendor},
        {QStringLiteral("product"), product},
        {QStringLiteral("vendorId"), vendor},
        {QStringLiteral("productId"), product},
        {QStringLiteral("status"), QStringLiteral("connected")},
        {QStringLiteral("properties"), QJsonArray{
            QJsonArray{QStringLiteral("port.id"), portId},
            QJsonArray{QStringLiteral("port.peer_id"), peerId},
            QJsonArray{QStringLiteral("usb_bos_container_id"), QStringLiteral("container")},
            QJsonArray{QStringLiteral("transport.usb2"), speed == QStringLiteral("usb2") ? QStringLiteral("true") : QStringLiteral("false")},
            QJsonArray{QStringLiteral("transport.usb3"), speed == QStringLiteral("usb3") ? QStringLiteral("true") : QStringLiteral("false")},
        }},
        {QStringLiteral("usb"), QJsonObject{
            {QStringLiteral("isHub"), hub},
            {QStringLiteral("speed"), speed == QStringLiteral("usb3") ? 5000 : 480},
        }},
    };
    return object;
}

static QVariant role(const DeviceModel &model, int row, const QByteArray &name)
{
    const auto names = model.roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        if (it.value() == name) return model.data(model.index(row, 0), it.key());
    return {};
}

static QJsonObject identityDevice(const QString &serial, const QString &container,
                                  const QString &portId)
{
    QJsonArray properties;
    if (!container.isEmpty())
        properties.append(QJsonArray{QStringLiteral("usb_bos_container_id"), container});
    if (!portId.isEmpty())
        properties.append(QJsonArray{QStringLiteral("port.id"), portId});
    return QJsonObject{
        {QStringLiteral("category"), QStringLiteral("usb")},
        {QStringLiteral("headline"), QStringLiteral("Identity device")},
        {QStringLiteral("vendorId"), QStringLiteral("0x1234")},
        {QStringLiteral("productId"), QStringLiteral("0x5678")},
        {QStringLiteral("status"), QStringLiteral("connected")},
        {QStringLiteral("properties"), properties},
        {QStringLiteral("usb"), QJsonObject{{QStringLiteral("serial"), serial}}},
    };
}

static QJsonObject profileJson(const QJsonValue &logicalPort)
{
    return QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("id"), QStringLiteral("generic.test")},
        {QStringLiteral("root"), QJsonObject{{QStringLiteral("requireCompanionGroup"), true}}},
        {QStringLiteral("members"), QJsonArray{QJsonObject{
            {QStringLiteral("match"), QJsonObject{
                {QStringLiteral("vid"), QStringLiteral("0bda")},
                {QStringLiteral("pid"), QStringLiteral("8153")},
                {QStringLiteral("logicalPort"), logicalPort},
            }},
            {QStringLiteral("relationship"), QStringLiteral("integrated_function")},
            {QStringLiteral("required"), true},
        }}},
    };
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    DeviceModel model;
    auto *client = model.findChild<UsbeehiveClient *>();
    if (!client) return 1;
    if (model.hasSnapshot()) return 34;
    model.setShowHubs(true);
    model.setShowHubs(false);
    if (model.hasSnapshot()) return 35;

    RequestSequencer requests;
    const auto refresh = requests.nextRefresh();
    const auto oldSnapshot = requests.nextSnapshot();
    const auto currentSnapshot = requests.nextSnapshot();
    if (requests.isCurrentSnapshot(oldSnapshot)
        || !requests.isCurrentSnapshot(currentSnapshot)
        || !requests.isCurrentRefresh(refresh)) return 2;
    const auto currentRefresh = requests.nextRefresh();
    if (requests.isCurrentRefresh(refresh) || !requests.isCurrentRefresh(currentRefresh)
        || !requests.isCurrentSnapshot(currentSnapshot)) return 3;
    requests.ownerChanged();
    if (requests.isCurrentSnapshot(currentSnapshot) || requests.isCurrentRefresh(currentRefresh)) return 4;

    QJsonArray snapshot{
        usbDevice(QStringLiteral("VL813 USB2"), QStringLiteral("0x2109"), QStringLiteral("0x2813"),
                  QStringLiteral("usb1-port10"), QStringLiteral("usb2-port5"), QStringLiteral("usb2"), true),
        usbDevice(QStringLiteral("VL813 USB3"), QStringLiteral("0x2109"), QStringLiteral("0x0813"),
                  QStringLiteral("usb2-port5"), QStringLiteral("usb1-port10"), QStringLiteral("usb3"), true),
        usbDevice(QStringLiteral("C-Media"), QStringLiteral("0x0d8c"), QStringLiteral("0x0014"),
                  QStringLiteral("1-10-port4"), QString(), QStringLiteral("usb2"), false),
    };
    emit client->snapshotReady(QJsonDocument(snapshot).toJson(QJsonDocument::Compact));
    model.setShowHubs(true);
    if (model.rowCount() != 2) return 5;
    const int visibleWithHub = model.rowCount();
    const auto relationshipWithHub = role(model, 1, "physicalRelationshipType");
    const auto profileWithHub = role(model, 1, "physicalProfileId");
    const auto keyWithHub = role(model, 1, "physicalInstanceKey");

    model.setShowHubs(false);
    if (model.rowCount() != 1) return 6;
    if (role(model, 0, "physicalRelationshipType") != relationshipWithHub
        || role(model, 0, "physicalProfileId") != profileWithHub
        || role(model, 0, "physicalInstanceKey") != keyWithHub
        || role(model, 0, "topologyResolved") != true)
        return 7;
    model.setShowHubs(true);
    if (model.rowCount() != visibleWithHub) return 8;

    std::vector<std::pair<int, bool>> ports{{1, true}, {0, false}, {2, true}, {0, false}};
    std::stable_sort(ports.begin(), ports.end(), [](const auto &left, const auto &right) {
        return topologySiblingLess(left.first, left.second, right.first, right.second);
    });
    if (ports != std::vector<std::pair<int, bool>>{{1, true}, {2, true}, {0, false}, {0, false}})
        return 9;

    PhysicalDeviceProfile constrained;
    constrained.profileId = QStringLiteral("test.constraints");
    constrained.requiredManufacturer = QStringLiteral("Raw Manufacturer");
    constrained.requiredInterfaceClass = QStringLiteral("Audio");
    PhysicalDeviceObservation observation;
    observation.topologyId = QStringLiteral("constrained");
    observation.vendor = QStringLiteral("Different Vendor");
    observation.manufacturer = QStringLiteral("Raw Manufacturer");
    observation.interfaceClasses = {QStringLiteral("Audio")};
    const auto constrainedMatch = PhysicalDeviceProfileMatcher::resolveWithProfiles(
        observation, {observation}, {constrained});
    if (constrainedMatch.physicalProfileId != constrained.profileId) return 10;
    observation.manufacturer = QStringLiteral("Different Manufacturer");
    if (!PhysicalDeviceProfileMatcher::resolveWithProfiles(observation, {observation}, {constrained})
             .physicalProfileId.isEmpty()) return 11;
    observation.manufacturer = QStringLiteral("Raw Manufacturer");
    observation.interfaceClasses.clear();
    observation.deviceSubclass = QStringLiteral("Audio");
    if (!PhysicalDeviceProfileMatcher::resolveWithProfiles(observation, {observation}, {constrained})
             .physicalProfileId.isEmpty()) return 12;

    QJsonObject noPath{{QStringLiteral("category"), QStringLiteral("usb")},
                       {QStringLiteral("headline"), QStringLiteral("No path")},
                       {QStringLiteral("vendorId"), QStringLiteral("0x1234")},
                       {QStringLiteral("productId"), QStringLiteral("0x5678")},
                       {QStringLiteral("status"), QStringLiteral("connected")},
                       {QStringLiteral("usb"), QJsonObject{}}};
    emit client->snapshotReady(QJsonDocument(QJsonArray{noPath}).toJson(QJsonDocument::Compact));
    if (!role(model, 0, "physicalInstanceKey").toString().isEmpty()
        || role(model, 0, "physicalInstanceKeySource").toString() != QStringLiteral("none")) return 13;

    emit client->snapshotReady(QJsonDocument(QJsonArray{identityDevice(
        QStringLiteral("SERIAL-42"), QStringLiteral("container-id"), QStringLiteral("usb1-port3"))})
                                   .toJson(QJsonDocument::Compact));
    if (role(model, 0, "physicalInstanceKeySource") != QStringLiteral("serial")) return 14;
    emit client->snapshotReady(QJsonDocument(QJsonArray{identityDevice(
        QStringLiteral("00000000"), QStringLiteral("container-id"), QStringLiteral("usb1-port3"))})
                                   .toJson(QJsonDocument::Compact));
    if (role(model, 0, "physicalInstanceKeySource") != QStringLiteral("container_id")) return 15;
    emit client->snapshotReady(QJsonDocument(QJsonArray{identityDevice(
        QStringLiteral("00000000"), QString(), QStringLiteral("usb1-port3"))})
                                   .toJson(QJsonDocument::Compact));
    if (role(model, 0, "physicalInstanceKeySource") != QStringLiteral("topology_fallback")) return 16;
    emit client->snapshotReady(QJsonDocument(QJsonArray{identityDevice(
        QStringLiteral("00000000"), QString(), QString())}).toJson(QJsonDocument::Compact));
    if (!role(model, 0, "physicalInstanceKey").toString().isEmpty()) return 17;

    if (selectCompanionSerial(QStringLiteral("SERIAL"), QStringLiteral("00000000"))
            != QStringLiteral("SERIAL")
        || selectCompanionSerial(QStringLiteral("00000000"), QStringLiteral("SERIAL"))
            != QStringLiteral("SERIAL")
        || selectCompanionSerial(QStringLiteral("SERIAL"), QStringLiteral("SERIAL"))
            != QStringLiteral("SERIAL")
        || !selectCompanionSerial(QStringLiteral("SERIAL-A"), QStringLiteral("SERIAL-B")).isEmpty()
        || !selectCompanionSerial(QStringLiteral("00000000"), QStringLiteral("unknown")).isEmpty()) return 18;

    const auto knownGood = QJsonDocument(QJsonArray{identityDevice(
        QStringLiteral("SERIAL-42"), QString(), QStringLiteral("usb1-port3"))})
                               .toJson(QJsonDocument::Compact);
    emit client->snapshotReady(knownGood);
    if (model.rowCount() != 1 || !model.hasSnapshot()) return 19;
    emit client->snapshotReady(QByteArrayLiteral("not json"));
    if (model.rowCount() != 1 || !model.backendAvailable() || !model.hasSnapshot()
        || model.snapshotError().isEmpty()) return 20;
    for (const auto &bad : {QByteArrayLiteral("{}"), QByteArrayLiteral("[1]"),
                            QByteArrayLiteral("[null]"), QByteArrayLiteral("{\"devices\":[null]}"),
                            QByteArrayLiteral("[{\"headline\":\"valid\"},1]")}) {
        emit client->snapshotReady(bad);
        if (model.rowCount() != 1 || model.snapshotError().isEmpty()) return 21;
    }
    emit client->snapshotReady(QByteArrayLiteral("[]"));
    if (model.rowCount() != 0 || !model.backendAvailable() || !model.hasSnapshot()
        || !model.snapshotError().isEmpty()) return 22;

    emit client->snapshotFailed(QStringLiteral("Snapshot failed with no devices"));
    if (model.rowCount() != 0 || !model.hasSnapshot() || model.snapshotError().isEmpty()) return 36;

    emit client->snapshotReady(knownGood);
    RequestSequencer failedRefreshRequests;
    const auto snapshotBeforeRefresh = failedRefreshRequests.nextSnapshot();
    const auto failedRefresh = failedRefreshRequests.nextRefresh();
    if (!failedRefreshRequests.acceptRefreshFailure(failedRefresh)) return 39;
    emit client->snapshotFailed(QStringLiteral("Refresh failed"));
    if (failedRefreshRequests.isCurrentSnapshot(snapshotBeforeRefresh)) {
        emit client->snapshotReady(knownGood);
        return 40;
    }
    if (model.rowCount() != 1 || !model.backendAvailable()
        || model.snapshotError() != QStringLiteral("Refresh failed")) return 23;

    const auto snapshotAfterFailure = failedRefreshRequests.nextSnapshot();
    if (!failedRefreshRequests.isCurrentSnapshot(snapshotAfterFailure)
        || !failedRefreshRequests.isCurrentRefresh(failedRefresh)) return 41;
    emit client->snapshotReady(knownGood);
    if (model.rowCount() != 1 || !model.snapshotError().isEmpty()) return 42;

    failedRefreshRequests.nextRefresh();
    if (failedRefreshRequests.acceptRefreshFailure(failedRefresh)
        || !failedRefreshRequests.isCurrentSnapshot(snapshotAfterFailure)) return 43;
    const auto signalSnapshot = failedRefreshRequests.nextSnapshot();
    const auto refreshBeforeSignal = failedRefreshRequests.nextRefresh();
    const auto snapshotDuringRefresh = failedRefreshRequests.nextSnapshot();
    if (!failedRefreshRequests.acceptRefreshFailure(refreshBeforeSignal)
        || failedRefreshRequests.isCurrentSnapshot(signalSnapshot)
        || failedRefreshRequests.isCurrentSnapshot(snapshotDuringRefresh)) return 44;
    failedRefreshRequests.ownerChanged();
    const auto snapshotNewOwner = failedRefreshRequests.nextSnapshot();
    if (failedRefreshRequests.acceptRefreshFailure(refreshBeforeSignal)
        || !failedRefreshRequests.isCurrentSnapshot(snapshotNewOwner)) return 45;

    int countChanges = 0;
    QObject::connect(&model, &DeviceModel::countChanged, [&countChanges] { ++countChanges; });
    countChanges = 0;
    emit client->serviceAvailableChanged(false);
    emit client->serviceAvailableChanged(false);
    if (model.rowCount() != 0 || model.backendAvailable() || model.hasSnapshot()
        || countChanges != 1) return 24;
    emit client->serviceAvailableChanged(true);
    if (!model.backendAvailable() || model.hasSnapshot()) return 37;
    emit client->snapshotReady(QByteArrayLiteral("[null]"));
    if (model.hasSnapshot() || model.snapshotError().isEmpty()) return 38;

    emit client->snapshotReady(knownGood);
    if (!QMetaObject::invokeMethod(client, "onServiceOwnerChanged", Qt::DirectConnection,
                                   Q_ARG(QString, QStringLiteral("org.usbeehive.Devices")),
                                   Q_ARG(QString, QStringLiteral(":1.100")),
                                   Q_ARG(QString, QStringLiteral(":1.101")))) return 25;
    emit client->snapshotFailed(QStringLiteral("new owner snapshot failed"));
    if (model.rowCount() != 0 || !model.backendAvailable()
        || model.snapshotError() != QStringLiteral("new owner snapshot failed")) return 26;

    for (const auto &validPort : {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("255")}) {
        PhysicalDeviceProfile parsed;
        const auto json = QJsonDocument(profileJson(validPort)).toJson(QJsonDocument::Compact);
        if (!PhysicalDeviceProfileMatcher::parseProfile(json, QStringLiteral("test.json"), &parsed)) return 27;
    }
    const QList<QJsonValue> invalidPorts{
        QString(), QStringLiteral("0"), QStringLiteral("01"), QStringLiteral("+1"),
        QStringLiteral("-1"), QStringLiteral("256"), QJsonValue::Null, 1,
    };
    for (const auto &invalidPort : invalidPorts) {
        PhysicalDeviceProfile parsed;
        const auto json = QJsonDocument(profileJson(invalidPort)).toJson(QJsonDocument::Compact);
        if (PhysicalDeviceProfileMatcher::parseProfile(json, QStringLiteral("test.json"), &parsed)) return 28;
    }

    for (const auto &suffix : {QStringLiteral("\n"), QStringLiteral("\r\n"),
                                QStringLiteral(" "), QStringLiteral("\t"), QStringLiteral("!")}) {
        for (const auto &field : {QStringLiteral("id"), QStringLiteral("vid"),
                                  QStringLiteral("pid"), QStringLiteral("logicalPort")}) {
            auto profile = profileJson(QStringLiteral("2"));
            if (field == QLatin1String("id")) {
                profile[field] = profile.value(field).toString() + suffix;
            } else {
                auto members = profile.value(QStringLiteral("members")).toArray();
                auto member = members.first().toObject();
                auto match = member.value(QStringLiteral("match")).toObject();
                match[field] = match.value(field).toString() + suffix;
                member[QStringLiteral("match")] = match;
                members[0] = member;
                profile[QStringLiteral("members")] = members;
            }
            PhysicalDeviceProfile parsed;
            if (PhysicalDeviceProfileMatcher::parseProfile(
                    QJsonDocument(profile).toJson(QJsonDocument::Compact),
                    QStringLiteral("suffix-test.json"), &parsed)) {
                std::fprintf(stderr, "Accepted invalid suffix for %s\n", qPrintable(field));
                return 46;
            }
        }
    }

    auto duplicateParent = profileJson(QStringLiteral("1"));
    auto root = duplicateParent.value(QStringLiteral("root")).toObject();
    const auto function = QJsonObject{{QStringLiteral("vid"), QStringLiteral("2109")},
                                      {QStringLiteral("pid"), QStringLiteral("2817")}};
    root.insert(QStringLiteral("parentFunctions"), QJsonArray{function, function});
    duplicateParent.insert(QStringLiteral("root"), root);
    PhysicalDeviceProfile parsed;
    if (PhysicalDeviceProfileMatcher::parseProfile(
            QJsonDocument(duplicateParent).toJson(QJsonDocument::Compact),
            QStringLiteral("test.json"), &parsed)) return 29;

    auto badRelationship = profileJson(QStringLiteral("1"));
    auto members = badRelationship.value(QStringLiteral("members")).toArray();
    auto member = members.first().toObject();
    member.insert(QStringLiteral("relationship"), QJsonArray{});
    members[0] = member;
    badRelationship.insert(QStringLiteral("members"), members);
    if (PhysicalDeviceProfileMatcher::parseProfile(
            QJsonDocument(badRelationship).toJson(QJsonDocument::Compact),
            QStringLiteral("test.json"), &parsed)) return 30;

    QList<QJsonObject> wrongTypeProfiles;
    auto wrongInterface = profileJson(QStringLiteral("1"));
    auto wrongInterfaceMembers = wrongInterface.value(QStringLiteral("members")).toArray();
    auto wrongInterfaceMember = wrongInterfaceMembers.first().toObject();
    auto wrongInterfaceMatch = wrongInterfaceMember.value(QStringLiteral("match")).toObject();
    wrongInterfaceMatch.insert(QStringLiteral("interfaceClass"), QJsonArray{});
    wrongInterfaceMember.insert(QStringLiteral("match"), wrongInterfaceMatch);
    wrongInterfaceMembers[0] = wrongInterfaceMember;
    wrongInterface.insert(QStringLiteral("members"), wrongInterfaceMembers);
    wrongTypeProfiles.append(wrongInterface);

    auto wrongManufacturer = profileJson(QStringLiteral("1"));
    auto wrongManufacturerRoot = wrongManufacturer.value(QStringLiteral("root")).toObject();
    wrongManufacturerRoot.insert(QStringLiteral("manufacturer"), QJsonArray{});
    wrongManufacturer.insert(QStringLiteral("root"), wrongManufacturerRoot);
    wrongTypeProfiles.append(wrongManufacturer);

    auto wrongPhysicalName = profileJson(QStringLiteral("1"));
    wrongPhysicalName.insert(QStringLiteral("physicalName"), QJsonObject{});
    wrongTypeProfiles.append(wrongPhysicalName);

    auto wrongSchemaVersion = profileJson(QStringLiteral("1"));
    wrongSchemaVersion.insert(QStringLiteral("schemaVersion"), true);
    wrongTypeProfiles.append(wrongSchemaVersion);

    for (const auto &wrongTypeProfile : wrongTypeProfiles) {
        if (PhysicalDeviceProfileMatcher::parseProfile(
                QJsonDocument(wrongTypeProfile).toJson(QJsonDocument::Compact),
                QStringLiteral("test.json"), &parsed)) return 31;
    }

    PhysicalDeviceProfile duplicateA;
    duplicateA.profileId = QStringLiteral("duplicate");
    PhysicalDeviceProfile duplicateB = duplicateA;
    PhysicalDeviceProfile unique;
    unique.profileId = QStringLiteral("unique");
    const auto firstOrder = PhysicalDeviceProfileMatcher::rejectDuplicateProfileIds(
        {duplicateA, unique, duplicateB});
    const auto reverseOrder = PhysicalDeviceProfileMatcher::rejectDuplicateProfileIds(
        {duplicateB, unique, duplicateA});
    if (firstOrder.size() != 1 || reverseOrder.size() != 1
        || firstOrder.first().profileId != QStringLiteral("unique")
        || reverseOrder.first().profileId != QStringLiteral("unique")) return 32;

    if (isTrustworthyUsbSerial(QStringLiteral("unknown"))
        || isTrustworthyUsbSerial(QStringLiteral("00000000"))
        || isTrustworthyUsbSerial(QStringLiteral("0x00000000"))
        || isTrustworthyUsbSerial(QStringLiteral("0X00000000"))
        || isTrustworthyUsbSerial(QStringLiteral("0000-0000"))
        || isTrustworthyUsbSerial(QStringLiteral("ff:ff:ff:ff"))
        || isTrustworthyUsbSerial(QStringLiteral("0xffffffff"))
        || !isTrustworthyUsbSerial(QStringLiteral("7"))
        || !isTrustworthyUsbSerial(QStringLiteral("000x0000"))
        || !isTrustworthyUsbSerial(QStringLiteral("ABC0x1234"))
        || !isTrustworthyUsbSerial(QStringLiteral("00001234"))
        || !isTrustworthyUsbSerial(QStringLiteral("ABCD0000"))
        || !isTrustworthyUsbSerial(QStringLiteral("00FF1234"))
        || !isTrustworthyUsbSerial(QStringLiteral("1234FFFF"))) return 33;
    return 0;
}
