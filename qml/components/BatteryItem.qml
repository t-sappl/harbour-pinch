import QtQuick 2.0
import Sailfish.Silica 1.0

// One battery gauge: caption, percentage, thin level bar, charging hint.
// level is -1 when the component is not reported (out of range, in the
// closed case, ...).
Column {
    id: root

    property string label
    property int level: -1
    property bool charging: false

    readonly property bool known: level >= 0
    readonly property bool low: known && level <= 15 && !charging

    spacing: Theme.paddingSmall

    Label {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryHighlightColor
        text: root.label
    }

    Label {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: Theme.fontSizeLarge
        color: !root.known ? Theme.secondaryColor
                           : (root.low ? Theme.errorColor : Theme.highlightColor)
        text: root.known ? qsTr("%1%").arg(root.level) : "–"
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.6
        height: Theme.paddingSmall
        radius: height / 2
        color: Theme.rgba(Theme.primaryColor, 0.15)

        Rectangle {
            width: root.known ? parent.width * Math.min(root.level, 100) / 100 : 0
            height: parent.height
            radius: parent.radius
            color: root.low ? Theme.errorColor : Theme.highlightColor
        }
    }

    Label {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: Theme.fontSizeTiny
        color: Theme.secondaryColor
        // Keep the line so the gauges do not jump when charging starts.
        text: root.charging ? qsTr("charging") : " "
    }
}
