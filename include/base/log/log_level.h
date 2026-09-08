/**
 * @file log_level.h
 * @brief 日志级别与源码位置等值类型。
 */
#ifndef ITFLEE_LOG_LEVEL_H_
#define ITFLEE_LOG_LEVEL_H_

#include "define.h"

namespace itflee {

/**
 * @brief 日志严重级别，数值越大越严重。
 *
 * @note 线程安全：不适用（无共享可变状态）。
 */
enum class LogLevel {
    Trace = 0,    ///< 最细粒度跟踪
    Debug = 1,    ///< 排障细节
    Info = 2,     ///< 默认现场可见事件
    Warn = 3,     ///< 可恢复异常
    Error = 4,    ///< 本次操作失败
    Critical = 5  ///< 致命错误，将立即 flush
};

/**
 * @brief 源码位置记录策略。
 *
 * @note 线程安全：不适用（无共享可变状态）。
 */
enum class SourceLocationMode {
    Auto = 0,  ///< Debug 构建记录，Release 不记录
    On = 1,    ///< 始终记录
    Off = 2    ///< 始终不记录
};

/**
 * @brief 一条日志对应的源码位置。
 *
 * @note 线程安全：不适用（无共享可变状态）。
 */
struct SourceLocation {
    const char* file{""};      ///< 源文件路径（通常来自 __FILE__）
    int line{0};               ///< 行号（通常来自 __LINE__）
    const char* function{""};  ///< 函数名（通常来自 __FUNCTION__）
};

}  // namespace itflee

#endif  // ITFLEE_LOG_LEVEL_H_
