#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

class ConnectionManager;
/*
 * CredentialsQuery needs ConnectionManager in order to send credentials frame. It doesn't own it,
 * and ConnectionManager exists even if CredentialsQuery does not. Therefore CredentialsQuerry has a reference to ConnectionManager
 * as a constructor parameter, it uses it, but cannot remove it.
*/

class CredentialsQuery: public QObject {
    Q_OBJECT;

    public:
        explicit CredentialsQuery(ConnectionManager *connectionManager, QObject *parent = nullptr);

        void enterCredentials(const QString &ssid, const QString &password);
        \
    private:
        QJsonObject buildCredentialsFrame() const;

        QString m_ssid;
        QString m_password;

        ConnectionManager *m_connectionManager = nullptr;
};

