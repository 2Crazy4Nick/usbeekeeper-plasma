import QtQuick
import QtQuick.Controls
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    property alias cfg_showHubs: showHubs.checked
    property alias cfg_showEmptyPorts: showEmptyPorts.checked
    property alias cfg_showTopologyTree: showTopologyTree.checked

    Kirigami.FormLayout {
        CheckBox {
            id: showHubs
            text: i18n("Show USB hubs")
        }

        CheckBox {
            id: showTopologyTree
            text: i18n("Show as topology tree")
            enabled: showHubs.checked
            Layout.leftMargin: Kirigami.Units.gridUnit
        }

        CheckBox {
            id: showEmptyPorts
            text: i18n("Show empty USB-C ports")
        }
    }
}
