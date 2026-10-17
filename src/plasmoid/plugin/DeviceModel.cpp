#include "DeviceModel.h"
#include "PhysicalDeviceProfile.h"

#include <algorithm>
#include <functional>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <QRegularExpression>
#include <utility>

namespace {
QJsonValue value(const QJsonObject &object, std::initializer_list<const char *> names)
{
    for (const auto name : names) {
        const auto it = object.find(QLatin1String(name));
        if (it != object.end()) return *it;
    }
    return {};
}

QString stringValue(const QJsonObject &object, std::initializer_list<const char *> names)
{
    const auto v = value(object, names);
    return v.isString() ? v.toString() : QString();
}

int intValue(const QJsonObject &object, std::initializer_list<const char *> names)
{
    const auto v = value(object, names);
    return v.isDouble() ? v.toInt() : 0;
}

QString idValue(const QJsonObject &object, std::initializer_list<const char *> names)
{
    const auto v = value(object, names);
    if (v.isString()) return v.toString();
    return v.isDouble() ? QStringLiteral("0x%1").arg(v.toInt(), 4, 16, QChar('0')) : QString();
}

QString propertyValue(const QJsonObject &object, const QString &key)
{
    for (const auto &entry : value(object, {"properties"}).toArray()) {
        const auto pair = entry.toArray();
        if (pair.size() == 2 && pair.at(0).toString() == key)
            return pair.at(1).toVariant().toString();
    }
    return {};
}

QString firstProperty(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    for (const auto key : keys) {
        const auto result = propertyValue(object, QLatin1String(key));
        if (!result.isEmpty()) return result;
    }
    return {};
}

QString rateLabel(int mbps)
{
    if (mbps <= 0) return {};
    if (mbps >= 1000 && mbps % 1000 == 0)
        return QStringLiteral("%1 Gbps").arg(mbps / 1000);
    return QStringLiteral("%1 Mbps").arg(mbps);
}

QString verdictLabel(const QString &verdict)
{
    if (verdict == QLatin1String("AtCapability")) return QStringLiteral("At capability");
    if (verdict == QLatin1String("BelowCapability")) return QStringLiteral("Below capability");
    return verdict;
}

QString altmodeStateLabel(const QString &state)
{
    if (state == QLatin1String("NotAttempted")) return QStringLiteral("Not attempted");
    return state;
}

bool usefulIdentity(const QString &value)
{
    const auto name = value.trimmed();
    return !name.isEmpty() && name.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) != 0;
}

bool genericHubProduct(const QString &value)
{
    const auto name = value.trimmed().toLower();
    if (name.isEmpty()) return true;
    if (name == QLatin1String("hub") || name == QLatin1String("usb hub")) return true;
    return name.contains(QStringLiteral("hub"))
        && (name.contains(QStringLiteral("usb")) || name.contains(QStringLiteral("port")));
}

bool genericIdentityName(const QString &value)
{
    const auto name = value.trimmed().toLower();
    if (name.isEmpty() || name == QLatin1String("unknown") || name == QLatin1String("miscellaneous")
        || name == QLatin1String("usb device") || name == QLatin1String("usb audio")
        || name == QLatin1String("usb audio device") || name == QLatin1String("usb audio codec"))
        return true;
    return genericHubProduct(name);
}

QString neutralManufacturer(QString value)
{
    value.replace(QStringLiteral(" Electronics Inc."), QString());
    value.replace(QStringLiteral(" Inc."), QString());
    return value.trimmed();
}

struct ParsedPort {
    QString path;
    QString parent;
    int rootBus = -1;
    int port = -1;
    bool valid = false;
};

ParsedPort parsePortId(const QString &portId)
{
    const auto id = portId.trimmed();
    static const QRegularExpression rootPattern(QStringLiteral(R"(^usb(\d+)-port(\d+)$)"));
    static const QRegularExpression childPattern(QStringLiteral(R"(^((\d+)-(\d+(?:\.\d+)*))-port(\d+)$)"));

    auto match = rootPattern.match(id);
    if (match.hasMatch()) {
        ParsedPort result;
        result.rootBus = match.captured(1).toInt();
        result.port = match.captured(2).toInt();
        result.path = QStringLiteral("%1-%2").arg(result.rootBus).arg(result.port);
        result.valid = result.rootBus > 0 && result.port > 0;
        return result;
    }

    match = childPattern.match(id);
    if (!match.hasMatch()) return {};
    ParsedPort result;
    result.parent = match.captured(1);
    result.rootBus = match.captured(2).toInt();
    result.port = match.captured(4).toInt();
    result.path = QStringLiteral("%1.%2").arg(result.parent).arg(result.port);
    result.valid = result.rootBus > 0 && result.port > 0;
    return result;
}

QString resolveDisplayName(const QString &product, const QString &productDb,
                           const QString &manufacturer, const QString &headline,
                           QString *source)
{
    if (usefulIdentity(product) && !genericIdentityName(product)) {
        if (source) *source = QStringLiteral("descriptor");
        return product;
    }
    if (product.compare(QStringLiteral("USB Audio Device"), Qt::CaseInsensitive) == 0
        && usefulIdentity(manufacturer)) {
        if (source) *source = QStringLiteral("fallback");
        return QStringLiteral("%1 USB Audio Adapter").arg(neutralManufacturer(manufacturer));
    }
    if (usefulIdentity(productDb) && !genericIdentityName(productDb)) {
        if (source) *source = QStringLiteral("product_db");
        return productDb;
    }
    if (usefulIdentity(product)) {
        if (source) *source = QStringLiteral("descriptor");
        return product;
    }
    if (usefulIdentity(headline)) {
        if (source) *source = QStringLiteral("fallback");
        return headline;
    }
    if (source) *source = QStringLiteral("fallback");
    return QStringLiteral("USB device");
}

}

namespace USBeeKeeper {

bool topologySiblingLess(int leftPort, bool leftKnown, int rightPort, bool rightKnown)
{
    if (leftKnown != rightKnown) return leftKnown;
    if (!leftKnown) return false;
    return leftPort < rightPort;
}

bool isTrustworthyUsbSerial(const QString &serial)
{
    const auto trimmed = serial.trimmed();
    if (trimmed.isEmpty()) return false;
    const auto lower = trimmed.toLower();
    if (QSet<QString>{QStringLiteral("unknown"), QStringLiteral("none"), QStringLiteral("null"),
                      QStringLiteral("n/a")}.contains(lower))
        return false;

    QString compact = lower;
    compact.remove(QRegularExpression(QStringLiteral("[\\s:_-]")));
    if (compact.startsWith(QStringLiteral("0x")))
        compact.remove(0, 2);
    if (compact.size() >= 4 && QRegularExpression(QStringLiteral("^0+$")).match(compact).hasMatch())
        return false;
    if (compact.size() >= 8 && QRegularExpression(QStringLiteral("^f+$")).match(compact).hasMatch())
        return false;
    return true;
}

QString selectCompanionSerial(const QString &usb2Serial, const QString &usb3Serial)
{
    const auto usb2 = usb2Serial.trimmed();
    const auto usb3 = usb3Serial.trimmed();
    const bool usb2Trusted = isTrustworthyUsbSerial(usb2);
    const bool usb3Trusted = isTrustworthyUsbSerial(usb3);
    if (usb2Trusted && usb3Trusted)
        return usb2 == usb3 ? usb2 : QString();
    if (usb2Trusted) return usb2;
    if (usb3Trusted) return usb3;
    return {};
}

DeviceModel::DeviceModel(QObject *parent)
    : QAbstractListModel(parent), m_client(this)
{
    connect(&m_client, &UsbeehiveClient::snapshotReady, this, &DeviceModel::loadSnapshot);
    connect(&m_client, &UsbeehiveClient::serviceAvailableChanged, this,
            [this](bool available) {
                if (!available) clearSnapshot(QStringLiteral("usbeehived is unavailable"), false);
                else if (m_backendAvailable != available) {
                    m_backendAvailable = available;
                    emit backendAvailableChanged();
                }
            });
    connect(&m_client, &UsbeehiveClient::snapshotFailed, this,
            [this](const QString &error) { reportSnapshotError(error, true); });
}

void DeviceModel::loadSnapshot(const QString &json)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    QJsonArray devices;
    bool validSnapshot = document.isArray();
    if (document.isArray()) devices = document.array();
    else if (document.isObject()) {
        const auto root = document.object();
        auto devicesValue = value(root, {"devices"});
        if (!devicesValue.isArray())
            devicesValue = value(root, {"deviceSummaries", "device_summaries"});
        validSnapshot = devicesValue.isArray();
        if (validSnapshot) devices = devicesValue.toArray();
    }
    if (validSnapshot) {
        for (const auto &entry : devices) {
            if (!entry.isObject() || entry.toObject().isEmpty()) {
                validSnapshot = false;
                break;
            }
        }
    }
    if (!validSnapshot) {
        reportSnapshotError(parseError.error != QJsonParseError::NoError
                                ? parseError.errorString()
                                : QStringLiteral("SnapshotJson has an invalid device array"), true);
        return;
    }

