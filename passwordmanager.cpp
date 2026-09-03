#include "passwordmanager.h"
#include <QSettings>

bool PasswordManager::verifyPassword(const QString &password)
{
    QSettings settings("MetricScope", "Settings");
    // Default password is "1234" if none has been set yet
    QString storedPass = settings.value("AppPassword", "1234").toString();
    return (password == storedPass);
}

void PasswordManager::setPassword(const QString &password)
{
    QSettings settings("MetricScope", "Settings");
    settings.setValue("AppPassword", password);
}
