import QtQuick
import QtQuick.Controls
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    property alias cfg_showHubs: showHubs.checked
    property alias cfg_showEmptyPorts: showEmptyPorts.checked

    Kirigami.FormLayout {
        CheckBox {
            id: showHubs
            text: i18n("Show USB hubs")
        }

        CheckBox {
            id: showEmptyPorts
            text: i18n("Show empty USB-C ports")
        }
    }
}
