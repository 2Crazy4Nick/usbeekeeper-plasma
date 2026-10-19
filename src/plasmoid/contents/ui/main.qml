import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import io.github._2crazy4nick.usbeekeeper 1.0

PlasmoidItem {
    id: root

    DeviceModel {
        id: deviceModel
        showHubs: Plasmoid.configuration.showHubs || false
        showEmptyPorts: Plasmoid.configuration.showEmptyPorts || false
        showTopologyTree: Plasmoid.configuration.showTopologyTree || false
        userAliasesJson: Plasmoid.configuration.physicalAliases || "{}"
    }

    readonly property string backendStatusText: {
        if (!deviceModel.backendAvailable) {
            return deviceModel.snapshotError.length > 0
                ? i18n("Cannot connect to usbeehived. Make sure it is running.")
                : i18n("Connecting to usbeehived…")
        }
        if (deviceModel.snapshotError.length > 0) {
            return deviceModel.hasSnapshot && deviceModel.count > 0
                ? i18n("Could not update USB device information. Displayed data may be out of date.")
                : i18n("Could not load USB device information. Try refreshing.")
        }
        return deviceModel.hasSnapshot ? "" : i18n("Loading USB device information…")
    }

    function updatePhysicalAlias(key, alias) {
        var aliases = {}
        try {
            var parsed = JSON.parse(Plasmoid.configuration.physicalAliases || "{}")
            if (parsed !== null && !Array.isArray(parsed) && typeof parsed === "object")
                aliases = parsed
        } catch (error) {
            aliases = {}
        }
        if ((alias || "").trim().length > 0)
            aliases[key] = alias.trim()
        else
            delete aliases[key]
        Plasmoid.configuration.physicalAliases = JSON.stringify(aliases)
    }

    Component.onCompleted: {
        deviceModel.startMonitoring()
    }

    switchWidth: Kirigami.Units.gridUnit * 20
    switchHeight: Kirigami.Units.gridUnit * 24

    toolTipMainText: "USBeeKeeper"
    toolTipSubText: {
        if (root.backendStatusText.length > 0) return root.backendStatusText
        var count = deviceModel.count
        if (count === 0) return "No USB devices"
        return count + " USB device" + (count !== 1 ? "s" : "")
    }

    compactRepresentation: CompactView {
        deviceCount: deviceModel.count
        onClicked: root.expanded = !root.expanded
    }

    fullRepresentation: ColumnLayout {
        Layout.preferredWidth: Kirigami.Units.gridUnit * 22
        Layout.preferredHeight: Kirigami.Units.gridUnit * 28
        Layout.minimumWidth: Kirigami.Units.gridUnit * 16
        Layout.minimumHeight: Kirigami.Units.gridUnit * 12
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "drive-removable-media-usb"
                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            }

            PlasmaComponents.Label {
                text: "USBeeKeeper"
                font.bold: true
                Layout.fillWidth: true
            }

            PlasmaComponents.Label {
                text: deviceModel.count + " device" + (deviceModel.count !== 1 ? "s" : "")
                opacity: 0.6
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }

            PlasmaComponents.ToolButton {
                icon.name: "view-refresh"
                onClicked: deviceModel.refresh()
                PlasmaComponents.ToolTip { text: "Refresh" }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            visible: root.backendStatusText.length > 0
            type: deviceModel.snapshotError.length > 0
                ? Kirigami.MessageType.Error : Kirigami.MessageType.Information
            text: root.backendStatusText
        }

        // Device list
        PlasmaComponents.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: deviceListView
                model: deviceModel
                spacing: Kirigami.Units.smallSpacing

                delegate: DeviceCard {
                    width: deviceListView.width
                    topologyTreeActive: deviceModel.showHubs && deviceModel.showTopologyTree
                    informationDetailLevel: Plasmoid.configuration.deviceInformationDetailLevel || "standard"
                    showPhysicalDeviceRenamingControls: Plasmoid.configuration.showPhysicalDeviceRenamingControls || false
                    onPhysicalAliasSubmitted: root.updatePhysicalAlias(key, alias)
                    onPhysicalAliasCleared: root.updatePhysicalAlias(key, "")
                }

                PlasmaComponents.Label {
                    anchors.centerIn: parent
                    visible: deviceModel.backendAvailable && deviceModel.hasSnapshot
                             && deviceModel.snapshotError.length === 0 && deviceModel.count === 0
                    text: i18n("No USB devices found")
                    opacity: 0.5
                }
            }
        }
    }
}
