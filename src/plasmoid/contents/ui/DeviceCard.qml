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
    property bool editingPhysicalAlias: false
    property string physicalAliasDraft: model.physicalName || ""
    readonly property var deviceData: model

    signal physicalAliasSubmitted(string key, string alias)
    signal physicalAliasCleared(string key)

    // Reuse the same responsive row for primary information and advanced details.
    component DetailRow: GridLayout {
        id: detailRow
        objectName: "deviceDetailRow"
        property string labelText
        property string valueText
        property bool emphasis: false
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        columns: width < Kirigami.Units.gridUnit * 14 ? 1 : 2
        columnSpacing: Kirigami.Units.smallSpacing
        rowSpacing: Kirigami.Units.smallSpacing / 2

        PlasmaComponents.Label {
            text: detailRow.labelText + ":"
            textFormat: Text.PlainText
            opacity: 0.65
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.maximumWidth: detailRow.columns === 2 ? Kirigami.Units.gridUnit * 7 : Infinity
            Layout.preferredWidth: detailRow.columns === 2 ? Kirigami.Units.gridUnit * 7 : -1
        }

        PlasmaComponents.Label {
            text: detailRow.valueText
            textFormat: Text.PlainText
            font.bold: detailRow.emphasis
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
            Layout.minimumWidth: 0
        }
    }

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
                    ? (model.presentationTreeDepth || 0) * card.treeIndentStep : 0
                property var ancestorLastFlags: card.topologyTreeActive
                    ? (model.presentationTreeAncestorLastFlags || [])
                    : (model.topologyAncestorLastFlags || [])
                property int depth: card.topologyTreeActive
                    ? (model.presentationTreeDepth || 0) : (model.topologyDepth || 0)

                Repeater {
                    // The final column belongs to this node's elbow.  The
                    // preceding columns only continue ancestors that still
                    // have later siblings.
                    model: card.topologyTreeActive ? Math.max(0, topologyGuides.depth - 1) : 0

                    delegate: Item {
                        x: index * card.treeIndentStep
                        width: card.treeIndentStep
                        height: topologyGuides.height

                        Rectangle {
                            // ancestorLastFlags also contains the root.  A
                            // guide column represents the next ancestor down
                            // the chain, hence the offset by one here.
                            visible: !(topologyGuides.ancestorLastFlags[index + 1] || false)
                            width: 1
                            height: parent.height
                            x: (parent.width - width) / 2
                            color: Kirigami.Theme.textColor
                            opacity: 0.22
                        }
                    }
                }

                Rectangle {
                    visible: card.topologyTreeActive && topologyGuides.depth > 0
                    width: 1
                    height: (model.presentationTreeIsLastChild || false)
                        ? topologyGuides.height / 2 : topologyGuides.height
                    x: (topologyGuides.depth - 1) * card.treeIndentStep
                        + (card.treeIndentStep - width) / 2
                    color: Kirigami.Theme.textColor
                    opacity: 0.22
                }

                Rectangle {
                    visible: card.topologyTreeActive && topologyGuides.depth > 0
                    width: card.treeIndentStep / 2
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
                    Layout.minimumWidth: 0
                    spacing: 0

                    PlasmaComponents.Label {
                        id: titleLabel
                        text: (card.topologyTreeActive
                               && model.physicalRelationshipType === "root"
                               && (model.physicalName || "").length > 0)
                            ? model.physicalName
                            : (model.displayName || model.headline || "")
                        font.bold: true
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        Layout.fillWidth: true

                        HoverHandler {
                            id: titleHoverHandler
                        }

                        PlasmaComponents.ToolTip {
                            text: titleLabel.text
                            visible: titleLabel.truncated && titleHoverHandler.hovered
                        }
                    }

                    PlasmaComponents.Label {
                        id: subtitleLabel
                        text: {
                            if (card.topologyTreeActive && model.physicalRelationshipType === "root"
                                    && (model.physicalName || "").length > 0) {
                                var logical = model.displayName || ""
                                var vendor = model.displayVendor || ""
                                var transport = model.isCompanionHubGroup ? "USB 2 + USB 3" : ""
                                var result = vendor
                                if (logical.length > 0) result += (result.length > 0 ? " · " : "") + logical
                                if (transport.length > 0) result += (result.length > 0 ? " · " : "") + transport
                                return result
                            }
                            var subtitle = model.isCompanionHubGroup ? (model.subtitle || "") : (model.displayVendor || model.subtitle || "")
                            if (model.physicalRelationshipType === "integrated_function")
                                subtitle += (subtitle.length > 0 ? " · " : "") + "Integrated"
                            return subtitle
                        }
                        opacity: 0.7
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        visible: text.length > 0

                        HoverHandler {
                            id: subtitleHoverHandler
                        }

                        PlasmaComponents.ToolTip {
                            text: subtitleLabel.text
                            visible: subtitleLabel.truncated && subtitleHoverHandler.hovered
                        }
                    }

                    PlasmaComponents.Label {
                        text: model.linkSummary || ""
                        color: (model.dataRateWarning || false) ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                        opacity: (model.dataRateWarning || false) ? 1.0 : 0.7
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        wrapMode: Text.Wrap
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

            // Primary technical identity, before the more detailed diagnostics.
            ColumnLayout {
                id: deviceInformation
                objectName: "deviceInformation"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: card.expanded && (card.deviceData.deviceInformation || []).length > 0
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Separator {
                    Layout.fillWidth: true
                }

                PlasmaComponents.Label {
                    text: i18n("Device Information")
                    font.bold: true
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                Repeater {
                    model: card.deviceData.deviceInformation || []

                    ColumnLayout {
                        required property var modelData
                        id: informationSection
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            text: informationSection.modelData.title
                            textFormat: Text.PlainText
                            visible: text.length > 0
                            font.bold: true
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        Repeater {
                            model: informationSection.modelData.rows

                            DetailRow {
                                required property var modelData
                                labelText: modelData.label
                                valueText: modelData.value
                                emphasis: modelData.emphasis || false
                            }
                        }
                    }
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
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        PlasmaComponents.Label {
                            text: model.chargingDetail || ""
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
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

                PlasmaComponents.Label {
                    text: "Integrated USB function of the parent physical device"
                    visible: model.physicalRelationshipType === "integrated_function"
                    opacity: 0.7
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                PlasmaComponents.Label {
                    text: "Relationship source: " + (model.physicalRelationshipSource || "")
                    visible: model.physicalRelationshipType === "integrated_function"
                             && (model.physicalRelationshipSource || "").length > 0
                    opacity: 0.7
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                Repeater {
                    model: card.deviceData.details || []

                    DetailRow {
                        required property var modelData
                        labelText: (card.deviceData.physicalRelationshipType === "integrated_function"
                                   && modelData.label === "Port")
                            ? "Logical downstream port" : modelData.label
                        valueText: modelData.value
                    }
                }
            }

            // Optional user-defined physical identity. This is separate from
            // the logical USB display identity and is intentionally compact.
            ColumnLayout {
                Layout.fillWidth: true
                visible: card.expanded && (model.physicalInstanceKey || "").length > 0
                         && !(model.physicalAliasIdentityAmbiguous || false)
                         && ((model.physicalRelationshipType || "").length === 0
                             || model.physicalRelationshipType === "root")
                spacing: 2

                PlasmaComponents.Label {
                    text: "Physical device name: " + (model.physicalName || "")
                    visible: (model.physicalName || "").length > 0
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }

                PlasmaComponents.Label {
                    text: "This name may be tied to the current USB port."
                    visible: (model.physicalInstanceKeySource || "") === "topology_fallback"
                    opacity: 0.7
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: card.editingPhysicalAlias

                    PlasmaComponents.TextField {
                        text: card.physicalAliasDraft
                        placeholderText: "Friendly physical-device name"
                        Layout.fillWidth: true
                        onTextChanged: card.physicalAliasDraft = text
                    }

                    PlasmaComponents.Button {
                        text: "Save"
                        onClicked: {
                            card.physicalAliasSubmitted(model.physicalInstanceKey, card.physicalAliasDraft)
                            card.editingPhysicalAlias = false
                        }
                    }

                    PlasmaComponents.Button {
                        text: "Cancel"
                        onClicked: card.editingPhysicalAlias = false
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !card.editingPhysicalAlias

                    PlasmaComponents.Button {
                        text: (model.physicalName || "").length > 0
                            ? "Rename physical device…" : "Name physical device…"
                        onClicked: {
                            card.physicalAliasDraft = model.physicalName || ""
                            card.editingPhysicalAlias = true
                        }
                    }

                    PlasmaComponents.Button {
                        text: "Clear"
                        visible: (model.physicalName || "").length > 0
                        onClicked: card.physicalAliasCleared(model.physicalInstanceKey)
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
