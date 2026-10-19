import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import "../../src/plasmoid/contents/ui" as UI

Window {
    id: window
    visible: true
    width: Kirigami.Units.gridUnit * 26
    height: Kirigami.Units.gridUnit * 60
    property int phase: 0
    property real collapsedHeight: 0

    UI.DeviceCard {
        id: card
        width: Kirigami.Units.gridUnit * 24
        height: implicitHeight
        informationDetailLevel: "debug"
        property var model: ({
            physicalName: "", physicalRelationshipType: "integrated_function",
            physicalRelationshipSource: "profile", category: "usb",
            iconName: "drive-removable-media-usb", displayName: "Example USB device",
            displayVendor: "Example vendor", headline: "Example USB device", subtitle: "Example vendor",
            linkSummary: "USB 3.0 · 5 Gbps", dataRateWarning: false,
            hasAltmode: false, hasChargingDiag: false, hasPowerDelivery: false,
            physicalInstanceKey: "", presentationTreeDepth: 3,
            presentationTreeAncestorLastFlags: [true, false, false], presentationTreeIsLastChild: true,
            deviceInformation: [
                {title: "USB 2 function", rows: [
                    {label: "VID:PID", value: "1234:0001", emphasis: true, detailLevel: 0},
                    {label: "Declared max USB current", value: "500 mA", detailLevel: 0},
                    {label: "Kernel driver", value: "long-example-driver-name-without-spaces", detailLevel: 0},
                    {label: "Serial number", value: "example-serial-value-with-a-very-long-suffix", detailLevel: 1}
                ]},
                {title: "USB 3 function", rows: [
                    {label: "VID:PID", value: "1234:0002", emphasis: true, detailLevel: 0},
                    {label: "Device class", value: "Hub", detailLevel: 1}
                ]}
            ],
            dataRateSummary: "Example diagnostic with enough words to wrap at narrow widths.",
            dataRateDetail: "Example diagnostic detail.",
            details: [{label: "USB database", value: "Long example database name for a USB controller", detailLevel: 2},
                      {label: "Port", value: "1-1-port4", detailLevel: 2}]
        })
        // Plasma supplies the translation function in the real applet context.
        function i18n(text) { return text }
    }

    function descendants(item) {
        var result = [item]
        for (var i = 0; i < item.children.length; ++i)
            result = result.concat(descendants(item.children[i]))
        return result
    }

    function verify() {
        var items = descendants(card)
        var info = items.filter(function(item) { return item.objectName === "deviceInformation" })[0]
        if (!info || !info.visible) throw new Error("Primary information is missing")
        var rows = items.filter(function(item) { return item.objectName === "deviceDetailRow" })
        if (rows.length !== 8) throw new Error("Expected eight primary/advanced rows; got " + rows.length)
        rows.forEach(function(row) {
            if (!(row.width > 0) || !(row.height > 0)) throw new Error("Empty detail row")
            var origin = row.mapToItem(card, 0, 0)
            if (origin.x < -1 || origin.x + row.width > card.width + 1)
                throw new Error("Detail row extends beyond card")
            row.children.forEach(function(label) {
                if (label.text === undefined) return
                if (label.truncated || label.height + 1 < label.implicitHeight)
                    throw new Error("Truncated detail: " + label.text)
                if (label.x < -1 || label.x + label.width > row.width + 1
                        || label.y < -1 || label.y + label.height > row.height + 1)
                    throw new Error("Detail extends beyond row: " + label.text)
            })
        })
        if (!rows.some(function(row) { return row.labelText === "Logical downstream port" }))
            throw new Error("Integrated-function port semantics lost")
        console.warn("PASS layout phase", phase)
    }

    Timer {
        interval: 250
        repeat: true
        running: true
        onTriggered: {
            try {
                if (phase === 0) {
                    collapsedHeight = card.height
                    card.expanded = true
                } else if (phase === 1) {
                    verify()
                    card.width = Kirigami.Units.gridUnit * 16
                    card.topologyTreeActive = true
                } else if (phase === 2) {
                    verify()
                    card.expanded = false
                    card.topologyTreeActive = false
                    card.width = Kirigami.Units.gridUnit * 24
                } else {
                    if (Math.abs(card.height - collapsedHeight) > 1)
                        throw new Error("Collapsed height changed after expansion")
                    console.warn("PASS collapsed height")
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
