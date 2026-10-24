import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.pinch 1.0

Page {
    id: page

    readonly property int maxConnected: 2
    readonly property int connectedCount: {
        var n = 0
        for (var i = 0; i < ear.dualDevices.length; ++i)
            if (ear.dualDevices[i].connected)
                n++
        return n
    }

    Component.onCompleted: ear.refreshDualDevices()

    Connections {
        target: ear
        onConnectionStateChanged: {
            if (ear.connectionState !== Ear.Connected)
                pageStack.pop()
        }
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: ear.dualDevices

        header: Column {
            width: list.width

            PageHeader {
                title: qsTr("Dual connection")
                description: ear.deviceName
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("%1 of %2 devices connected").arg(page.connectedCount).arg(page.maxConnected)
            }

            Item { width: 1; height: Theme.paddingMedium }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: ear.refreshDualDevices()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No remembered devices")
            hintText: qsTr("Devices the earbuds were paired with appear here")
        }

        delegate: TextSwitch {
            width: list.width
            text: modelData.self ? qsTr("This phone") : (modelData.name.length > 0 ? modelData.name
                                                                                    : qsTr("Unknown device"))
            description: (modelData.connected ? qsTr("Connected") : qsTr("Not connected"))
                         + " · " + modelData.mac
            checked: modelData.connected
            automaticCheck: false
            // Disconnecting this phone would cut off the app itself, and only
            // two devices can be connected at a time.
            enabled: !modelData.self && (modelData.connected || page.connectedCount < page.maxConnected)
            onClicked: ear.setDualDeviceConnected(modelData.mac, !checked)
        }

        VerticalScrollDecorator { }
    }
}
