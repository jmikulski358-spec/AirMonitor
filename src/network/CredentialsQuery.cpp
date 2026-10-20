#include "network/CredentialsQuery.h"
#include "network/ConnectionManager.h"

CredentialsQuery::CredentialsQuery(ConnectionManager *connectionManager, QObject *parent)
    : QObject(parent), m_connectionManager(connectionManager){
}

void CredentialsQuery::enterCredentials(const QString &ssid, const QString &password)
{
    m_ssid = ssid;
    m_password = password;

    //Check if ptr is not nullptr, since ConnectionManager * is a regular pointer we have to check if we have an object at all.
    if (m_connectionManager) {
        m_connectionManager->sendFrame(buildCredentialsFrame());
    }
}

QJsonObject CredentialsQuery::buildCredentialsFrame() const //it means we won't change anything in the object we're using.
{
    QJsonObject payload;
    //compare .value in ConnectionManager; now we create an object from scratch.
    payload.insert(QStringLiteral("ssid"), m_ssid);
    payload.insert(QStringLiteral("password"), m_password);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), QStringLiteral("credentials_submit"));
    frame.insert(QStringLiteral("payload"), payload);
    return frame;
}