    if (m_snapshotError.isEmpty() == false) {
        m_snapshotError.clear();
        emit snapshotErrorChanged();
    }
    if (!m_backendAvailable) {
        m_backendAvailable = true;
        emit backendAvailableChanged();
    }

    m_devices.clear();
    for (const auto &entry : devices) {
        const auto object = entry.toObject();
        if (object.isEmpty()) continue;
        Device device;
        device.headline = stringValue(object, {"headline"});
        device.subtitle = stringValue(object, {"subtitle"});
        device.icon = stringValue(object, {"icon", "iconName", "icon_name"});
        device.category = stringValue(object, {"category"}).toLower();
        device.deviceClass = stringValue(object, {"deviceClass", "device_class"});
        device.deviceSubclass = stringValue(object, {"deviceSubclass", "device_subclass"});
        device.primaryDriver = stringValue(object, {"primaryDriver", "primary_driver"});
        device.vendor = stringValue(object, {"vendor"});
        device.product = stringValue(object, {"product"});
        device.vendorId = idValue(object, {"vendorId", "vendor_id"});
        device.productId = idValue(object, {"productId", "product_id"});
        device.usbVersion = stringValue(object, {"usbVersion", "usb_version"});
        device.busPort = stringValue(object, {"busPort", "bus_port"});
        const auto usb = value(object, {"usb", "usb_device"}).toObject();
        device.manufacturer = stringValue(usb, {"manufacturer"});
        if (device.manufacturer.isEmpty())
            device.manufacturer = stringValue(object, {"manufacturer"});
        auto interfaces = value(usb, {"interfaces"}).toArray();
        if (interfaces.isEmpty()) interfaces = value(object, {"interfaces"}).toArray();
        for (const auto &interfaceValue : interfaces) {
            const auto interfaceObject = interfaceValue.toObject();
            const auto interfaceClass = interfaceObject.value(QStringLiteral("class"));
            if (interfaceClass.isString()) device.interfaceClasses.append(interfaceClass.toString());
            else if (interfaceClass.isDouble()) device.interfaceClasses.append(QString::number(interfaceClass.toInt()));
        }
        device.speed = intValue(usb, {"speed", "linkSpeedMbps", "link_speed_mbps"});
        if (device.speed == 0)
            device.speed = intValue(object, {"linkSpeedMbps", "link_speed_mbps"});
        device.speedLabel = stringValue(usb, {"speedLabel", "speed_label"});
        if (device.speedLabel.isEmpty() && device.speed > 0)
            device.speedLabel = QStringLiteral("%1 Mbps").arg(device.speed);
        device.maxPower = intValue(usb, {"maxPowerMA", "max_power_ma"});
        device.serial = stringValue(usb, {"serial"});
        device.portNumber = intValue(object, {"portNumber", "port_number"});
        device.isHub = device.category == QLatin1String("hub")
            || value(usb, {"isHub", "is_hub"}).toBool()
            || stringValue(object, {"deviceClass", "device_class"}).contains(QStringLiteral("hub"), Qt::CaseInsensitive);

        const auto status = stringValue(object, {"status"}).toLower();
        device.status = status == QLatin1String("empty") ? 0 : 1;
        device.isConnected = status != QLatin1String("empty") && status != QLatin1String("disconnected");
        if (device.category == QLatin1String("usbdevice")) device.category = QStringLiteral("usb");
        if (device.category.isEmpty()) device.category = device.portNumber >= 0 ? QStringLiteral("typec") : QStringLiteral("usb");

        const auto dataRate = value(object, {"dataRate", "data_rate"}).toObject();
        device.dataRateCapable = intValue(dataRate, {"capable_mbps"});
        device.dataRateNegotiated = intValue(dataRate, {"negotiated_mbps"});
        device.dataRateSummary = stringValue(dataRate, {"summary"});
        device.dataRateDetail = stringValue(dataRate, {"detail"});
        device.dataRateVerdict = stringValue(dataRate, {"verdict"});
        device.dataRateWarning = value(dataRate, {"is_warning"}).toBool();
        const auto power = value(object, {"power"}).toObject();
        if (device.powerRole.isEmpty())
            device.powerRole = stringValue(power, {"powerRole", "power_role"});
        const auto compactSpeed = rateLabel(device.speed);
        const auto compactVersion = device.usbVersion.isEmpty()
            ? QString() : QStringLiteral("USB %1").arg(device.usbVersion);
        if (!compactVersion.isEmpty() && !compactSpeed.isEmpty())
            device.linkSummary = compactVersion + QStringLiteral(" · ") + compactSpeed;
        else if (!compactSpeed.isEmpty())
            device.linkSummary = compactSpeed;
        else if (!compactVersion.isEmpty())
            device.linkSummary = compactVersion;
        if (device.dataRateCapable > 0 && device.dataRateCapable != device.speed)
            device.linkSummary += QStringLiteral(" · capable ") + rateLabel(device.dataRateCapable);

        device.dataRole = firstProperty(object, {"port.data_role", "data_role", "dataRole"});
        const auto propertyPowerRole = firstProperty(object, {"port.power_role", "power_role", "powerRole"});
        if (!propertyPowerRole.isEmpty()) device.powerRole = propertyPowerRole;
        device.usbCapableSpeed = firstProperty(object, {"usb_capable_speed_mbps", "usb_capable_speed"});
        device.usbCapableGen = firstProperty(object, {"usb_capable_gen"});
        device.usbLinkVerdict = firstProperty(object, {"usb_link_verdict"});
        device.containerId = firstProperty(object, {"usb_bos_container_id"});
        device.transportUsb2 = firstProperty(object, {"transport.usb2"}) == QLatin1String("true");
        device.transportUsb3 = firstProperty(object, {"transport.usb3"}) == QLatin1String("true");
        device.portId = firstProperty(object, {"port.id"});
        const auto parsedPort = parsePortId(device.portId);
        if (parsedPort.valid) {
            device.logicalPath = parsedPort.path;
            device.logicalParentPath = parsedPort.parent;
            device.logicalRootBus = parsedPort.rootBus;
            device.logicalPortNumber = parsedPort.port;
            device.logicalPaths = {parsedPort.path};
            device.logicalParentPaths = {parsedPort.parent};
            device.logicalPortNumbers = {QString::number(parsedPort.port)};
        }
        device.portPeerId = firstProperty(object, {"port.peer_id"});
        device.portPeerState = firstProperty(object, {"port.peer_state"});
        device.portConnectType = firstProperty(object, {"port.connect_type"});
        device.hubPortsTotal = firstProperty(object, {"hub.ports_total"});
        device.hubPortsUsed = firstProperty(object, {"hub.ports_used"});
        device.hubPowerCommitted = firstProperty(object, {"hub.power_committed_ma"});
        device.powerSource = firstProperty(object, {"power.source"});
        device.productDb = firstProperty(object, {"product_db"});
        device.displayName = resolveDisplayName(device.product, device.productDb, device.manufacturer,
                                                device.headline, &device.identitySource);
        device.displayVendor = usefulIdentity(device.manufacturer) ? device.manufacturer : device.vendor;
        device.kernelQuirks = firstProperty(object, {"kernel.quirks"});
        device.usbAltmodeSvids = firstProperty(object, {"usb_altmode_svids"});
        device.usbAltmodeState = firstProperty(object, {"usb_altmode_state"});
        device.hasAltmode = !device.usbAltmodeSvids.isEmpty() || !device.usbAltmodeState.isEmpty();
        if (device.usbAltmodeSvids.contains(QStringLiteral("ff01"), Qt::CaseInsensitive))
            device.altmodeName = QStringLiteral("DisplayPort Alt Mode");
        else if (!device.usbAltmodeSvids.isEmpty())
            device.altmodeName = QStringLiteral("Alternate Mode");
        const auto altmodeState = altmodeStateLabel(device.usbAltmodeState);
        if (!device.altmodeName.isEmpty() && !altmodeState.isEmpty())
            device.altmodeSummary = device.altmodeName + QStringLiteral(" · ") + altmodeState;
        else if (!device.altmodeName.isEmpty())
            device.altmodeSummary = device.altmodeName;
        else
            device.altmodeSummary = altmodeState;
        if (device.hasAltmode) {
            if (!device.dataRateWarning && device.dataRateCapable > 0 && device.dataRateCapable != device.speed)
                device.linkSummary.remove(QStringLiteral(" · capable ") + rateLabel(device.dataRateCapable));
            if (!device.altmodeSummary.isEmpty())
                device.linkSummary += (device.linkSummary.isEmpty() ? QString() : QStringLiteral(" · ")) + device.altmodeSummary;
        }
        device.cableSpeed = firstProperty(object, {"cable.speed", "cable_speed"});
        device.cableCurrent = firstProperty(object, {"cable.current", "cable_current"});
        device.cableType = firstProperty(object, {"cable.type", "cable_type"});
        device.cableVendor = firstProperty(object, {"cable.vendor", "cable_vendor"});
        device.cableMaxWatts = firstProperty(object, {"cable.max_watts", "cable_max_watts"}).toInt();

        const auto diagnostic = value(object, {"chargingDiagnostic", "charging_diagnostic"}).toObject();
        device.hasChargingDiag = !diagnostic.isEmpty();
        device.chargingSummary = stringValue(diagnostic, {"summary"});
        device.chargingDetail = stringValue(diagnostic, {"detail"});
        device.chargingIsWarning = value(diagnostic, {"isWarning", "is_warning"}).toBool();

        device.sourcePdos = value(object, {"pdoData", "pdo_data", "pdoList", "pdo_list"}).toArray();
        device.hasPowerDelivery = !device.sourcePdos.isEmpty();
        for (const auto &pdo : device.sourcePdos)
            device.maxSourcePower = qMax(device.maxSourcePower, intValue(pdo.toObject(), {"powerMw", "power_mw"}) / 1000);

        auto addDetail = [&device](const QString &label, const QString &detail) {
            if (detail.isEmpty() || detail.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0)
                return;
            device.details.append(QVariantMap{{QStringLiteral("label"), label},
                                              {QStringLiteral("value"), detail}});
        };
        addDetail(QStringLiteral("Device class"), device.deviceClass);
        addDetail(QStringLiteral("Device subclass"), device.deviceSubclass);
        addDetail(QStringLiteral("Serial"), device.serial);
        addDetail(QStringLiteral("Raw product"), device.product);
        addDetail(QStringLiteral("Manufacturer"), device.manufacturer);
        addDetail(QStringLiteral("USB version"), device.usbVersion);
        addDetail(QStringLiteral("Link speed"), device.speedLabel);
        const auto capableSpeed = device.dataRateCapable > 0
            ? rateLabel(device.dataRateCapable) : device.usbCapableSpeed;
        if (!device.hasAltmode || device.dataRateWarning)
            addDetail(QStringLiteral("Capable speed"), capableSpeed);
        addDetail(QStringLiteral("Capable generation"), device.usbCapableGen);
        if (!device.hasAltmode || device.dataRateWarning)
            addDetail(QStringLiteral("Link status"), verdictLabel(device.dataRateVerdict));
        if (!device.vendorId.isEmpty() && !device.productId.isEmpty())
            addDetail(QStringLiteral("VID:PID"), device.vendorId + QStringLiteral(":") + device.productId);
        addDetail(QStringLiteral("Driver"), device.primaryDriver);
        if (device.maxPower > 0)
            addDetail(QStringLiteral("Max USB current"), QStringLiteral("%1 mA").arg(device.maxPower));
        addDetail(QStringLiteral("Power source"), device.powerSource);
        addDetail(QStringLiteral("USB database"), device.productDb);
        addDetail(QStringLiteral("Port"), device.portId);
        addDetail(QStringLiteral("Companion port"), device.portPeerId);
        addDetail(QStringLiteral("Companion state"), device.portPeerState);
        addDetail(QStringLiteral("Connector"), device.portConnectType);
        if (!device.hubPortsTotal.isEmpty() && !device.hubPortsUsed.isEmpty())
            addDetail(QStringLiteral("Hub ports"), device.hubPortsUsed + QStringLiteral(" / ") + device.hubPortsTotal + QStringLiteral(" used"));
        if (!device.hubPowerCommitted.isEmpty())
            addDetail(QStringLiteral("Hub committed power"), device.hubPowerCommitted + QStringLiteral(" mA"));
        addDetail(QStringLiteral("Kernel quirks"), device.kernelQuirks);
        addDetail(QStringLiteral("USB link"), device.speedLabel);
        addDetail(QStringLiteral("Alternate mode"), device.altmodeName);
        addDetail(QStringLiteral("Alternate mode state"), altmodeStateLabel(device.usbAltmodeState));
        device.bullets.clear();
        m_devices.append(device);
    }
    rebuildFiltered();
    if (!m_hasSnapshot) {
        m_hasSnapshot = true;
        emit hasSnapshotChanged();
    }
}

