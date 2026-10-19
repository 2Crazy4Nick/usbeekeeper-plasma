import QtQuick
import QtQuick.Controls
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: generalSettings
    property alias cfg_showHubs: showHubs.checked
    property alias cfg_showEmptyPorts: showEmptyPorts.checked
    property alias cfg_showTopologyTree: showTopologyTree.checked
    property string cfg_deviceInformationDetailLevel: "standard"
    property alias cfg_showPhysicalDeviceRenamingControls: showPhysicalDeviceRenamingControls.checked

    Kirigami.FormLayout {
        CheckBox {
            id: showHubs
            text: i18n("Show USB hubs")
        }

        CheckBox {
            id: showTopologyTree
            text: i18n("Show as topology tree")
            enabled: showHubs.checked
            leftPadding: Kirigami.Units.gridUnit
        }

        CheckBox {
            id: showEmptyPorts
            text: i18n("Show empty USB-C ports")
        }

        ComboBox {
            id: detailLevel
            objectName: "deviceInformationDetailLevel"
            Kirigami.FormData.label: i18n("Device information detail level:")
            textRole: "label"
            valueRole: "value"
            model: [
                {label: i18n("Standard"), value: "standard"},
                {label: i18n("Extended"), value: "extended"},
                {label: i18n("Debug"), value: "debug"}
            ]
            currentIndex: Math.max(0, indexOfValue(generalSettings.cfg_deviceInformationDetailLevel))
            onActivated: generalSettings.cfg_deviceInformationDetailLevel = currentValue
        }

        CheckBox {
            id: showPhysicalDeviceRenamingControls
            text: i18n("Show physical device renaming controls")
        }
    }
}
