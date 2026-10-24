import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.pinch 1.0

Page {
    id: page

    // Never leave the buds ringing behind the user's back.
    onStatusChanged: {
        if (status === PageStatus.Deactivating)
            ear.stopRinging()
    }
    Component.onDestruction: ear.stopRinging()

    Connections {
        target: ear
        onConnectionStateChanged: {
            if (ear.connectionState !== Ear.Connected)
                pageStack.pop()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Find my earbuds")
                description: ear.deviceName
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("The earbud plays a loud sound. Take the earbuds out of your ears first.")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 4 * Theme.horizontalPageMargin
                enabled: ear.batteryLeft >= 0 || ear.ringingLeft
                text: ear.ringingLeft ? qsTr("Stop left") : qsTr("Ring left")
                onClicked: ear.ring(Ear.SideLeft, !ear.ringingLeft)
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 4 * Theme.horizontalPageMargin
                enabled: ear.batteryRight >= 0 || ear.ringingRight
                text: ear.ringingRight ? qsTr("Stop right") : qsTr("Ring right")
                onClicked: ear.ring(Ear.SideRight, !ear.ringingRight)
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Only earbuds that are connected can ring. The sound stops when you leave this page.")
            }
        }
    }
}
