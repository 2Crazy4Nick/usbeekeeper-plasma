import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import io.github._2crazy4nick.usbeekeeper 1.0
import "../../src/plasmoid/contents/ui" as UI
import "../../src/plasmoid/contents/ui/config" as Config

Window {
    id: window
    width: Kirigami.Units.gridUnit * 26
    height: Kirigami.Units.gridUnit * 100
    visible: true
    property int phase: 0
    function i18n(text) { return text }

    Config.ConfigGeneral {
        id: settings
        visible: false
    }

    Config.ConfigGeneral {
        id: otherWidgetSettings
        visible: false
    }

    DeviceModel {
        id: testDeviceModel
        objectName: "testDeviceModel"
        showHubs: true
    }

    // Deliberately use the same implicit model context as main.qml, not a JS
    // object or a card-local property named model.
    ListView {
        id: list
        anchors.fill: parent
        model: testDeviceModel
        delegate: UI.DeviceCard {
            width: list.width
            topologyTreeActive: testDeviceModel.showHubs && testDeviceModel.showTopologyTree
            informationDetailLevel: settings.cfg_deviceInformationDetailLevel
            showPhysicalDeviceRenamingControls: settings.cfg_showPhysicalDeviceRenamingControls
        }
    }

    function descendants(item) {
        var items = [item]
        for (var i = 0; i < item.children.length; ++i)
            items = items.concat(descendants(item.children[i]))
        return items
    }

    function verify() {
        if (otherWidgetSettings.cfg_deviceInformationDetailLevel !== "standard"
                || otherWidgetSettings.cfg_showPhysicalDeviceRenamingControls !== false)
            throw new Error("Detail settings leaked to another widget instance")
        var selector = descendants(settings).filter(function(item) {
            return item.objectName === "deviceInformationDetailLevel"
        })[0]
        if (!selector || selector.currentValue !== settings.cfg_deviceInformationDetailLevel)
            throw new Error("Settings selector does not follow persisted detail level")
        if (list.count !== 2) throw new Error("Companion group/child rows missing")
        for (var i = 0; i < list.count; ++i) {
            var card = list.itemAtIndex(i)
            if (!card) throw new Error("ListView delegate missing")
            var sections = card.deviceData.deviceInformation
            if (!sections || sections.length !== (i === 0 ? 2 : 1))
                throw new Error("Production model role did not reach delegate")
            var items = descendants(card)
            var info = items.filter(function(item) { return item.objectName === "deviceInformation" })[0]
            if (!info || !info.visible || info.width <= 0 || info.height <= 0)
                throw new Error("Device Information is not rendered")
            var rows = descendants(info).filter(function(item) { return item.objectName === "deviceDetailRow" })
            var expectedCount = card.detailRank === 0 ? (i === 0 ? 6 : 3) : (i === 0 ? 14 : 6)
            if (rows.length !== expectedCount)
                throw new Error("Missing rendered information rows: " + rows.length)
            if (rows.some(function(row) { return row.labelText === "Negotiated link speed" }))
                throw new Error("Negotiated link speed duplicates header")
            var versions = rows.filter(function(row) { return row.labelText === "USB version" })
            if (versions.length !== (i === 0 && card.detailRank > 0 ? 2 : 0))
                throw new Error("USB version fallback/deduplication failed")
            var expectedIds = i === 0 ? ["1234:0001", "1234:0002"] : ["1234:0003"]
            var ids = rows.filter(function(row) { return row.labelText === "VID:PID" })
                          .map(function(row) { return row.valueText })
            if (JSON.stringify(ids) !== JSON.stringify(expectedIds))
                throw new Error("Companion USB identities lost")
            rows.forEach(function(row) {
                if (!row.visible || row.height <= 0) throw new Error("Hidden information row")
                var origin = row.mapToItem(card, 0, 0)
                if (origin.x < -1 || origin.x + row.width > card.width + 1)
                    throw new Error("Information extends beyond delegate")
            })
            var allRows = items.filter(function(item) { return item.objectName === "deviceDetailRow" })
            var port = allRows.some(function(row) { return row.labelText === "Port" })
            if (i === 1 && port !== (card.detailRank === 2))
                throw new Error("Port must be debug-only")
            var database = allRows.some(function(row) { return row.labelText === "USB database" })
            if (i === 1 && database !== (card.detailRank === 2))
                throw new Error("Database must be debug-only")
            var rawProduct = allRows.some(function(row) { return row.labelText === "Raw product" })
            if (i === 1 && rawProduct !== (card.detailRank >= 1))
                throw new Error("Raw product must be extended/debug-only")
            if (i === 0) {
                var actions = items.filter(function(item) { return item.objectName === "physicalAliasActions" })[0]
                if (!actions || actions.visible !== (settings.cfg_showPhysicalDeviceRenamingControls && !card.editingPhysicalAlias))
                    throw new Error("Alias controls do not follow setting")
                var editor = items.filter(function(item) { return item.objectName === "physicalAliasEditor" })[0]
                if (!editor || editor.visible !== (settings.cfg_showPhysicalDeviceRenamingControls && card.editingPhysicalAlias))
                    throw new Error("Alias editor does not follow setting")
                if (!settings.cfg_showPhysicalDeviceRenamingControls && card.editingPhysicalAlias)
                    throw new Error("Hidden rename editor left active")
                var alias = items.filter(function(item) { return item.objectName === "physicalAliasName" })[0]
                if (!alias || !alias.visible || card.deviceData.physicalName !== "Example physical alias")
                    throw new Error("Hiding rename controls lost saved alias")
            }
        }
    }

    Timer {
        interval: 200
        running: true
        repeat: true
        onTriggered: {
            try {
                if (phase === 0 || phase === 6) {
                    if (phase === 0 && (settings.cfg_deviceInformationDetailLevel !== "standard"
                            || settings.cfg_showPhysicalDeviceRenamingControls !== false))
                        throw new Error("Incorrect General settings defaults")
                    for (var i = 0; i < list.count; ++i) list.itemAtIndex(i).expanded = true
                } else if (phase === 1) {
                    verify()
                    settings.cfg_deviceInformationDetailLevel = "extended"
                } else if (phase === 2) {
                    verify()
                    settings.cfg_deviceInformationDetailLevel = "debug"
                    settings.cfg_showPhysicalDeviceRenamingControls = true
                } else if (phase === 3) {
                    verify()
                    var rootItems = descendants(list.itemAtIndex(0))
                    var actions = rootItems.filter(function(item) { return item.objectName === "physicalAliasActions" })[0]
                    actions.children.filter(function(item) { return item.text === "Rename physical device…" })[0].clicked()
                } else if (phase === 4) {
                    verify()
                    settings.cfg_deviceInformationDetailLevel = "standard"
                    settings.cfg_showPhysicalDeviceRenamingControls = false
                } else if (phase === 5) {
                    verify()
                    testDeviceModel.showTopologyTree = true
                    window.width = Kirigami.Units.gridUnit * 16
                } else if (phase === 7) {
                    verify()
                    settings.cfg_deviceInformationDetailLevel = "debug"
                } else {
                    verify()
                    console.warn("PASS detail levels, alias controls, production DeviceModel/ListView and narrow tree layout")
                    Qt.quit()
                }
                phase++
            } catch (error) {
                console.error(error)
                Qt.exit(1)
            }
        }
    }
}
