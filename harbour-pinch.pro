TARGET = harbour-pinch

CONFIG += sailfishapp sailfishapp_i18n c++11
QT += dbus

VERSION = 1.0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    src/main.cpp \
    src/nothingprotocol.cpp \
    src/earcontroller.cpp \
    src/bluezprofile.cpp \
    src/rfcommchannel.cpp \
    src/devicemodel.cpp

HEADERS += \
    src/nothingprotocol.h \
    src/earcontroller.h \
    src/bluezprofile.h \
    src/rfcommchannel.h \
    src/devicemodel.h \
    src/formfactor.h \
    src/modelinfo.h

DISTFILES += \
    qml/harbour-pinch.qml \
    qml/cover/CoverPage.qml \
    qml/pages/MainPage.qml \
    qml/pages/DevicesPage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/AboutPage.qml \
    qml/pages/GesturesPage.qml \
    qml/pages/FindPage.qml \
    qml/pages/FitTestPage.qml \
    qml/pages/DualConnectPage.qml \
    qml/components/ChoiceRow.qml \
    qml/components/BatteryItem.qml \
    qml/components/FormFactorImage.qml \
    qml/components/NavItem.qml \
    qml/images/*.png \
    design/*.svg \
    rpm/harbour-pinch.spec \
    harbour-pinch.desktop \
    translations/*.ts

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

TRANSLATIONS += \
    translations/harbour-pinch.ts \
    translations/harbour-pinch-de.ts
