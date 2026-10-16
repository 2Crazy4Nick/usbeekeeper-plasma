#include "DeviceModel.h"

#include <QJsonDocument>
#include <QJsonObject>

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
        if (pair.size() == 2 && pair.at(0).toString() == key) return pair.at(1).toString();
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
        device.vendorId = idValue(object, {"vendorId", "vendor_id"});
        device.productId = idValue(object, {"productId", "product_id"});
        device.usbVersion = stringValue(object, {"usbVersion", "usb_version"});
        device.busPort = stringValue(object, {"busPort", "bus_port"});
        const auto usb = value(object, {"usb", "usb_device"}).toObject();
        device.speed = intValue(usb, {"speed", "linkSpeedMbps", "link_speed_mbps"});
        if (device.speed == 0)
            device.speed = intValue(object, {"linkSpeedMbps", "link_speed_mbps"});
        device.speedLabel = stringValue(usb, {"speedLabel", "speed_label"});
        if (device.speedLabel.isEmpty() && device.speed > 0)
            device.speedLabel = QStringLiteral("%1 Mbps").arg(device.speed);
        device.maxPower = intValue(usb, {"maxPowerMA", "max_power_ma"});
        device.portNumber = intValue(object, {"portNumber", "port_number"});
        device.isHub = device.category == QLatin1String("hub")
            || value(usb, {"isHub", "is_hub"}).toBool()
            || stringValue(object, {"deviceClass", "device_class"}).contains(QStringLiteral("hub"), Qt::CaseInsensitive);

        const auto status = stringValue(object, {"status"}).toLower();
        device.status = status == QLatin1String("empty") ? 0 : 1;
        device.isConnected = status != QLatin1String("empty") && status != QLatin1String("disconnected");
        if (device.category == QLatin1String("usbdevice")) device.category = QStringLiteral("usb");
        if (device.category.isEmpty()) device.category = device.portNumber >= 0 ? QStringLiteral("typec") : QStringLiteral("usb");

        // The old role is USB max current in mA; usbeehive exposes negotiated
        // power instead, so keep this compatibility role safely empty.
        device.dataRole = firstProperty(object, {"port.data_role", "data_role", "dataRole"});
        device.powerRole = firstProperty(object, {"port.power_role", "power_role", "powerRole"});
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

        const auto product = stringValue(object, {"product"});
        if (!product.isEmpty()) device.bullets.append(product);
        m_devices.append(device);
    }
    rebuildFiltered();
}

void DeviceModel::rebuildFiltered()
{
    beginResetModel();
    m_filtered.clear();
    for (const auto &device : m_devices) {
        if (!m_showHubs && device.isHub) continue;
        if (!m_showEmptyPorts && device.status == 0) continue;
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
        {ChargingSummaryRole,"chargingSummary"},{ChargingDetailRole,"chargingDetail"},{ChargingIsWarningRole,"chargingIsWarning"}};
}

} // namespace WhatCable
