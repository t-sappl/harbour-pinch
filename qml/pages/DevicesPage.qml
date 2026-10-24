import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    Component.onCompleted: deviceModel.refresh()

    SilicaListView {
        id: list

        anchors.fill: parent
        model: deviceModel

        header: PageHeader {
            title: qsTr("Choose earbuds")
            description: qsTr("Paired devices")
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: deviceModel.refresh()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: deviceModel.available ? qsTr("No paired devices")
                                        : qsTr("Bluetooth is not available")
            hintText: qsTr("Pair your earbuds in the Bluetooth settings first")
        }

        delegate: ListItem {
            id: item

            contentHeight: Theme.itemSizeMedium
            highlighted: down || model.address === ear.deviceAddress

            onClicked: {
                ear.selectDevice(model.address, model.name, model.modelName, model.path, model.connected)
                pageStack.pop()
            }

            FormFactorImage {
                id: deviceIcon
                visible: model.isNothing
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: Theme.iconSizeMedium
                height: width
                formFactor: model.formFactor
                highlighted: item.highlighted
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                // Same text column for all rows, with or without an icon.
                x: Theme.horizontalPageMargin + Theme.iconSizeMedium + Theme.paddingMedium
                width: parent.width - x - Theme.horizontalPageMargin

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: model.name
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    font.bold: model.isNothing
                }

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    text: model.connected ? qsTr("Connected") + " · " + model.address
                                          : model.address
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
