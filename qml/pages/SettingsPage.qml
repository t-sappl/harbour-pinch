import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Settings") }

            SectionHeader { text: qsTr("Connection") }

            TextSwitch {
                text: qsTr("Connect earbuds on start")
                description: qsTr("When the app starts and your earbuds are not connected, Pinch connects them. Take them out of the case first.")
                checked: ear.autoConnectAudio
                automaticCheck: false
                onClicked: ear.autoConnectAudio = !checked
            }

            TextSwitch {
                id: autoChannel
                text: qsTr("Find control channel automatically")
                description: qsTr("Recommended. Turn off only if connecting fails with “control service not found”.")
                checked: ear.manualChannel === 0
                automaticCheck: false
                onClicked: ear.manualChannel = checked ? 1 : 0
            }

            Slider {
                visible: !autoChannel.checked
                width: parent.width
                label: qsTr("RFCOMM channel")
                minimumValue: 1
                maximumValue: 30
                stepSize: 1
                value: Math.max(1, ear.manualChannel)
                valueText: value
                onReleased: ear.manualChannel = value
            }

            Label {
                visible: !autoChannel.checked
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("On a PC you can find the channel with: sdptool search --bdaddr <address> SP")
            }
        }
    }
}
