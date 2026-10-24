import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.pinch 1.0
import "../components"

Page {
    id: page

    property int side: Ear.SideLeft
    // Bumped whenever the buds report gestures, re-evaluates the combos.
    property int revision: 0

    readonly property var noiseVariants: [10, 20, 22, 21]

    function typeName(type) {
        switch (type) {
        case 2: return qsTr("Double pinch")
        case 3: return qsTr("Triple pinch")
        case 7: return qsTr("Pinch and hold")
        case 9: return qsTr("Double pinch and hold")
        }
        return qsTr("Gesture %1").arg(type)
    }

    function actionName(action) {
        switch (action) {
        case 1:  return qsTr("No action")
        case 8:  return qsTr("Previous track")
        case 9:  return qsTr("Next track")
        case 10: return qsTr("Noise control")
        case 11: return qsTr("Voice assistant")
        case 18: return qsTr("Volume up")
        case 19: return qsTr("Volume down")
        case 29: return qsTr("Microphone on/off")
        }
        return qsTr("Action %1").arg(action)
    }

    function variantName(action) {
        switch (action) {
        case 10: return qsTr("Noise cancellation, transparency, off")
        case 20: return qsTr("Noise cancellation and off")
        case 22: return qsTr("Noise cancellation and transparency")
        case 21: return qsTr("Transparency and off")
        }
        return ""
    }

    Connections {
        target: ear
        onGesturesChanged: page.revision++
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
                title: qsTr("Gestures")
                description: ear.deviceName
            }

            ChoiceRow {
                currentValue: page.side
                options: [
                    { value: Ear.SideLeft, label: qsTr("Left") },
                    { value: Ear.SideRight, label: qsTr("Right") }
                ]
                onChosen: page.side = value
            }

            Repeater {
                model: ear.gestureTypes()

                Column {
                    id: slot

                    readonly property int type: modelData
                    readonly property var actions: (page.revision, ear.gestureActions(page.side, type))
                    readonly property int current: (page.revision, ear.gestureAction(page.side, type))
                    readonly property bool noiseControl: ear.isNoiseControlAction(current)

                    width: column.width

                    ComboBox {
                        width: parent.width
                        label: page.typeName(slot.type)
                        automaticSelection: false
                        currentIndex: slot.actions.indexOf(slot.noiseControl ? 10 : slot.current)
                        // Built from the reported action, not from currentItem:
                        // without automatic selection currentItem can lag behind.
                        value: slot.current < 0 ? qsTr("Unknown")
                                                : page.actionName(slot.noiseControl ? 10 : slot.current)

                        menu: ContextMenu {
                            Repeater {
                                model: slot.actions
                                MenuItem {
                                    text: page.actionName(modelData)
                                    onClicked: {
                                        // Keep the chosen noise control variant.
                                        if (modelData === 10 && slot.noiseControl)
                                            return
                                        ear.setGestureAction(page.side, slot.type, modelData)
                                    }
                                }
                            }
                        }
                    }

                    ComboBox {
                        visible: slot.noiseControl
                        width: parent.width
                        label: qsTr("Switches between")
                        automaticSelection: false
                        currentIndex: page.noiseVariants.indexOf(slot.current)
                        value: page.variantName(slot.current)

                        menu: ContextMenu {
                            Repeater {
                                model: page.noiseVariants
                                MenuItem {
                                    text: page.variantName(modelData)
                                    onClicked: ear.setGestureAction(page.side, slot.type, modelData)
                                }
                            }
                        }
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Changes are saved on the earbuds right away. Double pinch also declines incoming calls.")
            }
        }

        VerticalScrollDecorator { }
    }
}