void DeviceModel::clearSnapshot(const QString &error, bool available)
{
    const bool availabilityChanged = m_backendAvailable != available;
    const bool errorChanged = m_snapshotError != error;
    const bool hadSnapshot = m_hasSnapshot;
    m_backendAvailable = available;
    m_hasSnapshot = false;
    m_snapshotError = error;
    const bool modelChanged = !m_devices.isEmpty() || !m_filtered.isEmpty() || !m_visible.isEmpty();
    if (modelChanged) {
        beginResetModel();
        m_devices.clear();
        m_filtered.clear();
        m_visible.clear();
        endResetModel();
        emit countChanged();
    }
    if (availabilityChanged) emit backendAvailableChanged();
    if (hadSnapshot) emit hasSnapshotChanged();
    if (errorChanged) emit snapshotErrorChanged();
}

void DeviceModel::reportSnapshotError(const QString &error, bool available)
{
    const bool availabilityChanged = m_backendAvailable != available;
    const bool errorChanged = m_snapshotError != error;
    m_backendAvailable = available;
    m_snapshotError = error;
    if (availabilityChanged) emit backendAvailableChanged();
    if (errorChanged) emit snapshotErrorChanged();
}

void DeviceModel::rebuildFiltered()
{
    beginResetModel();
    m_filtered.clear();
    m_visible.clear();
    QList<bool> consumed(m_devices.size(), false);

    auto transportFlags = [](const Device &device) {
        const bool usb2 = device.transportUsb2
            || (!device.transportUsb3 && device.speed > 0 && device.speed <= 480);
        const bool usb3 = device.transportUsb3
            || (!device.transportUsb2 && device.speed >= 5000);
        return std::pair<bool, bool>(usb2, usb3);
    };
    auto companion = [&transportFlags](const Device &a, const Device &b) {
        if (!a.isHub || !b.isHub || a.containerId.isEmpty() || a.containerId != b.containerId)
            return false;
        if (a.portPeerId.isEmpty() || b.portId.isEmpty() || a.portPeerId != b.portId
            || b.portPeerId.isEmpty() || a.portId.isEmpty() || b.portPeerId != a.portId)
            return false;
        const auto aTransport = transportFlags(a);
        const auto bTransport = transportFlags(b);
        return (aTransport.first && bTransport.second) || (aTransport.second && bTransport.first);
    };
    auto addDetail = [](Device &device, const QString &label, const QString &detail) {
        if (!detail.isEmpty())
            device.details.append(QVariantMap{{QStringLiteral("label"), label},
                                              {QStringLiteral("value"), detail}});
    };
    auto groupedHubName = [](const Device &usb2, const Device &usb3) {
        if (usefulIdentity(usb2.product) && usb2.product.compare(usb3.product, Qt::CaseInsensitive) == 0
            && !genericHubProduct(usb2.product))
            return usb2.product;

        for (const auto *productDb : {&usb2.productDb, &usb3.productDb}) {
            if (usefulIdentity(*productDb) && !genericHubProduct(*productDb))
                return *productDb;
        }

        if (usefulIdentity(usb2.product) && !genericHubProduct(usb2.product)) return usb2.product;
        if (usefulIdentity(usb3.product) && !genericHubProduct(usb3.product)) return usb3.product;
        if (usefulIdentity(usb2.product) && usefulIdentity(usb3.product)
            && usb2.product.compare(usb3.product, Qt::CaseInsensitive) != 0)
            return QStringLiteral("%1 / %2").arg(usb2.product, usb3.product);

        return usb2.hubPortsTotal.toInt() > 0
            ? QStringLiteral("%1-Port USB Hub").arg(usb2.hubPortsTotal.toInt())
            : QStringLiteral("USB Hub");
    };
    auto groupedVendor = [](const Device &device) {
        const auto separator = device.subtitle.indexOf(QStringLiteral(" · "));
        if (separator > 0) {
            const auto name = device.subtitle.left(separator).trimmed();
            if (usefulIdentity(name)) return name;
        }
        return device.vendor;
    };
    auto makeGroup = [&transportFlags, &addDetail, &groupedHubName, &groupedVendor](const Device &a, const Device &b) {
        const auto aTransport = transportFlags(a);
        const Device &usb2 = aTransport.first ? a : b;
        const Device &usb3 = aTransport.first ? b : a;
        Device group = usb2;
        group.isCompanionHubGroup = true;
        group.category = QStringLiteral("hub");
        group.isHub = true;
        group.headline = groupedHubName(usb2, usb3);
        const auto vendor = usefulIdentity(groupedVendor(usb2)) ? groupedVendor(usb2) : groupedVendor(usb3);
        group.subtitle = vendor.isEmpty()
            ? QStringLiteral("USB 2 + USB 3")
            : QStringLiteral("%1 · USB 2 + USB 3").arg(vendor);
        group.displayName = group.headline;
        group.displayVendor = vendor;
        group.identitySource = QStringLiteral("companion_group");
        group.linkSummary = QStringLiteral("USB2: %1 · USB3: %2")
            .arg(rateLabel(usb2.speed), rateLabel(usb3.speed));
        group.dataRateWarning = usb2.dataRateWarning || usb3.dataRateWarning;
        if (group.dataRateWarning) {
            const Device &warningDevice = usb2.dataRateWarning ? usb2 : usb3;
            group.dataRateSummary = warningDevice.dataRateSummary;
            group.dataRateDetail = warningDevice.dataRateDetail;
        } else {
            group.dataRateSummary.clear();
            group.dataRateDetail.clear();
        }
        group.containerId = usb2.containerId;
        group.serial = selectCompanionSerial(usb2.serial, usb3.serial);
        group.logicalPath = usb2.logicalPath;
        group.logicalParentPath = usb2.logicalParentPath;
        group.logicalRootBus = usb2.logicalRootBus;
        group.logicalPortNumber = usb2.logicalPortNumber;
        group.logicalPaths = usb2.logicalPaths;
        group.logicalPaths.append(usb3.logicalPaths);
        group.logicalPaths.removeDuplicates();
        group.logicalParentPaths = usb2.logicalParentPaths;
        group.logicalParentPaths.append(usb3.logicalParentPaths);
        group.logicalParentPaths.removeDuplicates();
        group.logicalPortNumbers = usb2.logicalPortNumbers;
        group.logicalPortNumbers.append(usb3.logicalPortNumbers);
        group.logicalPortNumbers.removeDuplicates();
        group.reportedDownstreamPorts = usb2.hubPortsTotal.toInt() > 0
            ? usb2.hubPortsTotal.toInt() : usb3.hubPortsTotal.toInt();
        group.details.clear();
        addDetail(group, QStringLiteral("Hub ports"), QString::number(group.reportedDownstreamPorts));
        addDetail(group, QStringLiteral("USB 2 companion"), usb2.product);
        addDetail(group, QStringLiteral("USB 2 link"), usb2.speedLabel);
        addDetail(group, QStringLiteral("USB 2 ports used"),
                  QStringLiteral("%1 / %2").arg(usb2.hubPortsUsed, usb2.hubPortsTotal));
        addDetail(group, QStringLiteral("USB 2 port"), usb2.portId);
        addDetail(group, QStringLiteral("USB 2 VID:PID"), usb2.vendorId + QStringLiteral(":") + usb2.productId);
        addDetail(group, QStringLiteral("USB 3 companion"), usb3.product);
        addDetail(group, QStringLiteral("USB 3 link"), usb3.speedLabel);
        addDetail(group, QStringLiteral("USB 3 ports used"),
                  QStringLiteral("%1 / %2").arg(usb3.hubPortsUsed, usb3.hubPortsTotal));
        addDetail(group, QStringLiteral("USB 3 port"), usb3.portId);
        addDetail(group, QStringLiteral("USB 3 VID:PID"), usb3.vendorId + QStringLiteral(":") + usb3.productId);
        addDetail(group, QStringLiteral("Container ID"), group.containerId);
        addDetail(group, QStringLiteral("USB database"), !usb2.productDb.isEmpty() ? usb2.productDb : usb3.productDb);
        group.usb2Product = usb2.product;
        group.usb2VendorId = usb2.vendorId;
        group.usb2ProductId = usb2.productId;
        group.usb2Speed = usb2.speed;
        group.usb2SpeedLabel = usb2.speedLabel;
        group.usb2PortId = usb2.portId;
        group.usb2PortsTotal = usb2.hubPortsTotal.toInt();
        group.usb2PortsUsed = usb2.hubPortsUsed.toInt();
        group.usb3Product = usb3.product;
        group.usb3VendorId = usb3.vendorId;
        group.usb3ProductId = usb3.productId;
        group.usb3Speed = usb3.speed;
        group.usb3SpeedLabel = usb3.speedLabel;
        group.usb3PortId = usb3.portId;
        group.usb3PortsTotal = usb3.hubPortsTotal.toInt();
        group.usb3PortsUsed = usb3.hubPortsUsed.toInt();
        return group;
    };

    for (int i = 0; i < m_devices.size(); ++i) {
        if (consumed.at(i)) continue;
        const auto &device = m_devices.at(i);
        if (device.isHub) {
            int match = -1;
            int matches = 0;
            for (int j = i + 1; j < m_devices.size(); ++j) {
                if (consumed.at(j)) continue;
                if (companion(device, m_devices.at(j))) {
                    match = j;
                    ++matches;
                }
            }
            int reverseMatches = 0;
            if (matches == 1) {
                for (int j = 0; j < m_devices.size(); ++j)
                    if (j != match && companion(m_devices.at(match), m_devices.at(j))) ++reverseMatches;
            }
            if (matches == 1 && reverseMatches == 1) {
                m_filtered.append(makeGroup(device, m_devices.at(match)));
                consumed[match] = true;
                continue;
            }
        }
        m_filtered.append(device);
    }
    resolveTopology();
    for (const auto &device : m_filtered) {
        if (!m_showHubs && device.isHub) continue;
        if (!m_showEmptyPorts && device.status == 0) continue;
        m_visible.append(device);
    }
    endResetModel();
    emit countChanged();
}

