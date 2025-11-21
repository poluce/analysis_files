#include "ui/presenter/message_presenter.h"

#include <QWidget>

MessagePresenter::MessagePresenter(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

void MessagePresenter::setParentWidget(QWidget* parentWidget)
{
    m_parentWidget = parentWidget;
}

QWidget* MessagePresenter::parentWidget() const
{
    return m_parentWidget;
}

void MessagePresenter::showInformation(const QString& title, const QString& text) const
{
    QMessageBox::information(m_parentWidget, title, text);
}

void MessagePresenter::showWarning(const QString& title, const QString& text) const
{
    QMessageBox::warning(m_parentWidget, title, text);
}

QMessageBox::StandardButton MessagePresenter::askQuestion(const QString& title,
                                                          const QString& text,
                                                          QMessageBox::StandardButtons buttons,
                                                          QMessageBox::StandardButton defaultButton) const
{
    return QMessageBox::question(m_parentWidget, title, text, buttons, defaultButton);
}
