#include <QDebug>
#include <QCursor>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <cstdlib>

#include "system_bridge.h"

int main(int argc, char *argv[]) {
    // Default to software rendering on Linux framebuffer or Wayland if set
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "linuxfb:fb=/dev/fb0");
    }
    qputenv("QT_QPA_FB_HIDECURSOR", "0");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RHI_BACKEND", "software");
    qputenv("QT_QPA_FONTDIR", "/cdrom/boot/fonts");
    qputenv("QT_QPA_EVDEV_KEYBOARD_PARAMETERS", "/dev/input/event0");
    qputenv("QT_QPA_EVDEV_MOUSE_PARAMETERS", "/dev/input/event1");
    qputenv("QT_QPA_GENERIC_PLUGINS", "evdevmouse:/dev/input/event1,evdevkeyboard:/dev/input/event0");
    qputenv("QT_LOGGING_RULES", "qt.qpa.input=true;qt.qpa.*=true");

    QGuiApplication app(argc, argv);
    app.setApplicationName("KeshOS Desktop Shell");
    app.setOrganizationName("KeshOS");
    app.setOverrideCursor(QCursor(Qt::ArrowCursor));

    // Load embedded Open Sans vector font
    int fontId = QFontDatabase::addApplicationFont(":/fonts/fonts/OpenSans.ttf");
    if (fontId >= 0) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            QFont appFont(families.first(), 11);
            QGuiApplication::setFont(appFont);
            qDebug() << "[KeshShell] Default UI font:" << families.first();
        }
    }

    QQmlApplicationEngine engine;
    SystemBridge bridge;
    engine.rootContext()->setContextProperty("KeshOS", &bridge);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    engine.loadFromModule("KeshOS.Shell", "Main");

    qDebug() << "[KeshShell] Shell engine started successfully.";
    return app.exec();
}