void DeviceModel::resolveTopology()
{
    QHash<QString, QList<int>> aliasOwners;
    QHash<QString, QList<int>> idOwners;

    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        device.topologyId.clear();
        device.topologyParentId.clear();
        device.topologyDepth = 0;
        device.topologyPort.clear();
        device.topologyPath = device.logicalPath;
        device.topologyAliases = device.logicalPaths;
        device.topologyResolved = false;
        device.topologyIssue.clear();

        if (device.topologyAliases.isEmpty()) {
            device.topologyIssue = QStringLiteral("no valid port.id path");
            continue;
        }
        if (device.isCompanionHubGroup && !device.containerId.isEmpty()) {
            auto aliases = device.topologyAliases;
            aliases.sort();
            device.topologyId = QStringLiteral("group:%1:%2").arg(device.containerId, aliases.join(QLatin1Char('|')));
        }
        else if (!device.logicalPath.isEmpty())
            device.topologyId = QStringLiteral("path:%1").arg(device.logicalPath);
        else
            device.topologyId = QStringLiteral("paths:%1").arg(device.topologyAliases.join(QLatin1Char('|')));

        for (const auto &alias : device.topologyAliases)
            aliasOwners[alias].append(i);
        idOwners[device.topologyId].append(i);
    }

    auto markIssue = [this](int index, const QString &issue) {
        auto &device = m_filtered[index];
        device.topologyResolved = false;
        device.topologyParentId.clear();
        device.topologyDepth = 0;
        if (device.topologyIssue.isEmpty()) device.topologyIssue = issue;
    };

    for (auto it = aliasOwners.cbegin(); it != aliasOwners.cend(); ++it) {
        if (it.value().size() > 1) {
            for (const auto index : it.value())
                markIssue(index, QStringLiteral("logical alias claimed by multiple presentation devices: %1").arg(it.key()));
        }
    }
    for (auto it = idOwners.cbegin(); it != idOwners.cend(); ++it) {
        if (it.value().size() > 1) {
            for (const auto index : it.value())
                markIssue(index, QStringLiteral("duplicate topology identity: %1").arg(it.key()));
        }
    }

    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        if (!device.topologyIssue.isEmpty()) continue;

        QSet<QString> parentPaths;
        for (const auto &parentPath : device.logicalParentPaths) {
            if (!parentPath.isEmpty()) parentPaths.insert(parentPath);
        }
        if (parentPaths.isEmpty()) {
            device.topologyResolved = true;
            continue;
        }

        QSet<int> parentIndexes;
        for (const auto &parentPath : parentPaths) {
            const auto owners = aliasOwners.value(parentPath);
            if (owners.size() != 1) {
                markIssue(i, owners.isEmpty()
                              ? QStringLiteral("parent path not found: %1").arg(parentPath)
                              : QStringLiteral("parent path is ambiguous: %1").arg(parentPath));
                break;
            }
            if (owners.first() == i) {
                markIssue(i, QStringLiteral("self-referential topology parent: %1").arg(parentPath));
                break;
            }
            parentIndexes.insert(owners.first());
        }
        if (!device.topologyIssue.isEmpty()) continue;
        if (parentIndexes.size() != 1) {
            markIssue(i, QStringLiteral("multiple presentation parents"));
            continue;
        }

        const auto parentIndex = *parentIndexes.cbegin();
        const auto &parent = m_filtered.at(parentIndex);
        device.topologyParentId = parent.topologyId;
        device.topologyResolved = true;
        QSet<QString> ports;
        for (const auto &port : device.logicalPortNumbers)
            if (!port.isEmpty()) ports.insert(port);
        if (ports.size() == 1) device.topologyPort = *ports.cbegin();
    }

    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        if (!device.topologyResolved || device.topologyParentId.isEmpty()) continue;
        QSet<int> seen;
        int current = i;
        int depth = 0;
        while (!m_filtered.at(current).topologyParentId.isEmpty()) {
            if (seen.contains(current)) {
                markIssue(i, QStringLiteral("topology cycle detected"));
                break;
            }
            seen.insert(current);
            const auto owners = idOwners.value(m_filtered.at(current).topologyParentId);
            if (owners.size() != 1 || !m_filtered.at(owners.first()).topologyResolved) {
                markIssue(i, QStringLiteral("topology parent is unresolved"));
                break;
            }
            current = owners.first();
            ++depth;
        }
        if (device.topologyResolved) device.topologyDepth = depth;
    }

    QHash<QString, QList<int>> childrenByParent;
    QHash<QString, int> idToIndex;
    QList<int> roots;
    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        device.topologyHasChildren = false;
        device.topologyIsLastChild = false;
        device.topologyAncestorLastFlags.clear();
        idToIndex.insert(device.topologyId, i);
    }
    for (int i = 0; i < m_filtered.size(); ++i) {
        const auto &device = m_filtered.at(i);
        const auto parentIndex = idToIndex.value(device.topologyParentId, -1);
        if (device.topologyResolved && !device.topologyParentId.isEmpty()
            && parentIndex >= 0)
            childrenByParent[device.topologyParentId].append(i);
        else
            roots.append(i);
    }
    for (auto it = childrenByParent.cbegin(); it != childrenByParent.cend(); ++it) {
        const auto children = it.value();
        if (idToIndex.contains(it.key()))
            m_filtered[idToIndex.value(it.key())].topologyHasChildren = !children.isEmpty();
        for (int i = 0; i < children.size(); ++i)
            m_filtered[children.at(i)].topologyIsLastChild = i == children.size() - 1;
    }
    for (int i = 0; i < roots.size(); ++i)
        m_filtered[roots.at(i)].topologyIsLastChild = i == roots.size() - 1;

    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        if (!device.topologyResolved || device.topologyParentId.isEmpty()) continue;
        QStringList ancestorIds;
        QString currentId = device.topologyParentId;
        QSet<QString> seen;
        while (!currentId.isEmpty() && !seen.contains(currentId)) {
            seen.insert(currentId);
            ancestorIds.prepend(currentId);
            const auto parentIndex = idToIndex.value(currentId, -1);
            if (parentIndex < 0) break;
            currentId = m_filtered.at(parentIndex).topologyParentId;
        }
        for (const auto &ancestorId : ancestorIds) {
            const auto ancestorIndex = idToIndex.value(ancestorId, -1);
            if (ancestorIndex >= 0)
                device.topologyAncestorLastFlags.append(m_filtered.at(ancestorIndex).topologyIsLastChild);
        }
    }
    resolvePhysicalProfiles();
    resolvePhysicalInstances();
    if (m_showHubs && m_showTopologyTree)
        resolvePresentationTree();
}

