#include <QDebug>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QTimer>
#include <QUrl>
#include <QStringList>

static const char *qmlStatusName(QQmlComponent::Status status) {
    switch (status) {
    case QQmlComponent::Null: return "Null";
    case QQmlComponent::Ready: return "Ready";
    case QQmlComponent::Loading: return "Loading";
    case QQmlComponent::Error: return "Error";
    }
    return "Unknown";
}

static void logQmlErrors(const QQmlComponent &component) {
    const QList<QQmlError> errors = component.errors();
    qWarning() << "[qml_demo] Stage 3/error count:" << errors.size();
    for (qsizetype i = 0; i < errors.size(); ++i)
        qWarning().noquote() << "[qml_demo] Stage 3/error" << i << errors.at(i).toString();
}

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "linuxfb:fb=/dev/fb0");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RHI_BACKEND", "software");
    qputenv("QT_QPA_FONTDIR", "/cdrom/boot/fonts");
    qputenv("QT_QPA_EVDEV_KEYBOARD_PARAMETERS", "/dev/input/event0");
    qputenv("QT_QPA_EVDEV_MOUSE_PARAMETERS", "/dev/input/event1");

    QGuiApplication app(argc, argv);

    // 1. Load font: try embedded resource first, then disk paths
    int fontId = QFontDatabase::addApplicationFont(":/fonts/fonts/OpenSans.ttf");
    qDebug() << "[qml_demo] Embedded font load result:" << fontId;

    if (fontId < 0) {
        fontId = QFontDatabase::addApplicationFont("/cdrom/boot/fonts/OpenSans.ttf");
        qDebug() << "[qml_demo] /cdrom font load result:" << fontId;
    }
    if (fontId < 0) {
        fontId = QFontDatabase::addApplicationFont("/boot/fonts/OpenSans.ttf");
        qDebug() << "[qml_demo] /boot font load result:" << fontId;
    }

    if (fontId >= 0) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        qDebug() << "[qml_demo] Loaded font families:" << families;
        if (!families.isEmpty()) {
            QFont appFont(families.first(), 13);
            QGuiApplication::setFont(appFont);
            qDebug() << "[qml_demo] Application default font set to:" << families.first();
        }
    } else {
        qWarning() << "[qml_demo] Could not load OpenSans.ttf font!";
    }

    qDebug() << "[qml_demo] Step 1: Instantiating QQmlApplicationEngine...";
    QQmlApplicationEngine engine;
    qDebug() << "[qml_demo] Step 2: QQmlApplicationEngine created successfully.";

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { 
                         qCritical() << "[qml_demo] objectCreationFailed triggered!";
                         QCoreApplication::exit(1); 
                     }, Qt::QueuedConnection);

    qDebug() << "[qml_demo] Stage 3.0: Inspecting QML import paths...";
    const QStringList importPaths = engine.importPathList();
    for (qsizetype i = 0; i < importPaths.size(); ++i)
        qDebug() << "[qml_demo] Stage 3.0/importPath" << i << importPaths.at(i);

    const QStringList resourceCandidates = {
        QStringLiteral(":/qt/qml/KeshOS/Demo/Main.qml"),
        QStringLiteral(":/KeshOS/Demo/Main.qml"),
        QStringLiteral(":/Main.qml")
    };
    for (const QString &path : resourceCandidates) {
        QFile file(path);
        qDebug() << "[qml_demo] Stage 3.1/resource" << path
                 << "exists=" << file.exists() << "open=" << file.open(QIODevice::ReadOnly)
                 << "size=" << file.size();
    }

    qDebug() << "[qml_demo] Stage 3.2: Constructing QQmlComponent...";
    QQmlComponent component(&engine);
    qDebug() << "[qml_demo] Stage 3.3: Component constructed; status="
             << qmlStatusName(component.status());

    QObject *rootObject = nullptr;
    QObject::connect(&component, &QQmlComponent::progressChanged,
                     &app, [](qreal progress) {
        qDebug() << "[qml_demo] Stage 3/progressChanged:" << progress;
    });
    QObject::connect(&component, &QQmlComponent::statusChanged,
                     &app, [&](QQmlComponent::Status status) {
        qDebug() << "[qml_demo] Stage 3/statusChanged:" << qmlStatusName(status)
                 << "progress=" << component.progress();
        if (status == QQmlComponent::Error) {
            logQmlErrors(component);
            QCoreApplication::exit(1);
            return;
        }
        if (status != QQmlComponent::Ready)
            return;

        qDebug() << "[qml_demo] Stage 3.6: Component ready; creating root object...";
        rootObject = component.create();
        qDebug() << "[qml_demo] Stage 3.7: component.create() returned" << rootObject;
        if (!rootObject) {
            logQmlErrors(component);
            QCoreApplication::exit(1);
            return;
        }
        rootObject->setParent(&app);
        qDebug() << "[qml_demo] Step 4: Root QML object created successfully.";
    });

    QTimer heartbeat;
    int heartbeatNumber = 0;
    QObject::connect(&heartbeat, &QTimer::timeout, &app, [&] {
        ++heartbeatNumber;
        qDebug() << "[qml_demo] Stage 3/heartbeat" << heartbeatNumber
                 << "status=" << qmlStatusName(component.status())
                 << "progress=" << component.progress();
        if (heartbeatNumber == 15 && component.status() == QQmlComponent::Loading)
            qWarning() << "[qml_demo] Stage 3 stalled for 15 seconds while Loading";
    });
    heartbeat.start(1000);

    const QUrl mainQmlUrl(QStringLiteral("qrc:/qt/qml/KeshOS/Demo/Main.qml"));
    qDebug() << "[qml_demo] Stage 3.4: Bypassing module lookup; calling loadUrl asynchronously:"
             << mainQmlUrl;
    component.loadUrl(mainQmlUrl, QQmlComponent::Asynchronous);
    qDebug() << "[qml_demo] Stage 3.5: loadUrl returned; status="
             << qmlStatusName(component.status()) << "progress=" << component.progress();
    if (component.status() == QQmlComponent::Error)
        logQmlErrors(component);

    qDebug() << "[qml_demo] Step 5: Entering app.exec() event loop...";
    return app.exec();
}
