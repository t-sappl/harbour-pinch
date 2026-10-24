import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.pinch 1.0

Page {
    id: page

    readonly property bool running: ear.fitState === Ear.FitRunning
    readonly property bool done: ear.fitState === Ear.FitDone

    function resultText(value) {
        switch (value) {
        case 0: return qsTr("Good seal")
        case 1: return qsTr("Adjust")
        case 2: return qsTr("Not detected")
        }
        return "–"
    }

    function resultColor(value) {
        return value === 0 ? Theme.highlightColor
                           : (value === 1 ? Theme.secondaryHighlightColor : Theme.errorColor)
    }

    function adviceText() {
        if (ear.fitState === Ear.FitNoResult)
            return qsTr("The earbuds did not send a result. Try again.")
        if (!done)
            return ""
        var l = ear.fitLeft, r = ear.fitRight
        if (l === 2 || r === 2)
            return qsTr("Make sure both earbuds are connected and in your ears.")
        if (l === 0 && r === 0)
            return qsTr("Both ear tips fit well.")
        if (l === 1 && r === 1)
            return qsTr("Adjust both earbuds or try another ear tip size, then test again.")
        if (l === 1)
            return qsTr("Adjust the left earbud or try another ear tip size, then test again.")
        return qsTr("Adjust the right earbud or try another ear tip size, then test again.")
    }

    Component.onCompleted: ear.resetFitTest()
    Component.onDestruction: ear.resetFitTest()

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
                title: qsTr("Ear tip fit test")
                description: ear.deviceName
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("Put both earbuds in your ears and start the test. The earbuds play a short sound; stay quiet while it runs.")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: !page.running
                text: ear.fitState === Ear.FitIdle ? qsTr("Start test") : qsTr("Test again")
                onClicked: ear.startFitTest()
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusySize.Medium
                running: page.running
                visible: running
            }

            Row {
                visible: page.done
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin

                Repeater {
                    model: [
                        { label: qsTr("Left"), value: ear.fitLeft },
                        { label: qsTr("Right"), value: ear.fitRight }
                    ]

                    Column {
                        width: parent.width / 2
                        spacing: Theme.paddingSmall

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.secondaryHighlightColor
                            text: modelData.label
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeLarge
                            color: page.resultColor(modelData.value)
                            text: page.resultText(modelData.value)
                        }
                    }
                }
            }

            Label {
                visible: text.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.secondaryHighlightColor
                text: page.adviceText()
            }
        }
    }
}
