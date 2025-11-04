QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets charts

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Include path for all source directories
INCLUDEPATH += $$PWD/src

# Ensure source files are treated as UTF-8 on Windows toolchains
win32:msvc: QMAKE_CXXFLAGS += /utf-8
win32:g++:  QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

SOURCES += \
    # UI Layer
    src/ui/DataImportWidget.cpp \
    src/ui/ProjectExplorerView.cpp \
    src/ui/main.cpp \
    src/ui/mainwindow.cpp \
    src/ui/ChartView.cpp \
    src/ui/CurveTreeModel.cpp \
    src/ui/PeakAreaDialog.cpp \
    src/ui/controller/MainController.cpp \
    src/ui/controller/CurveViewController.cpp \
    src/ui/controller/InteractionController.cpp \
    \
    # Application Layer
    src/application/curve/CurveManager.cpp \
    src/application/algorithm/AlgorithmManager.cpp \
    src/application/history/HistoryManager.cpp \
    src/application/history/AlgorithmCommand.cpp \
    src/application/history/BaselineCommand.cpp \
    \
    # Domain Layer
    src/domain/model/ThermalCurve.cpp \
    \
    # Infrastructure Layer
    src/infrastructure/io/TextFileReader.cpp \
    src/infrastructure/algorithm/DifferentiationAlgorithm.cpp \
    src/infrastructure/algorithm/MovingAverageFilterAlgorithm.cpp \
    src/infrastructure/algorithm/IntegrationAlgorithm.cpp \
    src/infrastructure/algorithm/BaselineCorrectionAlgorithm.cpp

HEADERS += \
    # UI Layer
    src/ui/DataImportWidget.h \
    src/ui/ProjectExplorerView.h \
    src/ui/mainwindow.h \
    src/ui/ChartView.h \
    src/ui/CurveTreeModel.h \
    src/ui/PeakAreaDialog.h \
    src/ui/controller/MainController.h \
    src/ui/controller/CurveViewController.h \
    src/ui/controller/InteractionController.h \
    \
    # Application Layer
    src/application/curve/CurveManager.h \
    src/application/algorithm/AlgorithmManager.h \
    src/application/history/HistoryManager.h \
    src/application/history/AlgorithmCommand.h \
    src/application/history/BaselineCommand.h \
    \
    # Domain Layer
    src/domain/model/ThermalDataPoint.h \
    src/domain/model/ThermalCurve.h \
    src/domain/algorithm/IThermalAlgorithm.h \
    src/domain/algorithm/ICommand.h \
    \
    # Infrastructure Layer
    src/infrastructure/io/IFileReader.h \
    src/infrastructure/io/TextFileReader.h \
    src/infrastructure/algorithm/DifferentiationAlgorithm.h \
    src/infrastructure/algorithm/MovingAverageFilterAlgorithm.h \
    src/infrastructure/algorithm/IntegrationAlgorithm.h \
    src/infrastructure/algorithm/BaselineCorrectionAlgorithm.h

# FORMS section removed as UI is now code-based

TRANSLATIONS += \
    Analysis_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/${TARGET}/bin
else: unix:!android: target.path = /opt/${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