void DeviceModel::resolvePresentationTree()
{
    QHash<QString, int> idToIndex;
    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        device.presentationTreeParentId.clear();
        device.presentationTreePort.clear();
        device.presentationTreeDepth = 0;
        device.presentationTreeHasChildren = false;
        device.presentationTreeIsLastChild = false;
        device.presentationTreeAncestorLastFlags.clear();
        idToIndex.insert(device.topologyId, i);
    }

    QList<int> visible;
    QSet<int> visibleSet;
    for (int i = 0; i < m_filtered.size(); ++i) {
        visible.append(i);
        visibleSet.insert(i);
    }

    QHash<QString, QList<int>> childrenByParent;
    QList<int> roots;
    for (const auto index : visible) {
        auto &device = m_filtered[index];
        QString candidate = device.topologyParentId;
        QString directParent = candidate;
        QSet<QString> seen;
        bool attached = false;
        while (!candidate.isEmpty() && !seen.contains(candidate)) {
            seen.insert(candidate);
            const auto parentIndex = idToIndex.value(candidate, -1);
            if (parentIndex < 0 || !visibleSet.contains(parentIndex)) {
                if (parentIndex < 0) candidate.clear();
                else candidate = m_filtered.at(parentIndex).topologyParentId;
                continue;
            }
            device.presentationTreeParentId = candidate;
            if (candidate == directParent)
                device.presentationTreePort = device.topologyPort;
            attached = true;
            break;
        }
        if (!attached)
            roots.append(index);
        else
            childrenByParent[device.presentationTreeParentId].append(index);
    }

    auto sortSiblings = [this](QList<int> &siblings) {
        std::stable_sort(siblings.begin(), siblings.end(), [this](int left, int right) {
            bool leftOk = false;
            bool rightOk = false;
            const int leftPort = m_filtered.at(left).topologyPort.toInt(&leftOk);
            const int rightPort = m_filtered.at(right).topologyPort.toInt(&rightOk);
            return topologySiblingLess(leftPort, leftOk, rightPort, rightOk);
        });
    };
    sortSiblings(roots);
    for (auto it = childrenByParent.begin(); it != childrenByParent.end(); ++it)
        sortSiblings(it.value());

    QList<int> ordered;
    QSet<int> visited;
    std::function<void(int)> appendSubtree = [&](int index) {
        if (visited.contains(index)) return;
        visited.insert(index);
        ordered.append(index);
        for (const auto child : childrenByParent.value(m_filtered.at(index).topologyId))
            appendSubtree(child);
    };
    for (const auto root : roots) appendSubtree(root);
    for (const auto index : visible) appendSubtree(index);

    QList<Device> reordered;
    reordered.reserve(ordered.size());
    for (const auto index : ordered) reordered.append(m_filtered.at(index));
    m_filtered = reordered;

    idToIndex.clear();
    for (int i = 0; i < m_filtered.size(); ++i)
        idToIndex.insert(m_filtered.at(i).topologyId, i);

    childrenByParent.clear();
    roots.clear();
    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        if (!device.presentationTreeParentId.isEmpty())
            childrenByParent[device.presentationTreeParentId].append(i);
        else
            roots.append(i);
    }
    for (const auto index : roots) {
        auto &device = m_filtered[index];
        device.presentationTreeDepth = 0;
    }
    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        QSet<QString> seen;
        QString parentId = device.presentationTreeParentId;
        int depth = 0;
        while (!parentId.isEmpty() && !seen.contains(parentId)) {
            seen.insert(parentId);
            const auto parentIndex = idToIndex.value(parentId, -1);
            if (parentIndex < 0) break;
            ++depth;
            parentId = m_filtered.at(parentIndex).presentationTreeParentId;
        }
        device.presentationTreeDepth = depth;
    }
    for (auto it = childrenByParent.cbegin(); it != childrenByParent.cend(); ++it) {
        const auto parentIndex = idToIndex.value(it.key(), -1);
        if (parentIndex >= 0)
            m_filtered[parentIndex].presentationTreeHasChildren = !it.value().isEmpty();
        for (int i = 0; i < it.value().size(); ++i)
            m_filtered[it.value().at(i)].presentationTreeIsLastChild = i == it.value().size() - 1;
    }
    for (int i = 0; i < roots.size(); ++i)
        m_filtered[roots.at(i)].presentationTreeIsLastChild = i == roots.size() - 1;

    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        QStringList ancestorIds;
        QString currentId = device.presentationTreeParentId;
        QSet<QString> seen;
        while (!currentId.isEmpty() && !seen.contains(currentId)) {
            seen.insert(currentId);
            ancestorIds.prepend(currentId);
            const auto parentIndex = idToIndex.value(currentId, -1);
            if (parentIndex < 0) break;
            currentId = m_filtered.at(parentIndex).presentationTreeParentId;
        }
        for (const auto &ancestorId : ancestorIds) {
            const auto ancestorIndex = idToIndex.value(ancestorId, -1);
            if (ancestorIndex >= 0)
                device.presentationTreeAncestorLastFlags.append(m_filtered.at(ancestorIndex).presentationTreeIsLastChild);
        }
    }
}

