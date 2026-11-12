#ifndef PROFILE_RUNTIME_H
#define PROFILE_RUNTIME_H

#include <QJsonObject>
#include <QString>

class ProfileRuntime
{
public:
    static bool writeExtension(const QString &environmentPath,
                               const QJsonObject &config,
                               QString *extensionPath,
                               QString *errorMessage);
};

#endif // PROFILE_RUNTIME_H
