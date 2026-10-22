#pragma once

#include <QString>
#include <QHostAddress>
#include <QDateTime>

// Enumy

enum class DeviceState { Disconnected = 0, Connecting, Connected };

enum class PairingState {
    Idle,
    ConnectingToAp,
    WaitingForResult,
    WaitingForCredentials,
    Succeeded,
    WrongPassword,
    TimedOut,
    Cancelled,
    NetworkError
};

enum class FanMode {
    Auto = 1,
    Turbo = 2,
    Sleep = 3,
    Manual = 4
};

enum class Parameter {
    ParticulateMatter,
    Voc,
    Co2,
    Temperature,
    Humidity,
    FilterState,
};

enum class TimeRange {
    LastHour,
    Last24Hours,
    LastWeek,
    LastTwoWeeks
};

enum class Severity { Normal, Bad, Critical };

// Structy

struct DiscoveredDevice {
    QString deviceId;
    QHostAddress address;
    quint16 port = 0;
};

struct SensorReading {
    double particulateMatter = 0.0;
    double voc = 0.0;
    int carbonDioxide = 0;
    double temperature = 0.0;
    int humidity = 0;
};

struct SensorThresholds {
    double particulateMatterBad = 20.0;
    double particulateMatterCritical = 55.0;
    double vocBad = 0.3;
    double vocCritical = 0.5;
    int carbonDioxideBad = 1000;
    int carbonDioxideCritical = 1500;
};

struct NotificationEvent {
    QString category;
    Parameter parameter = Parameter::ParticulateMatter;
    double value = 0.0;
    double threshold = 0.0;
    QDateTime timestamp;
    Severity severity = Severity::Normal;
};

struct StoredNotification {
    int id = -1;
    NotificationEvent event;
    QString description;
    QString deviceId;
    bool wasRead = false;
};

struct StoredReading {
    int id = -1;
    QString deviceId;
    SensorReading reading;
    QDateTime timestamp;
};

struct DateRange {
    QDateTime from;
    QDateTime to;
};