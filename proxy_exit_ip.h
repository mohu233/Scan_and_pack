#ifndef PROXY_EXIT_IP_H
#define PROXY_EXIT_IP_H

#include <QObject>
#include <QPointer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// Shared, proxy-only exit address lookup for the environment list and browser badge.
class ProxyExitIpLookup : public QObject
{
    Q_OBJECT
public:
    explicit ProxyExitIpLookup(QNetworkAccessManager *network, QObject *parent = nullptr,
                              const QUrl &url = QUrl("https://api.ipify.org?format=json"));
    void refresh();
    bool isChecking() const { return !m_reply.isNull(); }
    static QString verifiedAddress(const QString &text);

signals:
    void finished(const QString &ip);

private:
    QPointer<QNetworkAccessManager> m_network;
    QPointer<QNetworkReply> m_reply;
    QUrl m_url;
};

#endif
