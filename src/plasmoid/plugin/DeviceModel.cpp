#include "DeviceModel.h"

#include <QJsonDocument>
#include <QJsonObject>
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

namespace WhatCable {

DeviceModel::DeviceModel(QObject *parent)
    : QAbstractListModel(parent), m_client(this)
{
    connect(&m_client, &UsbeehiveClient::snapshotReady, this, &DeviceModel::loadSnapshot);
}

void DeviceModel::loadSnapshot(const QString &json)
{
    const auto document = QJsonDocument::fromJson(json.toUtf8());
    QJsonArray devices;
    if (document.isArray()) devices = document.array();
    else if (document.isObject()) {
        const auto root = document.object();
        devices = value(root, {"devices"}).toArray();
        if (devices.isEmpty()) devices = value(root, {"deviceSummaries", "device_summaries"}).toArray();
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
}

void DeviceModel::rebuildFiltered()
{
    beginResetModel();
    m_filtered.clear();
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
        if (!m_showHubs && device.isHub) continue;
        if (!m_showEmptyPorts && device.status == 0) continue;
        if (m_showHubs && device.isHub) {
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
    endResetModel();
    emit countChanged();
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
    emit showTopologyTreeChanged();
}

int DeviceModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_filtered.size(); }

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_filtered.size()) return {};
    const auto &device = m_filtered.at(index.row());
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
        {ReportedDownstreamPortsRole,"reportedDownstreamPorts"},{ContainerIdRole,"containerId"}};
}

} // namespace WhatCable
