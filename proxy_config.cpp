#include "proxy_config.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QRegularExpression>
#include <QUrl>

namespace {
bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}

bool validKey(const QString &key)
{
    return QRegularExpression("^[A-Za-z0-9+/]{43}=$").match(key).hasMatch()
        && QByteArray::fromBase64(key.toLatin1()).size() == 32;
}

bool validCidr(const QString &value)
{
    const QStringList parts = value.split('/');
    QHostAddress address;
    bool ok = false;
    const int prefix = parts.value(1).toInt(&ok);
    return parts.size() == 2 && address.setAddress(parts[0]) && ok && prefix >= 0
        && prefix <= (address.protocol() == QAbstractSocket::IPv4Protocol ? 32 : 128);
}

QStringList splitList(const QString &text)
{
    QStringList values;
    for (const QString &value : text.split(',', Qt::SkipEmptyParts)) values << value.trimmed();
    return values;
}

QJsonObject normalizeProxy(QJsonObject profile)
{
    profile["protocol"] = profile["protocol"].toString(profile["type"].toString("HTTP")).toUpper();
    profile["host"] = profile["host"].toString(profile["server"].toString()).trimmed();
    profile["port"] = profile["port"].toVariant().toString();
    profile["mode"] = "custom";
    return profile;
}
}

bool ProxyConfig::parseWireGuard(const QString &text, QJsonObject *profile, QString *error)
{
    if (error) error->clear();
    QMap<QString, QString> interface, peer;
    QString section;
    bool sawInterface = false, sawPeer = false;
    for (QString line : text.split('\n')) {
        line = line.trimmed();
        if (line.startsWith(QChar(0xfeff))) line.remove(0, 1);
        line = line.section('#', 0, 0).section(';', 0, 0).trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('[')) {
            if (line == "[Interface]" && !sawInterface) { section = "Interface"; sawInterface = true; }
            else if (line == "[Peer]" && !sawPeer) { section = "Peer"; sawPeer = true; }
            else return fail(error, QString::fromUtf8("仅支持一个 Interface 和一个 Peer 的 WireGuard 配置。"));
            continue;
        }
        const int separator = line.indexOf('=');
        if (separator < 1 || section.isEmpty()) return fail(error, QString::fromUtf8("WireGuard 配置格式无效。"));
        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        const QStringList allowed = section == "Interface"
            ? QStringList{"PrivateKey", "Address", "DNS", "MTU", "ListenPort"}
            : QStringList{"PublicKey", "PresharedKey", "AllowedIPs", "Endpoint", "PersistentKeepalive"};
        if (!allowed.contains(key)) return fail(error, QString::fromUtf8("不支持的配置字段：%1（不执行脚本或系统路由命令）。").arg(key.left(40)));
        auto &fields = section == "Interface" ? interface : peer;
        if (fields.contains(key)) return fail(error, QString::fromUtf8("配置字段重复：%1").arg(key));
        fields[key] = value;
    }
    if (!validKey(interface.value("PrivateKey")) || !validKey(peer.value("PublicKey"))
        || (peer.contains("PresharedKey") && !validKey(peer.value("PresharedKey"))))
        return fail(error, QString::fromUtf8("WireGuard 密钥缺失或格式无效。"));
    const QStringList addresses = splitList(interface.value("Address"));
    if (addresses.isEmpty()) return fail(error, QString::fromUtf8("缺少 Interface.Address。"));
    for (const QString &address : addresses) if (!validCidr(address)) return fail(error, QString::fromUtf8("Address 必须是有效的 IPv4/IPv6 CIDR 地址。"));
    const QStringList dns = splitList(interface.value("DNS"));
    if (dns.isEmpty()) return fail(error, QString::fromUtf8("缺少 DNS，请填写节点提供的 DNS 地址。"));
    for (const QString &server : dns) if (QHostAddress(server).isNull()) return fail(error, QString::fromUtf8("DNS 必须是有效的 IP 地址。"));
    const QStringList allowedIPs = splitList(peer.value("AllowedIPs"));
    if (allowedIPs.isEmpty()) return fail(error, QString::fromUtf8("缺少 Peer.AllowedIPs。"));
    for (const QString &network : allowedIPs) if (!validCidr(network)) return fail(error, QString::fromUtf8("AllowedIPs 必须是有效的 CIDR 地址。"));
    const QUrl endpoint("udp://" + peer.value("Endpoint"));
    if (!endpoint.isValid() || endpoint.host().isEmpty() || endpoint.port() < 1 || endpoint.port() > 65535
        || !endpoint.userInfo().isEmpty() || !endpoint.path().isEmpty() || endpoint.hasQuery() || endpoint.hasFragment())
        return fail(error, QString::fromUtf8("Peer.Endpoint 必须是主机:端口（IPv6 使用方括号）。"));
    for (auto it = interface.cbegin(); it != interface.cend(); ++it) {
        if (it.key() != "MTU" && it.key() != "ListenPort") continue;
        bool ok = false;
        const int number = it.value().toInt(&ok);
        if (!ok || number < (it.key() == "MTU" ? 576 : 0) || number > 65535)
            return fail(error, QString::fromUtf8("MTU 或 ListenPort 无效。"));
    }
    if (peer.contains("PersistentKeepalive")) {
        bool ok = false;
        const int number = peer["PersistentKeepalive"].toInt(&ok);
        if (!ok || number < 0 || number > 65535) return fail(error, QString::fromUtf8("PersistentKeepalive 无效。"));
    }
    // ListenPort belongs to the original VPN client. Use an ephemeral UDP port per environment.
    interface.remove("ListenPort");
    QString canonical = "[Interface]\n";
    for (auto it = interface.cbegin(); it != interface.cend(); ++it) canonical += it.key() + " = " + it.value() + '\n';
    canonical += "\n[Peer]\n";
    for (auto it = peer.cbegin(); it != peer.cend(); ++it) canonical += it.key() + " = " + it.value() + '\n';
    if (profile) {
        *profile = QJsonObject{{"mode", "wireguard"}, {"protocol", "WIREGUARD"},
                              {"host", endpoint.host()}, {"port", QString::number(endpoint.port())},
                              {"wireguard_config", canonical}, {"address", addresses.join(", ")},
                              {"dns", dns.join(", ")}};
    }
    return true;
}

