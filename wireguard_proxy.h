#ifndef WIREGUARD_PROXY_H
#define WIREGUARD_PROXY_H

#include <QObject>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QProcess>
#include <QTemporaryDir>
#include <memory>

struct WireGuardProxyState;

class WireGuardProxy : public QObject
{
public:
    explicit WireGuardProxy(QObject *parent = nullptr);
    ~WireGuardProxy() override;
    bool start(const QJsonObject &profile, QString *error);
    void stop();
    QNetworkProxy networkProxy() const;
    QString server() const;
    QProcess *process() const;
    static QString executablePath();

private:
    std::shared_ptr<WireGuardProxyState> m_state;
};

#endif