void DeviceModel::resolvePhysicalProfiles()
{
    QList<PhysicalDeviceObservation> observations;
    observations.reserve(m_filtered.size());
    for (const auto &device : m_filtered) {
        PhysicalDeviceObservation observation;
        observation.topologyId = device.topologyId;
        observation.topologyParentId = device.topologyParentId;
        observation.topologyPort = device.topologyPort;
        observation.vendorId = device.vendorId;
        observation.productId = device.productId;
        observation.manufacturer = device.manufacturer;
        observation.vendor = device.vendor;
        observation.productDb = device.productDb;
        observation.containerId = device.containerId;
        observation.deviceClass = device.deviceClass;
        observation.deviceSubclass = device.deviceSubclass;
        observation.interfaceClasses = device.interfaceClasses;
        observation.isCompanionHubGroup = device.isCompanionHubGroup;
        if (device.isCompanionHubGroup) {
            observation.companionFunctions = {
                {device.usb2VendorId, device.usb2ProductId},
                {device.usb3VendorId, device.usb3ProductId},
            };
        }
        observations.append(observation);
    }

    QList<PhysicalDeviceMatch> matches;
    matches.reserve(m_filtered.size());
    for (const auto &observation : observations)
        matches.append(PhysicalDeviceProfileMatcher::resolve(observation, observations));

    QHash<QString, int> presentationIndexes;
    for (int i = 0; i < m_filtered.size(); ++i)
        presentationIndexes.insert(m_filtered.at(i).topologyId, i);

    for (int i = 0; i < m_filtered.size(); ++i) {
        const auto &match = matches.at(i);
        auto &device = m_filtered[i];
        device.physicalName = match.physicalName;
        device.physicalVendor = match.physicalVendor;
        device.physicalIdentitySource = match.ambiguous
            ? QStringLiteral("none") : match.physicalIdentitySource;
        device.physicalConfidence = match.ambiguous
            ? QStringLiteral("none") : match.physicalConfidence;
        device.physicalProfileId = match.ambiguous ? QString() : match.physicalProfileId;
        device.physicalType = match.ambiguous ? QString() : match.physicalType;
        device.physicalIsComposite = !match.ambiguous && match.physicalIsComposite;
        device.physicalGroupId = match.ambiguous ? QString() : match.physicalGroupId;
        device.physicalRootId = match.ambiguous ? QString() : match.physicalRootId;
        device.physicalMemberIds = match.ambiguous ? QStringList() : match.physicalMemberIds;
        device.physicalRelationshipType = match.ambiguous ? QString() : match.physicalRelationshipType;
        device.physicalRelationshipSource = match.ambiguous ? QString() : match.physicalRelationshipSource;
        device.physicalRelationshipEvidence = match.ambiguous ? QString() : match.physicalRelationshipEvidence;
        device.profilePhysicalName = device.physicalName;
        device.profilePhysicalVendor = device.physicalVendor;
        device.profilePhysicalIdentitySource = device.physicalIdentitySource;
        device.profilePhysicalConfidence = device.physicalConfidence;
    }

    for (const auto &match : matches) {
        if (match.ambiguous || match.physicalGroupId.isEmpty()) continue;
        for (const auto &memberId : match.physicalChildMemberIds) {
            const auto memberIndex = presentationIndexes.value(memberId, -1);
            if (memberIndex < 0) continue;
            auto &member = m_filtered[memberIndex];
            member.physicalGroupId = match.physicalGroupId;
            member.physicalRootId = match.physicalRootId;
            member.physicalMemberIds = match.physicalMemberIds;
            member.physicalProfileId = match.physicalProfileId;
            member.physicalIdentitySource = match.physicalIdentitySource;
            member.physicalConfidence = match.physicalConfidence;
            member.physicalType = match.physicalType;
            member.physicalRelationshipType = match.physicalChildRelationshipType;
            member.physicalRelationshipSource = match.physicalRelationshipSource;
            member.physicalRelationshipEvidence = match.physicalRelationshipEvidence;
            member.physicalIsComposite = true;
            member.profilePhysicalName = match.physicalName;
            member.profilePhysicalVendor = match.physicalVendor;
            member.profilePhysicalIdentitySource = match.physicalIdentitySource;
            member.profilePhysicalConfidence = match.physicalConfidence;
        }
    }
}

void DeviceModel::resolvePhysicalInstances()
{
    auto useful = [](const QString &value) {
        return !value.trimmed().isEmpty() && value.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) != 0;
    };
    auto rootFor = [this](int index) {
        const auto &device = m_filtered.at(index);
        if (device.physicalRootId.isEmpty() || device.physicalRootId == device.topologyId)
            return index;
        for (int i = 0; i < m_filtered.size(); ++i)
            if (m_filtered.at(i).topologyId == device.physicalRootId) return i;
        return index;
    };
    auto rootKey = [&useful](const Device &root, QString *source) -> QString {
        if (isTrustworthyUsbSerial(root.serial)) {
            if (source) *source = QStringLiteral("serial");
            return QStringLiteral("serial:%1:%2:%3").arg(root.vendorId, root.productId, root.serial);
        }
        if (useful(root.containerId)) {
            if (source) *source = QStringLiteral("container_id");
            if (root.isCompanionHubGroup) {
                return QStringLiteral("container:%1:%2:%3")
                    .arg(root.containerId,
                         root.usb2VendorId + root.usb2ProductId,
                         root.usb3VendorId + root.usb3ProductId);
            }
            return QStringLiteral("container:%1").arg(root.containerId);
        }
        if (!root.topologyId.isEmpty()) {
            if (source) *source = QStringLiteral("topology_fallback");
            return QStringLiteral("topology:%1").arg(root.topologyId);
        }
        if (source) *source = QStringLiteral("none");
        return {};
    };

    QHash<QString, QString> keySources;
    QHash<QString, QStringList> keyRoots;
    QHash<int, QString> rootKeys;
    for (int i = 0; i < m_filtered.size(); ++i) {
        if (!m_filtered.at(i).physicalRootId.isEmpty() && m_filtered.at(i).physicalRootId != m_filtered.at(i).topologyId)
            continue;
        QString source;
        const auto key = rootKey(m_filtered.at(i), &source);
        if (!key.isEmpty()) {
            rootKeys.insert(i, key);
            keySources.insert(key, source);
            keyRoots[key].append(m_filtered.at(i).topologyId);
        }
    }

    QHash<QString, bool> ambiguousKeys;
    for (auto it = keyRoots.cbegin(); it != keyRoots.cend(); ++it)
        ambiguousKeys.insert(it.key(), it.value().size() > 1);

    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &device = m_filtered[i];
        device.physicalIntegratedMembers.clear();
        const int rootIndex = rootFor(i);
        const auto key = rootKeys.value(rootIndex);
        device.physicalInstanceKey = key;
        device.physicalInstanceKeySource = key.isEmpty()
            ? QStringLiteral("none") : keySources.value(key);
        device.physicalAliasIdentityAmbiguous = ambiguousKeys.value(key, false);
        device.physicalName = device.profilePhysicalName;
        device.physicalVendor = device.profilePhysicalVendor;
        device.physicalIdentitySource = device.profilePhysicalIdentitySource.isEmpty()
            ? QStringLiteral("none") : device.profilePhysicalIdentitySource;
        device.physicalConfidence = device.profilePhysicalConfidence.isEmpty()
            ? QStringLiteral("none") : device.profilePhysicalConfidence;
        if (!device.physicalAliasIdentityAmbiguous && m_userAliases.contains(key)) {
            device.physicalName = m_userAliases.value(key);
            device.physicalIdentitySource = QStringLiteral("user");
        }
    }

    QHash<QString, int> topologyIndexes;
    for (int i = 0; i < m_filtered.size(); ++i)
        topologyIndexes.insert(m_filtered.at(i).topologyId, i);
    for (int i = 0; i < m_filtered.size(); ++i) {
        auto &root = m_filtered[i];
        if (root.physicalRelationshipType != QLatin1String("root")) continue;
        for (const auto &memberId : root.physicalMemberIds) {
            const auto memberIndex = topologyIndexes.value(memberId, -1);
            if (memberIndex < 0 || memberIndex == i) continue;
            const auto &member = m_filtered.at(memberIndex);
            if (member.physicalRelationshipType != QLatin1String("integrated_function")) continue;
            root.physicalIntegratedMembers.append(QVariantMap{
                {QStringLiteral("topologyId"), member.topologyId},
                {QStringLiteral("displayName"), member.displayName},
                {QStringLiteral("displayVendor"), member.displayVendor},
                {QStringLiteral("vendorId"), member.vendorId},
                {QStringLiteral("productId"), member.productId},
                {QStringLiteral("deviceClass"), member.deviceClass},
                {QStringLiteral("primaryDriver"), member.primaryDriver},
                {QStringLiteral("physicalRelationshipType"), member.physicalRelationshipType},
                {QStringLiteral("physicalRelationshipSource"), member.physicalRelationshipSource},
                {QStringLiteral("physicalRelationshipEvidence"), member.physicalRelationshipEvidence},
                {QStringLiteral("topologyPort"), member.topologyPort}
            });
        }
    }
}

