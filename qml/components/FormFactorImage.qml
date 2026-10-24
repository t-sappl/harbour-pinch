import QtQuick 2.0
import Sailfish.Silica 1.0

// Generic, monochrome earbud illustration for a form factor, tinted like a
// theme icon. formFactor uses the Ear.FormFactor values (0 in-ear,
// 1 open-ear, 2 over-ear).
HighlightImage {
    property int formFactor: 0

    source: {
        switch (formFactor) {
        case 1:  return Qt.resolvedUrl("../images/formfactor-openear.png")
        case 2:  return Qt.resolvedUrl("../images/formfactor-overear.png")
        default: return Qt.resolvedUrl("../images/formfactor-inear.png")
        }
    }
    color: Theme.primaryColor
    highlightColor: Theme.highlightColor
    fillMode: Image.PreserveAspectFit
    sourceSize.width: width
    sourceSize.height: height
    smooth: true
}
