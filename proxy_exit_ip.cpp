#include "proxy_exit_ip.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

ProxyExitIpLookup::ProxyExitIpLookup(QNetworkAccessManager *network, QObject *parent, const QUrl &url)
    : QObject(parent), m_network(network), m_url(url)
{
    if (m_network) m_network->setParent(this);
}

QString ProxyExitIpLookup::verifiedAddress(const QString &text)
{
    QHostAddress address;
    if (!address.setAddress(text.trimmed()) || address.isNull() || address.isLoopback() || address.isMulticast()
        || address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6) return {};
    const QHostAddress ipv4(address.toIPv4Address());
    if (address.protocol() == QAbstractSocket::IPv6Protocol && address.toIPv4Address() != 0 && ipv4.isLoopback()) return {};
    return address.toString();
}

void ProxyExitIpLookup::refresh()
{
    if (m_reply) return;
    // Never resolve an environment's exit IP through the system/default route.
    if (!m_network || (m_network->proxy().type() != QNetworkProxy::HttpProxy
                       && m_network->proxy().type() != QNetworkProxy::Socks5Proxy)) {
        emit finished({});
        return;
    }
    QNetworkRequest request(m_url);
    request.setTransferTimeout(10000);
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "ProfilePower/1.0");
    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    QTimer::singleShot(12000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        QString ip;
        if (reply->error() == QNetworkReply::NoError && body.size() <= 4096
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
            ip = verifiedAddress(QJsonDocument::fromJson(body).object()["ip"].toString());
        m_reply.clear();
        reply->deleteLater();
        emit finished(ip);
    });
}
