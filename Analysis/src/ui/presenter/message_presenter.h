#ifndef UI_PRESENTER_MESSAGE_PRESENTER_H
#define UI_PRESENTER_MESSAGE_PRESENTER_H

#include <QObject>
#include <QMessageBox>

class QWidget;

class MessagePresenter : public QObject {
    Q_OBJECT

public:
    explicit MessagePresenter(QWidget* parentWidget = nullptr, QObject* parent = nullptr);

    void setParentWidget(QWidget* parentWidget);
    [[nodiscard]] QWidget* parentWidget() const;

    void showInformation(const QString& title, const QString& text) const;
    void showWarning(const QString& title, const QString& text) const;
    QMessageBox::StandardButton askQuestion(const QString& title,
                                            const QString& text,
                                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const;

private:
    QWidget* m_parentWidget = nullptr;
};

#endif // UI_PRESENTER_MESSAGE_PRESENTER_H
