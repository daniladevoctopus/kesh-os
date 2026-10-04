#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>

class SystemBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentTime READ currentTime NOTIFY timeChanged)
    Q_PROPERTY(QString currentDate READ currentDate NOTIFY timeChanged)
    Q_PROPERTY(int uptimeSeconds READ uptimeSeconds NOTIFY timeChanged)
    Q_PROPERTY(QVariantList installedApps READ installedApps CONSTANT)

public:
    explicit SystemBridge(QObject *parent = nullptr);

    QString currentTime() const;
    QString currentDate() const;
    int uptimeSeconds() const;
    QVariantList installedApps() const;

    Q_INVOKABLE bool launch(const QString &appPath);
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void poweroff();

signals:
    void timeChanged();
    void appLaunched(const QString &path, bool success);

private slots:
    void updateClock();

private:
    QTimer *m_timer;
    int m_uptime;
};
