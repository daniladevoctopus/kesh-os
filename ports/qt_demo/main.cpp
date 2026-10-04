#include <QApplication>
#include <QDateTime>
#include <QFontDatabase>
#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "linuxfb:fb=/dev/fb0");
    QApplication app(argc, argv);

    const int fontId = QFontDatabase::addApplicationFont("/cdrom/boot/fonts/OpenSans.ttf");
    if (fontId >= 0) {
        QFont font(QFontDatabase::applicationFontFamilies(fontId).value(0));
        font.setPointSize(12);
        app.setFont(font);
    }

    QWidget window;
    window.setWindowTitle("Qt 6 on KeshOS");
    window.setStyleSheet(
        "QWidget { background: #121722; color: #edf3ff; }"
        "QFrame#card { background: #202839; border: 1px solid #43516a; border-radius: 18px; }"
        "QLabel#title { color: #8bd5ff; font-size: 34px; font-weight: 700; }"
        "QLabel#status { color: #9ef0bf; font-size: 18px; }"
        "QProgressBar { background: #111722; border: 1px solid #43516a; border-radius: 8px; height: 18px; text-align: center; }"
        "QProgressBar::chunk { background: #47b8ff; border-radius: 7px; }"
        "QPushButton { background: #47b8ff; color: #08111c; border: 0; border-radius: 10px; padding: 12px 28px; font-weight: 700; }"
        "QPushButton:pressed { background: #9ef0bf; }");

    auto *outer = new QVBoxLayout(&window);
    outer->setContentsMargins(80, 70, 80, 70);
    auto *card = new QFrame;
    card->setObjectName("card");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(46, 38, 46, 38);
    layout->setSpacing(18);

    auto *title = new QLabel("Qt 6 is running on KeshOS");
    title->setObjectName("title");
    auto *subtitle = new QLabel("Static QtBase 6.11.2  |  musl libc  |  linuxfb + evdev");
    auto *status = new QLabel("Framebuffer initialized. The first native Qt window is alive.");
    status->setObjectName("status");
    auto *clock = new QLabel;
    auto *progress = new QProgressBar;
    progress->setRange(0, 100);
    progress->setValue(100);
    progress->setFormat("Phase 4: QtBase platform bring-up complete");
    auto *closeButton = new QPushButton("Close Qt demo");

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(12);
    layout->addWidget(status);
    layout->addWidget(clock);
    layout->addWidget(progress);
    layout->addStretch();
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    outer->addWidget(card);

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        clock->setText("System time: " + QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
    });
    QObject::connect(closeButton, &QPushButton::clicked, &app, &QApplication::quit);
    timer.start(1000);
    clock->setText("System time: " + QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
    window.showFullScreen();
    return app.exec();
}
