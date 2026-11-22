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
                title: qsTr("Equalizer")
                description: ear.deviceName
            }

            ChoiceRow {
                currentValue: ear.eqMode
                options: [
                    { value: Ear.EqBalanced, label: qsTr("Balanced") },
                    { value: Ear.EqVoice, label: qsTr("Voice") },
                    { value: Ear.EqTreble, label: qsTr("Treble") },
                    { value: Ear.EqBass, label: qsTr("Bass") },
                    { value: Ear.EqCustom, label: qsTr("Custom") }
                ]
                onChosen: ear.setEqMode(value)
            }

            Column {
                width: parent.width
                spacing: Theme.paddingMedium
                visible: ear.eqMode === Ear.EqCustom && ear.hasCustomEq

                Slider {
                    id: bassSlider
                    width: parent.width
                    label: qsTr("Bass")
                    minimumValue: -6.0
                    maximumValue: 6.0
                    stepSize: 0.5
                    value: ear.eqBass
                    valueText: value.toFixed(1)
                    onReleased: ear.setCustomEq(value, midSlider.value, trebleSlider.value)
                }

                Slider {
                    id: midSlider
                    width: parent.width
                    label: qsTr("Mid")
                    minimumValue: -6.0
                    maximumValue: 6.0
                    stepSize: 0.5
                    value: ear.eqMid
                    valueText: value.toFixed(1)
                    onReleased: ear.setCustomEq(bassSlider.value, value, trebleSlider.value)
                }

                Slider {
                    id: trebleSlider
                    width: parent.width
                    label: qsTr("Treble")
                    minimumValue: -6.0
                    maximumValue: 6.0
                    stepSize: 0.5
                    value: ear.eqTreble
                    valueText: value.toFixed(1)
                    onReleased: ear.setCustomEq(bassSlider.value, midSlider.value, value)
                }
            }

            Label {
                visible: ear.eqMode === Ear.EqCustom && !ear.hasCustomEq
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("These earbuds did not answer the custom EQ request.")
            }
        }

        VerticalScrollDecorator { }
    }
}
