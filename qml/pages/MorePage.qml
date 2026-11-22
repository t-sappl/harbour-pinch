import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.pinch 1.0
import "../components"

Page {
    id: page

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
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("More")
                description: ear.deviceName
            }

            NavItem {
                text: qsTr("Find my earbuds")
                description: qsTr("Let an earbud play a loud sound")
                onClicked: pageStack.push(Qt.resolvedUrl("FindPage.qml"))
            }

            NavItem {
                visible: ear.hasFitTest
                text: qsTr("Ear tip fit test")
                description: qsTr("Check whether the ear tips seal well")
                onClicked: pageStack.push(Qt.resolvedUrl("FitTestPage.qml"))
            }

            SectionHeader {
                visible: ear.firmwareVersion.length > 0 || ear.serialNumber.length > 0
                text: qsTr("Info")
            }

            DetailItem {
                visible: ear.detectedModel.length > 0
                label: qsTr("Model")
                value: ear.detectedModel
            }

            DetailItem {
                visible: ear.firmwareVersion.length > 0
                label: qsTr("Firmware")
                value: ear.firmwareVersion
            }

            DetailItem {
                visible: ear.serialNumber.length > 0
                label: qsTr("Serial number")
                value: ear.serialNumber
            }
        }

        VerticalScrollDecorator { }
    }
}
