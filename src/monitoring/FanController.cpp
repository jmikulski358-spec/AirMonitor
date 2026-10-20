#include "monitoring/FanController.h"

#include <QStringList>


#include "network/ConnectionManager.h"

FanController::FanController(ConnectionManager *connectionManager, QObject *parent)
    : QObject(parent)
    , m_connectionManager(connectionManager)
{
}

std::optional<FanMode> FanController::currentFanMode() const{
    return m_fanMode;
}

std::optional<int> FanController::currentFanSpeed() const{
    return m_fanSpeed;
}

void FanController::setNewMode(FanMode newMode, std::optional<int> newSpeed){
    if (!m_connectionManager) {
        return;
    }

    if(newMode == FanMode::Manual && !newSpeed.has_value()){
        return;
    }

    if(newMode == FanMode::Manual && (newSpeed < 65 || newSpeed > 80)){
        return;
    }

    m_connectionManager->sendFrame(buildFanRegulationRequest(newMode, newSpeed));
}

QJsonObject FanController::buildFanRegulationRequest(FanMode newFanMode, std::optional<int> newSpeed){

    QJsonObject payload;
    payload.insert(QStringLiteral("new_mode"), static_cast<int>(newFanMode)); //conversion to int.

    if(newFanMode == FanMode::Manual && newSpeed.has_value()){
        payload.insert(QStringLiteral("requested_speed"), *newSpeed);
    } else {
        payload.insert(QStringLiteral("requested_speed"), QJsonValue::Null);
    }

    QJsonObject frame;

    frame.insert(QStringLiteral("type"), QStringLiteral("fan_regulation_request"));
    frame.insert(QStringLiteral("payload"), payload);

    return frame;
}

void FanController::onDataReceived(const QJsonObject &payload, const QString &deviceId, const QDateTime &timestamp)
{
    const QJsonObject fanState = payload.value(QStringLiteral("fan_state")).toObject();
    const QJsonValue modeValue = fanState.value(QStringLiteral("current_mode"));
    const QJsonValue speedValue = fanState.value(QStringLiteral("current_speed"));

    if (!modeValue.isDouble() || !speedValue.isDouble()) {
        return;
    }

    const int rawMode = qRound(modeValue.toDouble());
    const int speed = qRound(speedValue.toDouble());

    if (rawMode < 1 || rawMode > 4 || speed < 0 || speed > 100) {
        return;
    }

    const FanMode mode = static_cast<FanMode>(rawMode);

    // TODO: DB save. :)

    if (m_fanMode != mode) {
        m_fanMode = mode;
        emit fanModeChanged(mode, deviceId, timestamp);
    }
    if (m_fanSpeed != speed) {
        m_fanSpeed = speed;
        emit fanSpeedChanged(speed, deviceId, timestamp);
    }
}

void FanController::onActiveDeviceChanged()
{
    m_fanMode.reset();
    m_fanSpeed.reset();
    emit fanStateCleared();
}




