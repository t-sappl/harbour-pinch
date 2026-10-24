import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.pinch 1.0
import "../components"

Page {
    id: page

    readonly property bool connected: ear.connectionState === Ear.Connected

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            busy: ear.audioConnecting || ear.connectionState === Ear.Connecting

            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Choose earbuds")
                onClicked: pageStack.push(Qt.resolvedUrl("DevicesPage.qml"))
            }
            MenuItem {
                visible: ear.hasDevice
                enabled: !ear.audioConnecting
                text: page.connected ? qsTr("Disconnect")
                                     : (ear.systemConnected ? qsTr("Connect")
                                                            : qsTr("Connect earbuds"))
                onClicked: page.connected ? ear.disconnectDevice() : ear.connectNow()
            }
        }

        ViewPlaceholder {
            enabled: !ear.hasDevice
            text: qsTr("No earbuds chosen")
            hintText: qsTr("Pull down to choose your Nothing earbuds")
        }

        Column {
            id: column

            width: parent.width
            visible: ear.hasDevice
            spacing: Theme.paddingMedium

            PageHeader {
                title: ear.deviceName
                description: {
                    if (ear.audioConnecting)
                        return qsTr("Connecting earbuds…")
                    switch (ear.connectionState) {
                    case Ear.Connected:  return qsTr("Connected")
                    case Ear.Connecting: return qsTr("Connecting…")
                    default:             return qsTr("Not connected")
                    }
                }
            }

            // Device illustration with the model name the buds report.
            Column {
                width: parent.width
                spacing: Theme.paddingSmall

                FormFactorImage {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Theme.iconSizeExtraLarge * 1.5
                    height: width
                    formFactor: ear.formFactor
                    highlighted: page.connected
                    opacity: page.connected ? 1.0 : Theme.opacityHigh
                    Behavior on opacity { FadeAnimation { } }
                }

                Label {
                    visible: ear.modelName.length > 0 && ear.modelName !== ear.deviceName
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: ear.modelName
                }
            }

            Label {
                visible: text.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: ear.errorString
            }

            SectionHeader {
                visible: page.connected
                text: qsTr("Battery")
            }

            // Over-ear headphones: one battery.
            BatteryItem {
                visible: page.connected && ear.batterySingle >= 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Headphones")
                level: ear.batterySingle
                charging: ear.chargingSingle
            }

            Row {
                visible: page.connected && ear.batterySingle < 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin

                BatteryItem {
                    width: parent.width / 3
                    label: qsTr("Left")
                    level: ear.batteryLeft
                    charging: ear.chargingLeft
                }
                BatteryItem {
                    width: parent.width / 3
                    label: qsTr("Right")
                    level: ear.batteryRight
                    charging: ear.chargingRight
                }
                BatteryItem {
                    width: parent.width / 3
                    label: qsTr("Case")
                    level: ear.batteryCase
                    charging: ear.chargingCase
                }
            }

            Column {
                width: parent.width
                spacing: Theme.paddingMedium
                visible: ear.ancSupported

                SectionHeader { text: qsTr("Noise control") }

                ChoiceRow {
                    tall: true
                    enabled: page.connected
                    opacity: enabled ? 1.0 : Theme.opacityLow
                    currentValue: ear.ancMode
                    options: [
                        { value: Ear.ModeOff, label: qsTr("Off") },
                        { value: Ear.ModeTransparency, label: qsTr("Transparency") },
                        { value: Ear.ModeNoiseCancellation, label: qsTr("Noise cancellation") }
                    ]
                    onChosen: ear.setAncMode(value)
                }

                SectionHeader {
                    text: qsTr("Noise cancellation strength")
                }

                ChoiceRow {
                    enabled: page.connected
                    opacity: !enabled ? Theme.opacityLow
                                      : (ear.ancMode === Ear.ModeNoiseCancellation ? 1.0 : Theme.opacityHigh)
                    // Only highlight while noise cancellation is active, but keep
                    // the row usable: picking a strength switches ANC on.
                    currentValue: ear.ancMode === Ear.ModeNoiseCancellation ? ear.ancStrength : -1
                    options: [
                        { value: Ear.StrengthLow, label: qsTr("Low") },
                        { value: Ear.StrengthMid, label: qsTr("Mid") },
                        { value: Ear.StrengthHigh, label: qsTr("High") },
                        { value: Ear.StrengthAdaptive, label: qsTr("Adaptive") }
                    ]
                    onChosen: ear.setAncStrength(value)
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: qsTr("Adaptive adjusts the strength to your surroundings.")
                }

                TextSwitch {
                    visible: ear.hasPersonalizedAnc
                    text: qsTr("Personalized noise cancellation")
                    description: qsTr("Adapts noise cancellation to the shape of your ears.")
                    checked: ear.personalizedAnc
                    automaticCheck: false
                    onClicked: ear.setPersonalizedAnc(!checked)
                }
            }

            Label {
                visible: page.connected && !ear.ancSupported
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: qsTr("These earbuds have no noise control.")
            }

            // ----- Sound -----
            SectionHeader {
                visible: ear.hasEnhancedBass || ear.hasSpatialAudio
                text: qsTr("Sound")
            }

            TextSwitch {
                visible: ear.hasEnhancedBass
                text: qsTr("Bass boost")
                description: ear.bassSpatialExclusive && ear.hasSpatialAudio
                             ? qsTr("Not at the same time as spatial audio.") : ""
                checked: ear.enhancedBass
                automaticCheck: false
                onClicked: ear.setEnhancedBass(!checked, ear.bassLevel)
            }

            Slider {
                visible: ear.hasEnhancedBass && ear.enhancedBass
                width: parent.width
                label: qsTr("Bass boost level")
                minimumValue: 1
                maximumValue: 5
                stepSize: 1
                value: ear.bassLevel
                valueText: value
                onReleased: ear.setEnhancedBass(true, value)
            }

            TextSwitch {
                visible: ear.hasSpatialAudio
                text: qsTr("Spatial audio")
                description: ear.bassSpatialExclusive && ear.hasEnhancedBass
                             ? qsTr("Not at the same time as bass boost. The earbuds cannot report this setting, the switch shows what you set last.")
                             : qsTr("The earbuds cannot report this setting, the switch shows what you set last.")
                checked: ear.spatialAudio
                automaticCheck: false
                onClicked: ear.setSpatialAudio(!checked)
            }

            // ----- Controls -----
            SectionHeader {
                visible: ear.hasInEarDetection || ear.hasGestures
                text: qsTr("Controls")
            }

            TextSwitch {
                visible: ear.hasInEarDetection
                text: qsTr("In-ear detection")
                description: qsTr("Pauses playback when you take an earbud out.")
                checked: ear.inEarDetection
                automaticCheck: false
                onClicked: ear.setInEarDetection(!checked)
            }

            NavItem {
                visible: ear.hasGestures
                text: qsTr("Gestures")
                description: qsTr("What pinching the earbuds does")
                onClicked: pageStack.push(Qt.resolvedUrl("GesturesPage.qml"))
            }

            // ----- Connection and calls -----
            SectionHeader {
                visible: ear.hasLowLatency || ear.hasSuperMic || ear.hasDualConnection
                text: qsTr("Connection and calls")
            }

            TextSwitch {
                visible: ear.hasDualConnection
                text: qsTr("Dual connection")
                description: ear.dualConnectionNeedsRestart
                             ? qsTr("Connected to two devices at the same time, e.g. phone and tablet. Switching restarts the earbuds.")
                             : qsTr("Connected to two devices at the same time, e.g. phone and tablet.")
                checked: ear.dualConnection
                automaticCheck: false
                onClicked: {
                    var enable = !checked
                    if (ear.dualConnectionNeedsRestart)
                        restartRemorse.execute(qsTr("Restarting earbuds"),
                                               function() { ear.setDualConnection(enable) })
                    else
                        ear.setDualConnection(enable)
                }
            }

            NavItem {
                visible: ear.hasDualConnection && ear.dualConnection
                text: qsTr("Manage devices")
                description: qsTr("Which devices the earbuds are connected to")
                onClicked: pageStack.push(Qt.resolvedUrl("DualConnectPage.qml"))
            }

            TextSwitch {
                visible: ear.hasLowLatency
                text: qsTr("Low latency")
                description: qsTr("Less delay between picture and sound, for games and videos.")
                checked: ear.lowLatency
                automaticCheck: false
                onClicked: ear.setLowLatency(!checked)
            }

            TextSwitch {
                visible: ear.hasSuperMic
                text: qsTr("Super Mic")
                description: qsTr("Microphone mode for calls in loud surroundings.")
                checked: ear.superMic
                automaticCheck: false
                onClicked: ear.setSuperMic(!checked)
            }

            // ----- Tools -----
            SectionHeader {
                visible: page.connected
                text: qsTr("Tools")
            }

            NavItem {
                visible: page.connected
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

            // ----- Info -----
            SectionHeader {
                visible: page.connected && (ear.firmwareVersion.length > 0 || ear.serialNumber.length > 0)
                text: qsTr("Info")
            }

            DetailItem {
                visible: page.connected && ear.detectedModel.length > 0
                label: qsTr("Model")
                value: ear.detectedModel
            }

            DetailItem {
                visible: page.connected && ear.firmwareVersion.length > 0
                label: qsTr("Firmware")
                value: ear.firmwareVersion
            }

            DetailItem {
                visible: page.connected && ear.serialNumber.length > 0
                label: qsTr("Serial number")
                value: ear.serialNumber
            }
        }

        VerticalScrollDecorator { }
    }

    RemorsePopup { id: restartRemorse }
}
