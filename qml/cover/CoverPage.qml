import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.pinch 1.0
import "../components"

CoverBackground {
    readonly property bool connected: ear.connectionState === Ear.Connected

    function modeText() {
        if (!ear.hasDevice)
            return qsTr("No earbuds")
        if (ear.audioConnecting)
            return qsTr("Connecting…")
        if (!connected)
            return qsTr("Not connected")
        if (!ear.ancSupported)
            return qsTr("Connected")
        switch (ear.ancMode) {
        case Ear.ModeOff:          return qsTr("Off")
        case Ear.ModeTransparency: return qsTr("Transparency")
        case Ear.ModeNoiseCancellation:
            switch (ear.ancStrength) {
            case Ear.StrengthLow:      return qsTr("ANC low")
            case Ear.StrengthMid:      return qsTr("ANC mid")
            case Ear.StrengthHigh:     return qsTr("ANC high")
            case Ear.StrengthAdaptive: return qsTr("ANC adaptive")
            }
            return qsTr("ANC")
        }
        return "–"
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        FormFactorImage {
            visible: ear.hasDevice
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.iconSizeLarge
            height: width
            formFactor: ear.formFactor
            opacity: connected ? 1.0 : Theme.opacityHigh
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
            text: ear.hasDevice ? ear.deviceName : "Pinch"
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
            text: modeText()
        }

        Label {
            visible: connected && text.length > 0
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            text: batteryText()
        }
    }

    // Compact "L 80% · R 75%" plus the case on its own line.
    function batteryText() {
        function part(prefix, level, charging) {
            return level < 0 ? "" : prefix + " " + level + "%" + (charging ? "+" : "")
        }
        if (ear.batterySingle >= 0)
            return part(qsTr("Battery"), ear.batterySingle, ear.chargingSingle)
        var buds = [part("L", ear.batteryLeft, ear.chargingLeft),
                    part("R", ear.batteryRight, ear.chargingRight)]
                .filter(function (s) { return s.length > 0 }).join(" · ")
        var box = part(qsTr("Case"), ear.batteryCase, ear.chargingCase)
        return [buds, box].filter(function (s) { return s.length > 0 }).join("\n")
    }

    CoverActionList {
        enabled: connected && ear.ancSupported

        CoverAction {
            iconSource: "image://theme/icon-cover-next"
            onTriggered: ear.cycleMode()
        }
    }

    CoverActionList {
        enabled: ear.hasDevice && !connected && !ear.audioConnecting

        CoverAction {
            iconSource: "image://theme/icon-cover-refresh"
            onTriggered: ear.connectNow()
        }
    }
}
