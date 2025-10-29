#ifndef FORM_H
#define FORM_H

#include <QComboBox>
#include <QList>
#include <QMap>
#include <QString>
#include <QWidget>

namespace Ui {
class Form;
}

// 数据结构，用于存储列数据
struct ColumnsData {
  QList<double> temperature;
  QList<double> time;
  QList<double> customColumn;
  QList<double> velocity;
};

class Form : public QWidget {
  Q_OBJECT

public:
  explicit Form(QWidget *parent = nullptr);
  QString lineEdit_5() const;
  QString textEdit() const;
  QString comboBox_2() const;
  QString comboBox_6() const;
  QString comboBox_4() const;
  QString comboBox_8() const;
  QString lineEdit_4() const;
  double getDoubleSpinBoxValue() const;
  double getDoubleSpinBoxValue_2() const;
  ~Form();

signals:
  void dataReady(const QVariantMap &data);

private slots:
  void on_pushButton_2_clicked();

  void on_pushButton_3_clicked();

  void on_pushButton_clicked();
  void onCheckBox3Changed(int state);
  void onCheckBox4Changed(int state);
  void onCheckBoxChanged(int state);
  void onCheckBox2Changed(int state);

private:
  void setupConnections();
  Ui::Form *ui;
};

#endif // FORM_H
