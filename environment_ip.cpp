#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "browser_proxy_badge.h"
#include "proxy_config.h"
#include "proxy_exit_ip.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QSaveFile>
#include <QSet>

QJsonObject MainWindow::environmentProxyProfile(const QJsonObject &config) const
{
    const QString type = config["proxy_type"].toString().trimmed().toUpper();
    if (type.isEmpty() || type == "NO PROXY" || type == QString::fromUtf8("无代理"))
        return QJsonObject{{"protocol", "No Proxy"}};
    if (type == "WIREGUARD") return config["wireguard_proxy"].toObject();
    return QJsonObject{{"protocol", type}, {"host", config["proxy_host"].toString()},
        {"port", config["proxy_port"].toVariant().toString()},
        {"username", config["proxy_username"].toString()}, {"password", config["proxy_password"].toString()}};
}

QString MainWindow::environmentProxyFingerprint(const QJsonObject &config) const
{
    if (config["proxy_profile_mode"].toString() == "dynamic" || config["proxy_mode_index"].toInt(-1) == 2)
        return ProxyConfig::fingerprint(QJsonObject{{"protocol", "DYNAMIC"}, {"host", config["dynamic_proxy_url"]}});
    return ProxyConfig::fingerprint(environmentProxyProfile(config));
}

QString MainWindow::environmentIpText(const QJsonObject &config) const
{
    if (environmentProxyProfile(config)["protocol"].toString() == "No Proxy") return QString::fromUtf8("直连");
    if (config["proxy_exit_ip_fingerprint"].toString() == environmentProxyFingerprint(config)) {
        const QString ip = ProxyExitIpLookup::verifiedAddress(config["proxy_exit_ip"].toString());
        if (!ip.isEmpty()) return ip;
        if (!config["proxy_exit_ip_checked_at"].toString().isEmpty()) return QString::fromUtf8("检测失败");
    }
    return QString::fromUtf8("未检测");
}

void MainWindow::queueEnvironmentIpChecks()
{
    m_environmentIpQueue.clear();
    QSet<QString> existing;
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        auto idItem = ui->tableWidget->item(row, 3);
        auto nameItem = ui->tableWidget->item(row, 2);
        if (!idItem || !nameItem) continue;
        const QString id = idItem->text();
        existing.insert(id);
        const QJsonObject config = QJsonDocument::fromJson(nameItem->data(Qt::UserRole + 3).toString().toUtf8()).object();
        const QJsonObject profile = environmentProxyProfile(config);
        const QString fingerprint = environmentProxyFingerprint(config);
        if (auto active = m_environmentIpChecks.value(id)) {
            if (active->property("proxyFingerprint").toString() == fingerprint) {
                if (ui->tableWidget->item(row, 5)) ui->tableWidget->item(row, 5)->setText(QString::fromUtf8("检测中…"));
                continue;
            }
            m_environmentIpChecks.remove(id);
            active->deleteLater();
        }
        if (profile["protocol"].toString() == "No Proxy") continue;
        bool running = false;
        for (QProcess *process : findChildren<QProcess *>()) {
            if (process->state() == QProcess::Running && process->property("env_id").toString() == id
                && process->property("proxyFingerprint").toString() == fingerprint
                && process->findChild<BrowserProxyBadge *>()) { running = true; break; }
        }
        if (running) continue; // The browser badge already refreshes this exact route.
        const QDateTime checked = QDateTime::fromString(config["proxy_exit_ip_checked_at"].toString(), Qt::ISODate);
        const qint64 age = checked.secsTo(QDateTime::currentDateTimeUtc());
        if (checked.isValid() && age >= 0 && age < 60 && config["proxy_exit_ip_fingerprint"].toString() == fingerprint) continue;
        // A dynamic API resolves a new endpoint on launch, so an old saved endpoint
        // cannot establish the current exit IP while that environment is closed.
        if (config["proxy_profile_mode"].toString() == "dynamic" || config["proxy_mode_index"].toInt(-1) == 2) continue;
        m_environmentIpQueue.enqueue(QJsonObject{{"env_id", id}, {"fingerprint", fingerprint}, {"profile", profile}});
    }
    for (auto it = m_environmentIpChecks.begin(); it != m_environmentIpChecks.end();) {
        if (!existing.contains(it.key())) { if (it.value()) it.value()->deleteLater(); it = m_environmentIpChecks.erase(it); }
        else ++it;
    }
    startNextEnvironmentIpCheck();
}

