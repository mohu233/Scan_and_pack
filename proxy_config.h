#ifndef PROXY_CONFIG_H
#define PROXY_CONFIG_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace ProxyConfig {
// Only recognized data fields are accepted; imported files never execute commands.
bool parseWireGuard(const QString &text, QJsonObject *profile, QString *error);
bool validate(const QJsonObject &profile, QString *error);
QJsonArray readFile(const QString &path, QString *error);
QString summary(const QJsonObject &profile);
QString fingerprint(const QJsonObject &profile);
}

#endif