bool ProxyConfig::validate(const QJsonObject &profile, QString *error)
{
    if (error) error->clear();
    const QString protocol = profile["protocol"].toString().toUpper();
    if (protocol == "WIREGUARD") return parseWireGuard(profile["wireguard_config"].toString(), nullptr, error);
    if (protocol == "NO PROXY") return true;
    if (!QStringList{"HTTP", "HTTPS", "SOCKS5"}.contains(protocol)) return fail(error, QString::fromUtf8("仅支持 HTTP、HTTPS、SOCKS5 和 WireGuard。"));
    bool ok = false;
    const int port = profile["port"].toVariant().toString().toInt(&ok);
    const QString host = profile["host"].toString();
    const QUrl url("http://" + (host.contains(':') ? '[' + host + ']' : host) + ':' + QString::number(port));
    if (!ok || port < 1 || port > 65535 || host.trimmed().isEmpty() || host != host.trimmed()
        || !url.isValid() || url.host().isEmpty() || url.port() != port
        || !url.userInfo().isEmpty() || !url.path().isEmpty() || url.hasQuery() || url.hasFragment())
        return fail(error, QString::fromUtf8("请输入有效的代理主机和 1–65535 端口。"));
    return true;
}

QJsonArray ProxyConfig::readFile(const QString &path, QString *error)
{
    if (error) error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        fail(error, QString::fromUtf8("无法读取配置文件，或文件超过 1 MB。")); return {};
    }
    const QByteArray bytes = file.readAll();
    const QString text = QString::fromUtf8(bytes);
    QJsonArray results;
    if (text.contains("[Interface]")) {
        QJsonObject profile;
        if (!parseWireGuard(text, &profile, error)) return {};
        profile["name"] = QFileInfo(path).completeBaseName();
        results.append(profile);
        return results;
    }
    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &jsonError);
    if (jsonError.error == QJsonParseError::NoError) {
        if (document.isArray()) results = document.array();
        else if (document.object()["proxies"].isArray()) results = document.object()["proxies"].toArray();
        else results.append(document.object());
    } else {
        for (const QString &line : text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts)) {
            const QString entry = line.trimmed();
            if (entry.startsWith('#')) continue;
            const QUrl url(entry.contains("://") ? entry : "http://" + entry);
            if (!url.isValid() || (!url.path().isEmpty() && url.path() != "/") || url.hasQuery() || url.hasFragment()) {
                fail(error, QString::fromUtf8("代理 URL 格式无效，请使用 协议://用户名:密码@主机:端口。"));
                return {};
            }
            results.append(QJsonObject{{"protocol", url.scheme().toUpper()}, {"host", url.host()},
                                       {"port", QString::number(url.port())}, {"username", url.userName(QUrl::FullyDecoded)},
                                       {"password", url.password(QUrl::FullyDecoded)}});
        }
    }
    if (results.isEmpty()) { fail(error, QString::fromUtf8("配置文件中没有可导入的代理。")); return {}; }
    QJsonArray normalized;
    int index = 0;
    for (const QJsonValue &value : results) {
        QJsonObject profile = normalizeProxy(value.toObject());
        if (profile["protocol"].toString() == "WIREGUARD") {
            const QString name = profile["name"].toString();
            QJsonObject wireguard;
            if (!parseWireGuard(profile["wireguard_config"].toString(), &wireguard, error)) return {};
            profile = wireguard;
            profile["name"] = name;
        }
        if (!validate(profile, error) || profile["protocol"].toString() == "NO PROXY") {
            if (error && error->isEmpty()) *error = QString::fromUtf8("配置文件中没有有效代理。");
            return {};
        }
        if (profile["name"].toString().trimmed().isEmpty()) profile["name"] = QFileInfo(path).completeBaseName() + QString("-%1").arg(++index);
        normalized.append(profile);
    }
    return normalized;
}

QString ProxyConfig::summary(const QJsonObject &profile)
{
    const QString host = profile["host"].toString();
    const QString endpoint = QString("%1://%2:%3").arg(profile["protocol"].toString().toLower(),
        host.contains(':') ? '[' + host + ']' : host, profile["port"].toVariant().toString());
    if (profile["protocol"].toString() == "WIREGUARD")
        return endpoint + QString::fromUtf8("\n隧道地址：%1\nDNS：%2").arg(profile["address"].toString(), profile["dns"].toString());
    return endpoint;
}

QString ProxyConfig::fingerprint(const QJsonObject &profile)
{
    QJsonObject identity;
    for (const QString key : {"protocol", "host", "port", "username", "password", "wireguard_config"}) identity[key] = profile[key];
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
