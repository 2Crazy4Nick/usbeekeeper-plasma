import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

ColumnLayout {
    id: card
    spacing: Kirigami.Units.smallSpacing

    property bool expanded: false
    property bool topologyTreeActive: false
    property real treeIndentStep: Kirigami.Units.gridUnit * 1.25

    Item {
        id: cardRow
        Layout.fillWidth: true
        implicitHeight: cardRectangle.implicitHeight

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                id: topologyGuides
                Layout.fillHeight: true
                Layout.preferredWidth: card.topologyTreeActive
                    ? (model.topologyDepth || 0) * card.treeIndentStep : 0
                property var ancestorLastFlags: model.topologyAncestorLastFlags || []
                property int depth: model.topologyDepth || 0

                Repeater {
                    model: card.topologyTreeActive ? topologyGuides.depth : 0

                    delegate: Item {
                        width: card.treeIndentStep
                        height: topologyGuides.height

                        Rectangle {
                            width: 1
                            height: (topologyGuides.ancestorLastFlags[index] || false)
                                ? parent.height / 2 : parent.height
                            x: (parent.width - width) / 2
                            color: Kirigami.Theme.textColor
                            opacity: 0.22
                        }
                    }
                }

                Rectangle {
                    visible: card.topologyTreeActive && topologyGuides.depth > 0
                    width: card.treeIndentStep
                    height: 1
                    x: (topologyGuides.depth - 1) * card.treeIndentStep + card.treeIndentStep / 2
                    y: topologyGuides.height / 2
                    color: Kirigami.Theme.textColor
                    opacity: 0.22
                }
            }

            Rectangle {
                id: cardRectangle
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                radius: Kirigami.Units.cornerRadius
                color: {
                    if (model.category === "typec")
                        return Qt.rgba(Kirigami.Theme.highlightColor.r,
                                      Kirigami.Theme.highlightColor.g,
                                      Kirigami.Theme.highlightColor.b, 0.08)
                    return Qt.rgba(Kirigami.Theme.textColor.r,
                                  Kirigami.Theme.textColor.g,
                                  Kirigami.Theme.textColor.b, 0.04)
                }
                implicitHeight: cardContent.implicitHeight + Kirigami.Units.smallSpacing * 2

                ColumnLayout {
                    id: cardContent
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

            // Header row
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: model.iconName || "drive-removable-media-usb"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    PlasmaComponents.Label {
                        text: model.displayName || model.headline || ""
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    PlasmaComponents.Label {
                        text: model.isCompanionHubGroup ? (model.subtitle || "") : (model.displayVendor || model.subtitle || "")
                        opacity: 0.7
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        visible: text.length > 0
                    }

                    PlasmaComponents.Label {
                        text: model.linkSummary || ""
                        color: (model.dataRateWarning || false) ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                        opacity: (model.dataRateWarning || false) ? 1.0 : 0.7
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        visible: text.length > 0
                    }
                }

                // Expand/collapse button
                PlasmaComponents.ToolButton {
                    icon.name: card.expanded ? "arrow-up" : "arrow-down"
                    onClicked: card.expanded = !card.expanded
                    implicitWidth: Kirigami.Units.iconSizes.small * 2
                    implicitHeight: Kirigami.Units.iconSizes.small * 2
                }
            }

            // Link diagnostics (shown when expanded)
            ColumnLayout {
                Layout.fillWidth: true
                visible: card.expanded && (!(model.hasAltmode || false) || (model.dataRateWarning || false))
                         && ((model.dataRateSummary || "").length > 0
                                           || (model.dataRateDetail || "").length > 0)
                spacing: 0

                PlasmaComponents.Label {
                    text: model.dataRateSummary || ""
                    color: (model.dataRateWarning || false) ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                    font.bold: true
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                PlasmaComponents.Label {
                    text: model.dataRateDetail || ""
                    opacity: 0.7
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    visible: text.length > 0
                }
            }

            // Charging diagnostic banner
            Rectangle {
                Layout.fillWidth: true
                visible: model.hasChargingDiag || false
                radius: Kirigami.Units.cornerRadius
                color: (model.chargingIsWarning || false)
                    ? Qt.rgba(1.0, 0.8, 0.0, 0.15)
                    : Qt.rgba(0.0, 0.8, 0.0, 0.15)
                implicitHeight: chargingRow.implicitHeight + Kirigami.Units.smallSpacing * 2

                RowLayout {
                    id: chargingRow
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: (model.chargingIsWarning || false) ? "dialog-warning" : "dialog-positive"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        PlasmaComponents.Label {
                            text: model.chargingSummary || ""
                            font.bold: true
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }

                        PlasmaComponents.Label {
                            text: model.chargingDetail || ""
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                            visible: text.length > 0
                        }
                    }
                }
            }

            // Curated usbeehive details (shown when expanded)
            ColumnLayout {
                Layout.fillWidth: true
                visible: card.expanded
                spacing: 2

                Repeater {
                    model: model.details || []

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            text: modelData.label + ":"
                            opacity: 0.5
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 7
                        }

                        PlasmaComponents.Label {
                            text: modelData.value
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // PDO list (shown when expanded, USB-C only)
            PowerSourceList {
                Layout.fillWidth: true
                visible: card.expanded && (model.hasPowerDelivery || false)
                pdos: model.sourcePdos || []
            }
                }
            }
        }
    }
}
