#include "system_bridge.h"

#include <QDateTime>
#include <QDebug>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_KESH_SPAWN 504
#define SYS_KESH_POWER 505

SystemBridge::SystemBridge(QObject *parent)
    : QObject(parent), m_uptime(0)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SystemBridge::updateClock);
    m_timer->start(1000);
}

void SystemBridge::updateClock() {
    m_uptime++;
    emit timeChanged();
}

QString SystemBridge::currentTime() const {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

QString SystemBridge::currentDate() const {
    return QDateTime::currentDateTime().toString("dd.MM.yyyy");
}

int SystemBridge::uptimeSeconds() const {
    return m_uptime;
}

bool SystemBridge::launch(const QString &appPath) {
    qDebug() << "[KeshShell] Launching application:" << appPath;
    QByteArray utf8 = appPath.toUtf8();
    long ret = syscall(SYS_KESH_SPAWN, utf8.constData());
    bool ok = (ret >= 0);
    qDebug() << "[KeshShell] Spawn result:" << ret;
    emit appLaunched(appPath, ok);
    return ok;
}

void SystemBridge::reboot() {
    qDebug() << "[KeshShell] System reboot requested";
    syscall(SYS_KESH_POWER, 1);
}

void SystemBridge::poweroff() {
    qDebug() << "[KeshShell] System poweroff requested";
    syscall(SYS_KESH_POWER, 2);
}

QVariantList SystemBridge::installedApps() const {
    QVariantList list;

    auto addApp = [&](const QString &name, const QString &cat, const QString &desc,
                      const QString &path, const QString &iconColor, const QString &glyph) {
        QVariantMap m;
        m["name"] = name;
        m["category"] = cat;
        m["description"] = desc;
        m["path"] = path;
        m["iconColor"] = iconColor;
        m["glyph"] = glyph;
        list.append(m);
    };

    addApp("Files", "System", "File Explorer", "/cdrom/boot/apps/explorer.elf", "#2980b9", "DIR");
    addApp("About KeshOS", "System", "System Info & Version", "/apps/about.elf", "#3daee9", "KESH");
    addApp("Terminal", "System", "Command Line Terminal", "/apps/term.kea", "#27ae60", ">_");
    addApp("Notepad", "Utilities", "Simple Text Editor", "/cdrom/boot/apps/notepad.elf", "#34495e", "TXT");
    addApp("Settings", "System", "System Configuration", "/cdrom/boot/apps/settings.elf", "#f39c12", "CFG");
    addApp("Doom", "Games", "Classic 3D Action Game", "/apps/doom.kea", "#e74c3c", "DOOM");
    addApp("Vector Fonts", "Graphics", "Vector Typography Demo", "/cdrom/boot/apps/font_demo.elf", "#9b59b6", "FON");
    addApp("2D Canvas", "Graphics", "Pixman Accelerated Graphics", "/cdrom/boot/apps/gfx_demo.elf", "#1abc9c", "GFX");

    return list;
}
