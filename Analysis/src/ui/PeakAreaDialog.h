#ifndef PEAKAREALDIALOG_H
#define PEAKAREALDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>

class PeakAreaDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PeakAreaDialog(QWidget *parent = nullptr);

    void showPickingPrompt();
    void updateProgress(int picked, int total);
    void showResult(double area, const QString& unit);

signals:
    void cancelled();

private:
    QLabel* m_promptLabel;
    QLabel* m_progressLabel;
    QLabel* m_resultLabel;
    QPushButton* m_cancelButton;
    QPushButton* m_okButton;
};

#endif // PEAKAREALDIALOG_H
