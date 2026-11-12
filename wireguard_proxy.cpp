#include "wireguard_proxy.h"
#include "proxy_config.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>

struct WireGuardProxyState
{
    QProcess process;
    std::unique_ptr<QTemporaryDir> directory;
    quint16 port = 0;

    ~WireGuardProxyState()
    {
        process.disconnect();
        if (process.state() != QProcess::NotRunning) {
            process.terminate();
            if (!process.waitForFinished(1000)) { process.kill(); process.waitForFinished(1000); }
        }
    }
};

WireGuardProxy::WireGuardProxy(QObject *parent) : QObject(parent) {}

WireGuardProxy::~WireGuardProxy() { stop(); }

QString WireGuardProxy::executablePath()
{
    const QDir app(QCoreApplication::applicationDirPath());
    for (const QString relative : {"tools/wireproxy/wireproxy.exe", "../../../tools/wireproxy/wireproxy.exe", "../../tools/wireproxy/wireproxy.exe"}) {
        const QString path = QDir::cleanPath(app.absoluteFilePath(relative));
        if (QFile::exists(path)) return path;
    }
    return {};
}

bool WireGuardProxy::start(const QJsonObject &profile, QString *error)
{
    stop();
    QJsonObject parsed;
    if (!ProxyConfig::parseWireGuard(profile["wireguard_config"].toString(), &parsed, error)) return false;
    // A WireGuard peer has one remote endpoint. Reuse a node within the application
    // so testing it or opening another browser cannot steal its UDP session.
    static QHash<QString, std::weak_ptr<WireGuardProxyState>> nodes;
    for (auto it = nodes.begin(); it != nodes.end();) {
        if (it.value().expired()) it = nodes.erase(it); else ++it;
    }
    const QString identity = ProxyConfig::fingerprint(parsed);
    if (auto existing = nodes.value(identity).lock()) {
        if (existing->process.state() == QProcess::Running) { m_state = existing; return true; }
    }
    const QString executable = executablePath();
    if (executable.isEmpty()) {
        if (error) *error = QString::fromUtf8("缺少 WireGuard 代理组件，请重新安装完整安装包。");
        return false;
    }
    auto state = std::make_shared<WireGuardProxyState>();
    QProcess *core = &state->process;
    core->setProcessChannelMode(QProcess::MergedChannels);
    // Diagnostics may include node data; discard them rather than persisting credentials.
    connect(core, &QProcess::readyReadStandardOutput, core, [core]() { core->readAllStandardOutput(); });
    QTcpServer portPicker;
    portPicker.setProxy(QNetworkProxy::NoProxy);
    if (!portPicker.listen(QHostAddress::LocalHost, 0)) {
        if (error) *error = QString::fromUtf8("无法分配本地代理端口。");
        return false;
    }
    state->port = portPicker.serverPort();
    portPicker.close();
    state->directory = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/profilepower-wg-XXXXXX");
    if (!state->directory->isValid()) {
        if (error) *error = QString::fromUtf8("无法创建节点运行目录。");
        return false;
    }
    QFile config(state->directory->filePath("wireproxy.conf"));
    if (!config.open(QIODevice::WriteOnly)) {
        if (error) *error = QString::fromUtf8("无法写入节点运行配置。");
        return false;
    }
    config.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QByteArray content = (parsed["wireguard_config"].toString()
        + QString("\n[Socks5]\nBindAddress = 127.0.0.1:%1\n").arg(state->port)).toUtf8();
    if (config.write(content) != content.size()) {
        if (error) *error = QString::fromUtf8("写入节点运行配置失败。");
        return false;
    }
    config.close();
    core->start(executable, {"-c", config.fileName()});
    if (core->waitForStarted(3000)) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000 && core->state() != QProcess::NotRunning) {
            QTcpSocket socket;
            socket.setProxy(QNetworkProxy::NoProxy);
            socket.connectToHost(QHostAddress::LocalHost, state->port);
            if (socket.waitForConnected(100)) {
                m_state = state;
                nodes[identity] = state;
                return true;
            }
            core->waitForReadyRead(50);
        }
    }
    stop();
    if (error) *error = QString::fromUtf8("WireGuard 代理未能启动，请检查节点 Endpoint、DNS 及代理组件。");
    return false;
}

void WireGuardProxy::stop()
{
    m_state.reset();
}

QNetworkProxy WireGuardProxy::networkProxy() const { return QNetworkProxy(QNetworkProxy::Socks5Proxy, "127.0.0.1", m_state ? m_state->port : 0); }
QString WireGuardProxy::server() const { return QString("socks5://127.0.0.1:%1").arg(m_state ? m_state->port : 0); }
QProcess *WireGuardProxy::process() const { return m_state ? &m_state->process : nullptr; }
