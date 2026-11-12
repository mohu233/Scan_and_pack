#ifndef BROWSER_PROXY_BADGE_H
#define BROWSER_PROXY_BADGE_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class ProxyExitIpLookup;

// A native WS_CHILD control inside the corresponding Chromium window.
// QLabel is only used to render the control; no Qt tool window is shown.
class BrowserProxyBadge : public QObject
{
    Q_OBJECT
public:
    BrowserProxyBadge(qint64 browserProcessId, QNetworkAccessManager *proxyNetwork,
                      QObject *parent = nullptr,
                      const QUrl &lookupUrl = QUrl("https://api.ipify.org?format=json"));
    ~BrowserProxyBadge() override;

    QString displayText() const;
    QString exitIp() const { return m_exitIp; }
    void refresh();
    void setEnvironmentName(const QString &name);

signals:
    void exitIpChanged(const QString &ip);

private:
    friend class ProxyWorkflowTests;
    enum class Status { Checking, Verified, Unconfirmed };
    QLabel *createLabel() const;
    void styleLabel(QLabel *label) const;
    void fitLabel(QLabel *label, int availableWidth) const;
    QString addressText() const;
    void updateLabels();
    void syncWindows();

    qint64 m_processId;
    ProxyExitIpLookup *m_lookup;
    QTimer m_windowTimer;
    QTimer m_refreshTimer;
    QHash<quintptr, QLabel *> m_labels;
    QHash<quintptr, quintptr> m_childWindows;
    QString m_exitIp;
    QString m_environmentName;
    Status m_status = Status::Checking;
};

#endif
