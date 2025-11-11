#ifndef I_PROGRESS_REPORTER_H
#define I_PROGRESS_REPORTER_H

#include <QString>

/**
 * @brief 算法进度报告接口
 *
 * 定义算法执行过程中报告进度和检查取消状态的接口。
 * AlgorithmWorker 实现此接口，算法通过调用这些方法来报告进度。
 */
class IProgressReporter {
public:
    virtual ~IProgressReporter() = default;

    /**
     * @brief 报告当前进度
     * @param percentage 进度百分比 (0-100)
     * @param message 可选的状态消息
     *
     * 注意：算法应避免过于频繁地调用此方法，建议每处理 10% 数据时调用一次。
     */
    virtual void reportProgress(int percentage, const QString& message = QString()) = 0;

    /**
     * @brief 检查是否应该取消执行
     * @return true 表示应该尽快停止执行
     *
     * 算法应定期（如每 100 次迭代）调用此方法检查取消标志，
     * 如果返回 true，应立即清理资源并返回空结果。
     */
    virtual bool shouldCancel() const = 0;
};

#endif // I_PROGRESS_REPORTER_H
