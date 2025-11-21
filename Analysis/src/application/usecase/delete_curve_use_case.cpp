#include "application/usecase/delete_curve_use_case.h"

#include "application/curve/curve_manager.h"
#include "application/history/history_manager.h"
#include "application/history/remove_curve_command.h"
#include "domain/model/thermal_curve.h"
#include "ui/presenter/message_presenter.h"

#include <QDebug>
#include <QMessageBox>
#include <memory>

DeleteCurveUseCase::DeleteCurveUseCase(CurveManager* curveManager,
                                       HistoryManager* historyManager,
                                       MessagePresenter* messagePresenter,
                                       QObject* parent)
    : QObject(parent)
    , m_curveManager(curveManager)
    , m_historyManager(historyManager)
    , m_messagePresenter(messagePresenter)
{
    Q_ASSERT(m_curveManager);
    Q_ASSERT(m_historyManager);
}

DeleteCurveUseCase::Result DeleteCurveUseCase::execute(const QString& curveId)
{
    Result result;

    if (curveId.isEmpty()) {
        qWarning() << "DeleteCurveUseCase::execute - curveId is empty";
        return result;
    }

    ThermalCurve* curve = m_curveManager->getCurve(curveId);
    if (!curve) {
        qWarning() << "DeleteCurveUseCase::execute - curve not found:" << curveId;
        result.outcome = Outcome::NotFound;
        return result;
    }

    if (curve->isMainCurve()) {
        if (m_messagePresenter) {
            m_messagePresenter->showWarning(
                tr("无法删除主曲线"),
                tr("曲线 \"%1\" 是主曲线（数据源），不能被删除。\n\n"
                   "主曲线是从文件导入的原始数据，是所有派生曲线的基础。\n"
                   "如果需要移除，请使�?文件 �?清空项目 功能。")
                    .arg(curve->name()));
        }
        result.outcome = Outcome::Forbidden;
        return result;
    }

    QVector<ThermalCurve*> children;
    bool cascadeDelete = false;
    if (m_curveManager->hasChildren(curveId)) {
        children = m_curveManager->getChildren(curveId);
        if (!confirmCascadeDelete(curve, children)) {
            result.outcome = Outcome::Cancelled;
            return result;
        }
        cascadeDelete = true;
    }

    QString description = cascadeDelete
        ? QStringLiteral("删除曲线 \"%1\" 及其子曲线").arg(curve->name())
        : QStringLiteral("删除曲线 \"%1\"").arg(curve->name());

    auto removeCommand = std::make_unique<RemoveCurveCommand>(
        m_curveManager,
        curveId,
        cascadeDelete,
        description);

    if (!m_historyManager->executeCommand(std::move(removeCommand))) {
        qWarning() << "DeleteCurveUseCase::execute - failed to execute remove command for curve:" << curveId;
        result.outcome = Outcome::Failed;
        return result;
    }

    result.outcome = Outcome::Deleted;
    result.cascade = cascadeDelete;
    return result;
}

bool DeleteCurveUseCase::confirmCascadeDelete(ThermalCurve* curve, const QVector<ThermalCurve*>& children) const
{
    if (!m_messagePresenter) {
        qWarning() << "DeleteCurveUseCase::confirmCascadeDelete - MessagePresenter not available";
        return false;
    }

    QString childrenList;
    for (int i = 0; i < children.size() && i < 5; ++i) {
        childrenList += QStringLiteral("  - %1\n").arg(children.at(i)->name());
    }
    if (children.size() > 5) {
        childrenList += QStringLiteral("  - ... (还有 %1 项)\n").arg(children.size() - 5);
    }

    auto reply = m_messagePresenter->askQuestion(
        tr("确认级联删除"),
        tr("曲线 \"%1\" 有 %2 个子曲线：\n\n%3\n"
           "删除此曲线将同时删除所有子曲线。\n\n"
           "是否继续删除？")
            .arg(curve->name())
            .arg(children.size())
            .arg(childrenList),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return reply == QMessageBox::Yes;
}
