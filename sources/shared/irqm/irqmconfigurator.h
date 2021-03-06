#ifndef IRQMCONFIGURATOR_H
#define IRQMCONFIGURATOR_H

#include <QApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>

class IRQMConfigurator : public QObject
{
    Q_OBJECT
public:
    explicit IRQMConfigurator(QObject *parent = 0);

    static void registerAllParams(QQmlApplicationEngine *engine);

    static void registerModules(QQmlApplicationEngine *engine);
    static void registerProperties(QQmlApplicationEngine *engine);

    static void configureRecommendedSettings();

signals:

public slots:
};

#endif // IRQMCONFIGURATOR_H
