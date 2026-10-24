import QtQuick 2.0
import Sailfish.Silica 1.0

// A row of equally wide, selectable tiles (segmented control).
// options: [{ value: <int>, label: <string> }]
Row {
    id: root

    property var options: []
    property int currentValue: -1
    property bool tall: false

    signal chosen(int value)

    x: Theme.horizontalPageMargin
    width: parent.width - 2 * Theme.horizontalPageMargin
    spacing: Theme.paddingSmall

    Repeater {
        model: root.options

        BackgroundItem {
            id: tile

            readonly property bool selected: modelData.value === root.currentValue

            width: (root.width - (root.options.length - 1) * root.spacing) / root.options.length
            height: root.tall ? Theme.itemSizeExtraLarge : Theme.itemSizeMedium
            highlighted: down || selected
            enabled: root.enabled

            onClicked: root.chosen(modelData.value)

            Rectangle {
                anchors.fill: parent
                radius: Theme.paddingSmall
                color: "transparent"
                border.width: tile.selected ? 2 : 1
                border.color: tile.selected ? Theme.highlightColor
                                            : Theme.rgba(Theme.primaryColor, 0.2)
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.paddingSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: root.tall ? Theme.fontSizeMedium : Theme.fontSizeSmall
                text: modelData.label
                color: tile.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }
    }
}
