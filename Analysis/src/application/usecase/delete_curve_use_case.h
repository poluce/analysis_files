#ifndef APPLICATION_USECASE_DELETE_CURVE_USE_CASE_H
#define APPLICATION_USECASE_DELETE_CURVE_USE_CASE_H

#include <QObject>
#include <QString>
#include <QVector>

class CurveManager;
class HistoryManager;
class MessagePresenter;
class ThermalCurve;

class DeleteCurveUseCase : public QObject {
    Q_OBJECT

public:
    enum class Outcome {
        Deleted,
        Cancelled,
        NotFound,
        Forbidden,
        Failed
    };

    struct Result {
        Outcome outcome = Outcome::Failed;
        bool cascade = false;
    };

    DeleteCurveUseCase(CurveManager* curveManager,
                       HistoryManager* historyManager,
                       MessagePresenter* messagePresenter,
                       QObject* parent = nullptr);

    Result execute(const QString& curveId);

private:
    CurveManager* m_curveManager = nullptr;
    HistoryManager* m_historyManager = nullptr;
    MessagePresenter* m_messagePresenter = nullptr;

    bool confirmCascadeDelete(ThermalCurve* curve, const QVector<ThermalCurve*>& children) const;
};

#endif // APPLICATION_USECASE_DELETE_CURVE_USE_CASE_H
