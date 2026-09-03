#ifndef PASSWORDMANAGER_H
#define PASSWORDMANAGER_H

#include <QString>

class PasswordManager
{
public:
    static bool verifyPassword(const QString &password);
    static void setPassword(const QString &password);
};

#endif // PASSWORDMANAGER_H
