import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"
import "cover"

ApplicationWindow {
    initialPage: Component { MainPage { } }
    cover: Component { CoverPage { } }
    allowedOrientations: defaultAllowedOrientations

    // Coming back to the app with the earbuds connected but no control
    // channel (e.g. after the automatic attempts gave up): try again.
    Connections {
        target: Qt.application
        onActiveChanged: if (Qt.application.active) ear.ensureConnected()
    }
}
