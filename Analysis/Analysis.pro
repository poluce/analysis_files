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
    src/ui/data_import_widget.cpp \
    src/ui/project_explorer_view.cpp \
    src/ui/main.cpp \
    src/ui/main_window.cpp \
    src/ui/chart_view.cpp \
    src/ui/floating_label.cpp \
    src/ui/trapezoid_measure_tool.cpp \
    src/ui/controller/main_controller.cpp \
    src/ui/controller/curve_view_controller.cpp \
    \
    # Application Layer
    src/application/application_context.cpp \
    src/application/curve/curve_manager.cpp \
    src/application/algorithm/algorithm_manager.cpp \
    src/application/algorithm/algorithm_context.cpp \
    src/application/algorithm/algorithm_coordinator.cpp \
    src/application/history/history_manager.cpp \
    src/application/history/add_curve_command.cpp \
    src/application/history/algorithm_command.cpp \
    src/application/history/baseline_command.cpp \
    src/application/project/project_tree_manager.cpp \
    \
    # Domain Layer
    src/domain/model/thermal_curve.cpp \
    \
    # Infrastructure Layer
    src/infrastructure/io/text_file_reader.cpp \
    src/infrastructure/algorithm/differentiation_algorithm.cpp \
    src/infrastructure/algorithm/moving_average_filter_algorithm.cpp \
    src/infrastructure/algorithm/integration_algorithm.cpp \
    src/infrastructure/algorithm/baseline_correction_algorithm.cpp


HEADERS += \
    # UI Layer
    src/ui/data_import_widget.h \
    src/ui/project_explorer_view.h \
    src/ui/main_window.h \
    src/ui/chart_view.h \
    src/ui/floating_label.h \
    src/ui/trapezoid_measure_tool.h \
    src/ui/controller/main_controller.h \
    src/ui/controller/curve_view_controller.h \
    \
    # Application Layer
    src/application/application_context.h \
    src/application/curve/curve_manager.h \
    src/application/algorithm/algorithm_manager.h \
    src/application/algorithm/algorithm_context.h \
    src/application/algorithm/algorithm_coordinator.h \
    src/application/algorithm/algorithm_descriptor.h \
    src/application/history/add_curve_command.h \
    src/application/history/history_manager.h \
    src/application/history/algorithm_command.h \
    src/application/history/baseline_command.h \
    src/application/project/project_tree_manager.h \
    \
    # Domain Layer
    src/domain/model/thermal_data_point.h \
    src/domain/model/thermal_curve.h \
    src/domain/algorithm/algorithm_descriptor.h \
    src/domain/algorithm/i_thermal_algorithm.h \
    src/domain/algorithm/i_command.h \
    \
    # Infrastructure Layer
    src/infrastructure/io/i_file_reader.h \
    src/infrastructure/io/text_file_reader.h \
    src/infrastructure/algorithm/differentiation_algorithm.h \
    src/infrastructure/algorithm/moving_average_filter_algorithm.h \
    src/infrastructure/algorithm/integration_algorithm.h \
    src/infrastructure/algorithm/baseline_correction_algorithm.h


# FORMS section removed as UI is now code-based

TRANSLATIONS += \
    Analysis_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/${TARGET}/bin
else: unix:!android: target.path = /opt/${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
