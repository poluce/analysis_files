#include "PeakAreaDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>

PeakAreaDialog::PeakAreaDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("峰面积计算"));
    setModal(false);  // 非模态，允许用户点击图表

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_promptLabel = new QLabel(tr("请在峰上点击两个点"), this);
    m_promptLabel->setStyleSheet("font-size: 14px; padding: 10px;");
    layout->addWidget(m_promptLabel);

    m_progressLabel = new QLabel(tr("已选择: 0 / 2"), this);
    layout->addWidget(m_progressLabel);

    m_resultLabel = new QLabel(this);
    m_resultLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: blue; padding: 10px;");
    m_resultLabel->setVisible(false);
    layout->addWidget(m_resultLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_cancelButton = new QPushButton(tr("取消"), this);
    m_okButton = new QPushButton(tr("确定"), this);
    m_okButton->setEnabled(false);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_okButton);
    layout->addLayout(buttonLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        emit cancelled();
        reject();
    });

    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);

    // 设置最小尺寸
    setMinimumWidth(300);
}

void PeakAreaDialog::showPickingPrompt()
{
    qDebug() << "PeakAreaDialog::showPickingPrompt - 显示点拾取提示";
    m_resultLabel->setVisible(false);
    m_progressLabel->setText(tr("已选择: 0 / 2"));
    m_okButton->setEnabled(false);
}

void PeakAreaDialog::updateProgress(int picked, int total)
{
    qDebug() << "PeakAreaDialog::updateProgress - 更新进度:" << picked << "/" << total;
    m_progressLabel->setText(tr("已选择: %1 / %2").arg(picked).arg(total));
}

void PeakAreaDialog::showResult(double area, const QString& unit)
{
    qDebug() << "PeakAreaDialog::showResult - 显示结果: 面积=" << area << ", 单位=" << unit;
    m_resultLabel->setText(tr("峰面积: %1 %2").arg(area, 0, 'f', 2).arg(unit));
    m_resultLabel->setVisible(true);
    m_okButton->setEnabled(true);
    qDebug() << "  结果标签已设置为可见，确定按钮已启用";
}
