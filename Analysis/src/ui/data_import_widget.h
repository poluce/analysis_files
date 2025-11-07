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

/**
 * @brief DataImportWidget 是数据导入对话框
 *
 * 提供智能列识别和预览功能，支持用户自定义列映射和参数配置。
 *
 * 功能区域：
 * - 文件选择：浏览并选择数据文件
 * - 文件预览：显示文件表头和数据预览
 * - 列映射配置：温度、时间、信号、速率列的映射
 * - 单位配置：各列的单位选择
 * - 参数配置：样品质量、数据类型等
 */
class DataImportWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit DataImportWidget(QWidget* parent = nullptr);

    /**
     * @brief 获取用户在UI上配置的导入参数
     *
     * 返回的参数包括：
     * - 列索引映射（温度列、时间列、信号列等）
     * - 单位选择（温度单位、时间单位等）
     * - 样品质量、数据类型等元数据
     *
     * @return 导入配置的键值对映射表
     */
    QVariantMap getImportConfig() const;

public slots:
    /**
     * @brief 设置从文件中读取的预览数据
     *
     * 根据预览数据更新UI，包括：
     * - 显示文件表头和预览内容
     * - 填充列下拉框
     * - 自动识别列类型
     *
     * @param previewData 文件预览数据
     */
    void setPreviewData(const FilePreviewData& previewData);

signals:
    /**
     * @brief 当用户点击导入按钮时发出
     */
    void importRequested();

    /**
     * @brief 当用户选择文件后，请求预览
     * @param filePath 文件路径
     */
    void previewRequested(const QString& filePath);

private slots:
    /**
     * @brief 处理"浏览文件"按钮点击
     */
    void onBrowseFile();

    /**
     * @brief 处理"导入"按钮点击
     */
    void onImportClicked();

    /**
     * @brief 处理"关闭"按钮点击
     */
    void onCloseClicked();

private:
    // 帮助函数

    /**
     * @brief 创建温度配置分组
     * @return 温度配置组件
     */
    QWidget* createTemperatureGroup();

    /**
     * @brief 创建时间配置分组
     * @return 时间配置组件
     */
    QWidget* createTimeGroup();

    /**
     * @brief 创建信号配置分组
     * @return 信号配置组件
     */
    QWidget* createSignalGroup();

    /**
     * @brief 创建速率配置分组
     * @return 速率配置组件
     */
    QWidget* createRateGroup();

    /**
     * @brief 创建分隔线
     * @return 分隔线组件
     */
    QFrame* createSeparator();

    /**
     * @brief 设置信号槽连接
     */
    void setupConnections();

    /**
     * @brief 填充列下拉框
     * @param columns 列信息列表
     */
    void populateColumnCombos(const QVector<FilePreviewColumn>& columns);

    /**
     * @brief 从下拉框中提取列索引
     * @param combo 下拉框指针
     * @return 列索引，如果未选择返回 -1
     */
    [[nodiscard]] int extractColumnIndex(const QComboBox* combo) const;

    /**
     * @brief 更新温度控件的启用/禁用状态
     */
    void updateTemperatureControls();

    /**
     * @brief 更新速率控件的启用/禁用状态
     */
    void updateRateControls();

    /**
     * @brief 处理信号模式切换（连续/非连续）
     * @param source 触发切换的复选框
     */
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
