#pragma once

#include <QObject>

class AppController;

/*
Debug programm (not a product feature) for testing functionalities of AppController: reads commands typed in the console, calls AppController,
and prints every change the UI would see. It shall be removed when the QML screens exist. "IT SHALL NOT PASS!!" ~Gandalf The Grey ;)
 */
class ConsoleTester : public QObject
{
    Q_OBJECT

public:
    explicit ConsoleTester(AppController *app, QObject *parent = nullptr);

    void start();   // connects the print-outs and starts the thread that reads the console

private:
    void handleLine(const QString &line);   // always runs in the main thread
    void printHelp() const;
    void printState() const;

    AppController *m_app = nullptr;   // not owned
};
