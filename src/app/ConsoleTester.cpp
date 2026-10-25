#include "app/ConsoleTester.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaEnum>
#include <QStringList>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>

#include "app/AppController.h"

namespace {
QString value(double v)
{
    return std::isnan(v) ? QStringLiteral("--") : QString::number(v); //insan in oreder to determine NaN value.
}
}

ConsoleTester::ConsoleTester(AppController *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
{
}

void ConsoleTester::start()
{
    connect(m_app, &AppController::screenChanged, this, [this] {
        qInfo() << "[screen]" << QMetaEnum::fromType<AppController::Screen>().valueToKey(static_cast<int>(m_app->screen()));
    });
    connect(m_app, &AppController::errorMessage, this, [](const QString &text) {
        qInfo() << "[error message]" << text;
    });
    connect(m_app, &AppController::devicesChanged, this, [this] {
        qInfo() << "[devices]" << m_app->devices();
    });
    connect(m_app, &AppController::scanningChanged, this, [this] {
        qInfo() << "[scanning]" << m_app->scanning();
    });
    connect(m_app, &AppController::connectingChanged, this, [this] {
        qInfo() << "[connecting]" << m_app->connecting();
    });
    connect(m_app, &AppController::pairingStateChanged, this, [this] {
        qInfo() << "[pairing state]" << m_app->pairingState();
    });
    connect(m_app, &AppController::readingChanged, this, [this] {
        qInfo().noquote() << "[reading] PM2.5" << value(m_app->pm25()) << "VOC" << value(m_app->voc())
                          << "CO2" << value(m_app->co2()) << "T" << value(m_app->temperature())
                          << "H" << value(m_app->humidity());
    });
    connect(m_app, &AppController::fanChanged, this, [this] {
        qInfo() << "[fan] mode" << m_app->fanMode() << "speed" << m_app->fanSpeed()
                << "pending" << m_app->fanCommandPending();
    });

    connect(m_app, &AppController::filterLifeChanged, this, [this]
            { qInfo() << "[filter]" << m_app->filterLifePercentage();
    });
    connect(m_app, &AppController::filterReplaced, this, [] {
        qInfo() << "[filter replaced]";
    });

    printHelp();

    // std::getline blocks, so it must not run in the main thread (it would freeze the event loop).
    // Every line is posted back to the main thread: the queued call is executed there, between events.
    std::thread([this] {
        std::string line;
        while (std::getline(std::cin, line)) {
            const QString text = QString::fromStdString(line);
            qInfo() << "[console] received:" << text;
            QMetaObject::invokeMethod(this, [this, text] { handleLine(text); }, Qt::QueuedConnection);
        }
    }).detach();   // debug tool: the thread simply dies with the process
}

void ConsoleTester::handleLine(const QString &line)
{
    const QStringList t = line.split(' ', Qt::SkipEmptyParts);
    if (t.isEmpty()) {
        return;
    }

    switch (t.at(0).toInt()) {
    case 1:  m_app->refreshDevices(); break;
    case 2:  qInfo() << "[devices]" << m_app->devices(); break;
    case 3:  m_app->startPairing(); break;
    case 4:  m_app->confirmApConnected(); break;
    case 5:  if (t.size() >= 3) m_app->submitCredentials(t.at(1), t.at(2)); else qInfo() << "usage: 5 <ssid> <password>"; break;
    case 6:  m_app->cancelPairing(); break;
    case 7:  if (t.size() >= 2) m_app->connectToDevice(t.at(1)); else qInfo() << "usage: 7 <deviceId>"; break;
    case 8:  m_app->goBack(); break;
    case 9:  if (t.size() >= 2) m_app->setFanMode(t.at(1).toInt()); else qInfo() << "usage: 9 <mode 1-4>"; break;
    case 10: if (t.size() >= 2) m_app->setManualSpeed(t.at(1).toInt()); else qInfo() << "usage: 10 <speed>"; break;
    case 11: printState(); break;
    case 12: m_app->replaceFilter(); break;
    case 0:
        if (t.at(0) == QLatin1String("0")) { QCoreApplication::quit(); break; }
        [[fallthrough]];
    default: printHelp(); break;
    }
}

void ConsoleTester::printHelp() const
{
    qInfo().noquote() << "\nCommands:\n"
        "  1 refresh list        2 show list        3 start pairing     4 \"I'm connected\"\n"
        "  5 <ssid> <password>   6 cancel pairing   7 <deviceId> connect   8 back (\"<\")\n"
        "  9 <mode 1-4>  fan mode (1 Auto, 2 Turbo, 3 Sleep, 4 Manual)    10 <speed>  manual speed\n"
        "  11 show state         0 quit";
}

void ConsoleTester::printState() const
{
    qInfo() << "[state] screen" << QMetaEnum::fromType<AppController::Screen>().valueToKey(static_cast<int>(m_app->screen()))
            << "connecting" << m_app->connecting() << "scanning" << m_app->scanning()
            << "fan mode" << m_app->fanMode() << "speed" << m_app->fanSpeed() << "filter" << m_app->filterLifePercentage();
}
