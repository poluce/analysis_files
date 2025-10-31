#include "mainwindow.h"
#include "application/algorithm/AlgorithmService.h"
#include "infrastructure/algorithm/DifferentiationAlgorithm.h"
#include "infrastructure/algorithm/MovingAverageFilterAlgorithm.h"
#include "infrastructure/algorithm/IntegrationAlgorithm.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString& locale : uiLanguages) {
        const QString baseName = "Analysis_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // 注册算法
    AlgorithmService::instance()->registerAlgorithm(new DifferentiationAlgorithm());
    AlgorithmService::instance()->registerAlgorithm(new MovingAverageFilterAlgorithm());
    AlgorithmService::instance()->registerAlgorithm(new IntegrationAlgorithm());

    MainWindow w;
    w.show();

    return a.exec();
}
