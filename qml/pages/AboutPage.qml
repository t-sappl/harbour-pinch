import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("About") }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "/usr/share/icons/hicolor/172x172/apps/harbour-pinch.png"
                width: Theme.iconSizeExtraLarge
                height: width
                sourceSize.width: width
                sourceSize.height: height
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
                text: "Pinch " + appVersion
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Noise control for Nothing earbuds. Unofficial app, not affiliated with Nothing Technology.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                textFormat: Text.StyledText
                linkColor: Theme.highlightColor
                text: qsTr("Protocol knowledge from the community projects <a href=\"https://github.com/radiance-project/ear-web\">ear-web</a> and <a href=\"https://github.com/DaanHessen/earctl\">earctl</a>. Licensed under the GPL-3.0-or-later.")
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: "© 2026 Thomas Sappl"
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Source code")
                onClicked: Qt.openUrlExternally("https://github.com/t-sappl/harbour-pinch")
            }
        }
    }
}
