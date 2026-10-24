/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QScopedPointer>
#include <QtQml>

#include <sailfishapp.h>

#include "devicemodel.h"
#include "earcontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match OrganizationName/ApplicationName in the desktop file (Sailjail).
    app->setOrganizationName(QStringLiteral("org.sappl"));
    app->setApplicationName(QStringLiteral("harbour-pinch"));
    app->setApplicationVersion(QStringLiteral(APP_VERSION));

    qmlRegisterUncreatableType<EarController>("harbour.pinch", 1, 0, "Ear",
                                              QStringLiteral("Use the 'ear' context property"));

    DeviceModel devices;
    EarController ear;
    QObject::connect(&devices, &DeviceModel::systemConnectionChanged,
                     &ear, &EarController::onSystemConnectionChanged);

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("ear"), &ear);
    view->rootContext()->setContextProperty(QStringLiteral("deviceModel"), &devices);
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"), QStringLiteral(APP_VERSION));
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    // The first refresh reports every paired device's connection state,
    // which lets the controller connect to the remembered buds right away.
    devices.refresh();

    return app->exec();
}
