#include "application/algorithm/algorithm_manager.h"
#include "infrastructure/algorithm/baseline_correction_algorithm.h"
#include "infrastructure/algorithm/differentiation_algorithm.h"
#include "infrastructure/algorithm/integration_algorithm.h"
#include "infrastructure/algorithm/moving_average_filter_algorithm.h"
#include "main_window.h"

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
