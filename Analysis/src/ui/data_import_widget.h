#ifndef DATAIMPORTWIDGET_H
#define DATAIMPORTWIDGET_H

#include <QVector>
#include <QWidget>
#include <QVariantMap>

#include "infrastructure/io/text_file_reader.h" // 提供 FilePreviewData 定义

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;

// DataImportWidget 类定义
class DataImportWidget : public QWidget {
    Q_OBJECT
public:
    explicit DataImportWidget(QWidget* parent = nullptr);

    // 获取用户在UI上配置的导入参数
    QVariantMap getImportConfig() const;

public slots:
    // 设置从文件中读取的预览数据
    void setPreviewData(const FilePreviewData& previewData);

signals:
    // 当用户点击导入按钮时发出
    void importRequested();
    // 当用户选择文件后，请求预览
    void previewRequested(const QString& filePath);

private slots:
    void onBrowseFile();
    void onImportClicked();
    void onCloseClicked();

private:
    // 帮助函数
    QWidget* createTemperatureGroup();
    QWidget* createTimeGroup();
    QWidget* createSignalGroup();
    QWidget* createRateGroup();
    QFrame* createSeparator();
    void setupConnections();
    void populateColumnCombos(const QVector<FilePreviewColumn>& columns);
    [[nodiscard]] int extractColumnIndex(const QComboBox* combo) const;
    void updateTemperatureControls();
    void updateRateControls();
    void handleSignalModeToggle(QCheckBox* source);

private:
    // 顶部：数据文件选择
    QLineEdit* m_filePathEdit;
    QPushButton* m_browseBtn;

    // 中间：表头 / 预览
    QTextEdit* m_headerPreview;
    QPlainTextEdit* m_previewArea;

    // 质量（样品初始质量）
    QDoubleSpinBox* m_initialMassSpin;
    QLabel* m_massUnitLabel; // "g"
    QComboBox* m_dataTypeCombo;

    // 温度分组
    QCheckBox* m_tempHasColumnChk;
    QComboBox* m_tempColumnCombo;
    QComboBox* m_tempUnitCombo;
    QCheckBox* m_noTempColumnChk;
    QDoubleSpinBox* m_tempSetSpin;

    // 时间分组
    QComboBox* m_timeColumnCombo;
    QComboBox* m_timeUnitCombo;
    QSpinBox* m_filterPointsSpin;

    // 信号分组
    QComboBox* m_signalColumnCombo;
    QCheckBox* m_continuousChk;
    QCheckBox* m_noncontinuousChk;
    QComboBox* m_signalTypeCombo;
    QLineEdit* m_signalUnitEdit;
    QLineEdit* m_signalNameEdit;

    // 速率分组
    QCheckBox* m_rateHasColumnChk;
    QComboBox* m_rateColumnCombo;
    QComboBox* m_rateUnitCombo;
    QSpinBox* m_dynamicRateSpin;

    // 底部按钮
    QPushButton* m_importBtn;
    QPushButton* m_closeBtn;
};
#endif // DATAIMPORTWIDGET_H