void DeviceModel::refresh() { m_client.refresh(); }
void DeviceModel::startMonitoring() { m_client.snapshot(); }

void DeviceModel::setShowHubs(bool value)
{
    if (m_showHubs == value) return;
    m_showHubs = value;
    rebuildFiltered();
    emit showHubsChanged();
}

void DeviceModel::setShowEmptyPorts(bool value)
{
    if (m_showEmptyPorts == value) return;
    m_showEmptyPorts = value;
    rebuildFiltered();
    emit showEmptyPortsChanged();
}

void DeviceModel::setShowTopologyTree(bool value)
{
    if (m_showTopologyTree == value) return;
    m_showTopologyTree = value;
    rebuildFiltered();
    emit showTopologyTreeChanged();
}

void DeviceModel::setUserAliasesJson(const QString &value)
{
    if (m_userAliasesJson == value) return;
    m_userAliasesJson = value;
    m_userAliases.clear();
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (document.isObject()) {
        const auto aliases = document.object();
        for (auto it = aliases.begin(); it != aliases.end(); ++it) {
            if (it.value().isString() && !it.value().toString().trimmed().isEmpty())
                m_userAliases.insert(it.key(), it.value().toString().trimmed());
        }
    }
    resolvePhysicalInstances();
    rebuildFiltered();
    emit userAliasesChanged();
}

int DeviceModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_visible.size(); }

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size()) return {};
    const auto &device = m_visible.at(index.row());
    switch (role) {
    case HeadlineRole: return device.headline; case SubtitleRole: return device.subtitle;
    case IconRole: return device.icon; case CategoryRole: return device.category;
    case BulletsRole: return device.bullets; case StatusRole: return device.status;
    case VendorIdRole: return device.vendorId; case ProductIdRole: return device.productId;
    case SpeedRole: return device.speed; case SpeedLabelRole: return device.speedLabel;
    case UsbVersionRole: return device.usbVersion; case MaxPowerRole: return device.maxPower;
    case IsHubRole: return device.isHub; case BusPortRole: return device.busPort;
    case PortNumberRole: return device.portNumber; case DataRoleRole: return device.dataRole;
    case PowerRoleRole: return device.powerRole; case IsConnectedRole: return device.isConnected;
    case CableSpeedRole: return device.cableSpeed; case CableCurrentRole: return device.cableCurrent;
    case CableMaxWattsRole: return device.cableMaxWatts; case CableTypeRole: return device.cableType;
    case CableVendorRole: return device.cableVendor; case HasPowerDeliveryRole: return device.hasPowerDelivery;
    case SourcePdosRole: return device.sourcePdos; case MaxSourcePowerRole: return device.maxSourcePower;
    case HasChargingDiagRole: return device.hasChargingDiag; case ChargingSummaryRole: return device.chargingSummary;
    case ChargingDetailRole: return device.chargingDetail; case ChargingIsWarningRole: return device.chargingIsWarning;
    case DeviceClassRole: return device.deviceClass; case DeviceSubclassRole: return device.deviceSubclass;
    case PrimaryDriverRole: return device.primaryDriver; case VendorRole: return device.vendor;
    case ProductRole: return device.product; case SerialRole: return device.serial;
    case DisplayNameRole: return device.displayName; case DisplayVendorRole: return device.displayVendor;
    case IdentitySourceRole: return device.identitySource; case RawProductRole: return device.product;
    case RawManufacturerRole: return device.manufacturer;
    case LinkSummaryRole: return device.linkSummary; case DataRateCapableRole: return device.dataRateCapable;
    case DataRateNegotiatedRole: return device.dataRateNegotiated; case DataRateSummaryRole: return device.dataRateSummary;
    case DataRateDetailRole: return device.dataRateDetail; case DataRateVerdictRole: return device.dataRateVerdict;
    case DataRateWarningRole: return device.dataRateWarning; case DetailsRole: return device.details;
    case UsbCapableSpeedRole: return device.usbCapableSpeed; case UsbCapableGenRole: return device.usbCapableGen;
    case UsbLinkVerdictRole: return device.usbLinkVerdict; case PortIdRole: return device.portId;
    case PortPeerIdRole: return device.portPeerId; case PortPeerStateRole: return device.portPeerState;
    case PortConnectTypeRole: return device.portConnectType; case HubPortsTotalRole: return device.hubPortsTotal;
    case HubPortsUsedRole: return device.hubPortsUsed; case HubPowerCommittedRole: return device.hubPowerCommitted;
    case PowerSourceRole: return device.powerSource; case ProductDbRole: return device.productDb;
    case KernelQuirksRole: return device.kernelQuirks; case UsbAltmodeSvidsRole: return device.usbAltmodeSvids;
    case UsbAltmodeStateRole: return device.usbAltmodeState;
    case HasAltmodeRole: return device.hasAltmode; case AltmodeNameRole: return device.altmodeName;
    case AltmodeSummaryRole: return device.altmodeSummary;
    case IsCompanionHubGroupRole: return device.isCompanionHubGroup;
    case Usb2ProductRole: return device.usb2Product; case Usb2VendorIdRole: return device.usb2VendorId;
    case Usb2ProductIdRole: return device.usb2ProductId; case Usb2SpeedRole: return device.usb2Speed;
    case Usb2SpeedLabelRole: return device.usb2SpeedLabel; case Usb2PortIdRole: return device.usb2PortId;
    case Usb2PortsTotalRole: return device.usb2PortsTotal; case Usb2PortsUsedRole: return device.usb2PortsUsed;
    case Usb3ProductRole: return device.usb3Product; case Usb3VendorIdRole: return device.usb3VendorId;
    case Usb3ProductIdRole: return device.usb3ProductId; case Usb3SpeedRole: return device.usb3Speed;
    case Usb3SpeedLabelRole: return device.usb3SpeedLabel; case Usb3PortIdRole: return device.usb3PortId;
    case Usb3PortsTotalRole: return device.usb3PortsTotal; case Usb3PortsUsedRole: return device.usb3PortsUsed;
    case ReportedDownstreamPortsRole: return device.reportedDownstreamPorts; case ContainerIdRole: return device.containerId;
    case TopologyIdRole: return device.topologyId; case TopologyParentIdRole: return device.topologyParentId;
    case TopologyDepthRole: return device.topologyDepth; case TopologyPortRole: return device.topologyPort;
    case TopologyPathRole: return device.topologyPath; case TopologyAliasesRole: return device.topologyAliases;
    case TopologyResolvedRole: return device.topologyResolved; case TopologyIssueRole: return device.topologyIssue;
    case TopologyHasChildrenRole: return device.topologyHasChildren;
    case TopologyIsLastChildRole: return device.topologyIsLastChild;
    case TopologyAncestorLastFlagsRole: return device.topologyAncestorLastFlags;
    case PresentationTreeParentIdRole: return device.presentationTreeParentId;
    case PresentationTreeDepthRole: return device.presentationTreeDepth;
    case PresentationTreePortRole: return device.presentationTreePort;
    case PresentationTreeHasChildrenRole: return device.presentationTreeHasChildren;
    case PresentationTreeIsLastChildRole: return device.presentationTreeIsLastChild;
    case PresentationTreeAncestorLastFlagsRole: return device.presentationTreeAncestorLastFlags;
    case PhysicalNameRole: return device.physicalName; case PhysicalVendorRole: return device.physicalVendor;
    case PhysicalIdentitySourceRole: return device.physicalIdentitySource;
    case PhysicalConfidenceRole: return device.physicalConfidence; case PhysicalProfileIdRole: return device.physicalProfileId;
    case PhysicalIsCompositeRole: return device.physicalIsComposite; case PhysicalTypeRole: return device.physicalType;
    case PhysicalGroupIdRole: return device.physicalGroupId; case PhysicalRootIdRole: return device.physicalRootId;
    case PhysicalMemberIdsRole: return device.physicalMemberIds;
    case PhysicalIntegratedMembersRole: return device.physicalIntegratedMembers;
    case PhysicalRelationshipTypeRole: return device.physicalRelationshipType;
    case PhysicalRelationshipSourceRole: return device.physicalRelationshipSource;
    case PhysicalRelationshipEvidenceRole: return device.physicalRelationshipEvidence;
    case PhysicalInstanceKeyRole: return device.physicalInstanceKey;
    case PhysicalInstanceKeySourceRole: return device.physicalInstanceKeySource;
    case PhysicalAliasIdentityAmbiguousRole: return device.physicalAliasIdentityAmbiguous;
    }
    return {};
}

