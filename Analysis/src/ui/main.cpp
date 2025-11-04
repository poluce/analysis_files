#include "application/algorithm/AlgorithmManager.h"
#include "infrastructure/algorithm/BaselineCorrectionAlgorithm.h"
#include "infrastructure/algorithm/DifferentiationAlgorithm.h"
#include "infrastructure/algorithm/IntegrationAlgorithm.h"
#include "infrastructure/algorithm/MovingAverageFilterAlgorithm.h"
#include "mainwindow.h"

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
    AlgorithmManager::instance()->registerAlgorithm(new DifferentiationAlgorithm());
    AlgorithmManager::instance()->registerAlgorithm(new MovingAverageFilterAlgorithm());
    AlgorithmManager::instance()->registerAlgorithm(new IntegrationAlgorithm());
    AlgorithmManager::instance()->registerAlgorithm(new BaselineCorrectionAlgorithm());

    MainWindow w;
    w.show();

    return a.exec();
}
