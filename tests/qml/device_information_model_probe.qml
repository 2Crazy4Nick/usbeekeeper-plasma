import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import io.github._2crazy4nick.usbeekeeper 1.0
import "../../src/plasmoid/contents/ui" as UI

Window {
    id: window
    width: Kirigami.Units.gridUnit * 26
    height: Kirigami.Units.gridUnit * 100
    visible: true
    property int phase: 0
    function i18n(text) { return text }

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
        }
    }

    function descendants(item) {
        var items = [item]
        for (var i = 0; i < item.children.length; ++i)
            items = items.concat(descendants(item.children[i]))
        return items
    }

    function verify() {
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
            if (rows.length !== (i === 0 ? 16 : 8))
                throw new Error("Missing rendered information rows: " + rows.length)
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
        }
    }

    Timer {
        interval: 200
        running: true
        repeat: true
        onTriggered: {
            try {
                if (phase === 0 || phase === 2) {
                    for (var i = 0; i < list.count; ++i) list.itemAtIndex(i).expanded = true
                } else if (phase === 1) {
                    verify()
                    testDeviceModel.showTopologyTree = true
                    window.width = Kirigami.Units.gridUnit * 16
                } else {
                    verify()
                    console.warn("PASS production DeviceModel/ListView, grouped functions and narrow tree layout")
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