void MainWindow::startNextEnvironmentIpCheck()
{
    // Bound process creation and IP service requests when many environments exist.
    while (m_environmentIpChecks.size() < 2 && !m_environmentIpQueue.isEmpty()) {
        const QJsonObject job = m_environmentIpQueue.dequeue();
        const QString id = job["env_id"].toString(), fingerprint = job["fingerprint"].toString();
        if (m_environmentIpChecks.contains(id)) continue;
        bool current = false;
        for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
            auto idItem = ui->tableWidget->item(row, 3);
            auto nameItem = ui->tableWidget->item(row, 2);
            if (!idItem || !nameItem || idItem->text() != id) continue;
            const auto config = QJsonDocument::fromJson(nameItem->data(Qt::UserRole + 3).toString().toUtf8()).object();
            current = environmentProxyFingerprint(config) == fingerprint;
            if (current && ui->tableWidget->item(row, 5)) ui->tableWidget->item(row, 5)->setText(QString::fromUtf8("检测中…"));
            break;
        }
        if (!current) continue;
        QString error;
        auto network = createProxyNetworkManager(job["profile"].toObject(), &error);
        if (!network) { updateEnvironmentExitIp(id, fingerprint, {}); continue; }
        auto lookup = new ProxyExitIpLookup(network, this, m_environmentIpLookupUrl);
        lookup->setProperty("proxyFingerprint", fingerprint);
        m_environmentIpChecks.insert(id, lookup);
        connect(lookup, &ProxyExitIpLookup::finished, this, [this, lookup, id, fingerprint](const QString &ip) {
            // A browser launch or proxy edit may have superseded this background check.
            if (m_environmentIpChecks.value(id) != lookup) return;
            m_environmentIpChecks.remove(id);
            updateEnvironmentExitIp(id, fingerprint, ip);
            lookup->deleteLater();
            startNextEnvironmentIpCheck();
        });
        lookup->refresh();
    }
}

void MainWindow::updateEnvironmentExitIp(const QString &envId, const QString &fingerprint, const QString &result)
{
    const QString ip = ProxyExitIpLookup::verifiedAddress(result);
    const QString text = ip.isEmpty() ? QString::fromUtf8("检测失败") : ip;
    const QString checkedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    for (int row = 0; row < ui->tableWidget_2->rowCount(); ++row) {
        auto group = ui->tableWidget_2->item(row, 1);
        if (group && group->data(Qt::UserRole).toString() == envId
            && group->data(Qt::UserRole + 1).toString() == fingerprint && ui->tableWidget_2->item(row, 4))
            ui->tableWidget_2->item(row, 4)->setText(text);
    }
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        auto idItem = ui->tableWidget->item(row, 3);
        auto nameItem = ui->tableWidget->item(row, 2);
        if (!idItem || !nameItem || idItem->text() != envId) continue;
        const QString path = nameItem->data(Qt::UserRole + 2).toString();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return; // Deleted environments must not be recreated.
        QJsonObject config = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        if (config.isEmpty() || environmentProxyFingerprint(config) != fingerprint) return;
        config["proxy_exit_ip"] = ip;
        config["proxy_exit_ip_checked_at"] = checkedAt;
        config["proxy_exit_ip_fingerprint"] = fingerprint;
        const QByteArray bytes = QJsonDocument(config).toJson(QJsonDocument::Indented);
        QSaveFile save(path);
        if (!save.open(QIODevice::WriteOnly) || save.write(bytes) != bytes.size() || !save.commit())
            writeLog("Exit IP", QString("Could not save last exit IP for environment %1").arg(envId));
        // Preserve normalized paths and the other cached UI data while updating only IP metadata.
        QJsonObject cached = QJsonDocument::fromJson(nameItem->data(Qt::UserRole + 3).toString().toUtf8()).object();
        for (const QString key : {"proxy_exit_ip", "proxy_exit_ip_checked_at", "proxy_exit_ip_fingerprint"}) cached[key] = config[key];
        nameItem->setData(Qt::UserRole + 3, QString::fromUtf8(QJsonDocument(cached).toJson(QJsonDocument::Compact)));
        auto item = ui->tableWidget->item(row, 5);
        if (item) {
            item->setText(text);
            item->setToolTip(QString::fromUtf8("出口 IP 检测时间：%1").arg(QDateTime::fromString(checkedAt, Qt::ISODate).toLocalTime().toString("yyyy-MM-dd HH:mm:ss")));
        }
        return;
    }
}