QHash<int, QByteArray> DeviceModel::roleNames() const
{
    return {{HeadlineRole,"headline"},{SubtitleRole,"subtitle"},{IconRole,"iconName"},{CategoryRole,"category"},
        {BulletsRole,"bullets"},{StatusRole,"status"},{VendorIdRole,"vendorId"},{ProductIdRole,"productId"},
        {SpeedRole,"speed"},{SpeedLabelRole,"speedLabel"},{UsbVersionRole,"usbVersion"},{MaxPowerRole,"maxPower"},
        {IsHubRole,"isHub"},{BusPortRole,"busPort"},{PortNumberRole,"portNumber"},{DataRoleRole,"dataRole"},
        {PowerRoleRole,"powerRole"},{IsConnectedRole,"isConnected"},{CableSpeedRole,"cableSpeed"},
        {CableCurrentRole,"cableCurrent"},{CableMaxWattsRole,"cableMaxWatts"},{CableTypeRole,"cableType"},
        {CableVendorRole,"cableVendor"},{HasPowerDeliveryRole,"hasPowerDelivery"},{SourcePdosRole,"sourcePdos"},
        {MaxSourcePowerRole,"maxSourcePower"},{HasChargingDiagRole,"hasChargingDiag"},
        {ChargingSummaryRole,"chargingSummary"},{ChargingDetailRole,"chargingDetail"},{ChargingIsWarningRole,"chargingIsWarning"},
        {DeviceClassRole,"deviceClass"},{DeviceSubclassRole,"deviceSubclass"},{PrimaryDriverRole,"primaryDriver"},
        {VendorRole,"vendor"},{ProductRole,"product"},{SerialRole,"serial"},
        {DisplayNameRole,"displayName"},{DisplayVendorRole,"displayVendor"},{IdentitySourceRole,"identitySource"},
        {RawProductRole,"rawProduct"},{RawManufacturerRole,"rawManufacturer"},{LinkSummaryRole,"linkSummary"},
        {DataRateCapableRole,"dataRateCapable"},{DataRateNegotiatedRole,"dataRateNegotiated"},
        {DataRateSummaryRole,"dataRateSummary"},{DataRateDetailRole,"dataRateDetail"},
        {DataRateVerdictRole,"dataRateVerdict"},{DataRateWarningRole,"dataRateWarning"},{DetailsRole,"details"},
        {UsbCapableSpeedRole,"usbCapableSpeed"},{UsbCapableGenRole,"usbCapableGen"},
        {UsbLinkVerdictRole,"usbLinkVerdict"},{PortIdRole,"portId"},{PortPeerIdRole,"portPeerId"},
        {PortPeerStateRole,"portPeerState"},{PortConnectTypeRole,"portConnectType"},
        {HubPortsTotalRole,"hubPortsTotal"},{HubPortsUsedRole,"hubPortsUsed"},
        {HubPowerCommittedRole,"hubPowerCommitted"},{PowerSourceRole,"powerSource"},
        {ProductDbRole,"productDb"},{KernelQuirksRole,"kernelQuirks"},
        {UsbAltmodeSvidsRole,"usbAltmodeSvids"},{UsbAltmodeStateRole,"usbAltmodeState"},
        {HasAltmodeRole,"hasAltmode"},{AltmodeNameRole,"altmodeName"},{AltmodeSummaryRole,"altmodeSummary"},
        {IsCompanionHubGroupRole,"isCompanionHubGroup"},{Usb2ProductRole,"usb2Product"},
        {Usb2VendorIdRole,"usb2VendorId"},{Usb2ProductIdRole,"usb2ProductId"},{Usb2SpeedRole,"usb2Speed"},
        {Usb2SpeedLabelRole,"usb2SpeedLabel"},{Usb2PortIdRole,"usb2PortId"},{Usb2PortsTotalRole,"usb2PortsTotal"},
        {Usb2PortsUsedRole,"usb2PortsUsed"},{Usb3ProductRole,"usb3Product"},{Usb3VendorIdRole,"usb3VendorId"},
        {Usb3ProductIdRole,"usb3ProductId"},{Usb3SpeedRole,"usb3Speed"},{Usb3SpeedLabelRole,"usb3SpeedLabel"},
        {Usb3PortIdRole,"usb3PortId"},{Usb3PortsTotalRole,"usb3PortsTotal"},{Usb3PortsUsedRole,"usb3PortsUsed"},
        {ReportedDownstreamPortsRole,"reportedDownstreamPorts"},{ContainerIdRole,"containerId"},
        {TopologyIdRole,"topologyId"},{TopologyParentIdRole,"topologyParentId"},{TopologyDepthRole,"topologyDepth"},
        {TopologyPortRole,"topologyPort"},{TopologyPathRole,"topologyPath"},{TopologyAliasesRole,"topologyAliases"},
        {TopologyResolvedRole,"topologyResolved"},{TopologyIssueRole,"topologyIssue"},
        {TopologyHasChildrenRole,"topologyHasChildren"},{TopologyIsLastChildRole,"topologyIsLastChild"},
        {TopologyAncestorLastFlagsRole,"topologyAncestorLastFlags"},
        {PresentationTreeParentIdRole,"presentationTreeParentId"},{PresentationTreeDepthRole,"presentationTreeDepth"},
        {PresentationTreePortRole,"presentationTreePort"},{PresentationTreeHasChildrenRole,"presentationTreeHasChildren"},
        {PresentationTreeIsLastChildRole,"presentationTreeIsLastChild"},
        {PresentationTreeAncestorLastFlagsRole,"presentationTreeAncestorLastFlags"},
        {PhysicalNameRole,"physicalName"},{PhysicalVendorRole,"physicalVendor"},
        {PhysicalIdentitySourceRole,"physicalIdentitySource"},{PhysicalConfidenceRole,"physicalConfidence"},
        {PhysicalProfileIdRole,"physicalProfileId"},{PhysicalIsCompositeRole,"physicalIsComposite"},
        {PhysicalTypeRole,"physicalType"},{PhysicalGroupIdRole,"physicalGroupId"},
        {PhysicalRootIdRole,"physicalRootId"},{PhysicalMemberIdsRole,"physicalMemberIds"},
        {PhysicalIntegratedMembersRole,"physicalIntegratedMembers"},
        {PhysicalRelationshipTypeRole,"physicalRelationshipType"},
        {PhysicalRelationshipSourceRole,"physicalRelationshipSource"},
        {PhysicalRelationshipEvidenceRole,"physicalRelationshipEvidence"},
        {PhysicalInstanceKeyRole,"physicalInstanceKey"},{PhysicalInstanceKeySourceRole,"physicalInstanceKeySource"},
        {PhysicalAliasIdentityAmbiguousRole,"physicalAliasIdentityAmbiguous"}};
}

} // namespace USBeeKeeper
