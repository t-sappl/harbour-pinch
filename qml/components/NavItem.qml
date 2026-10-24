import QtQuick 2.0
import Sailfish.Silica 1.0

// List entry that opens a sub page.
BackgroundItem {
    id: root

    property string text
    property string description

    width: parent.width
    height: Math.max(Theme.itemSizeSmall, column.height + 2 * Theme.paddingMedium)

    Column {
        id: column
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            right: arrow.left
            rightMargin: Theme.paddingMedium
            verticalCenter: parent.verticalCenter
        }

        Label {
            width: parent.width
            truncationMode: TruncationMode.Fade
            text: root.text
            color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
        }

        Label {
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            text: root.description
            color: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        }
    }

    Icon {
        id: arrow
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        source: "image://theme/icon-m-right"
        highlighted: root.highlighted
    }
}
