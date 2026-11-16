using System.ComponentModel;

namespace HNKC.AIBox.Enums;

/// <summary>
/// 任务运行状态
/// </summary>
public enum TaskStatusEnum
{
    /// <summary>
    /// 运行中
    /// </summary>
    [Description("运行中")] 
    Running = 1,

    /// <summary>
    /// 已停止
    /// </summary>
    [Description("已停止")]  
    Stopped = 2,

    /// <summary>
    /// 运行异常
    /// </summary>
    [Description("运行异常")]  
    Exception = 3,
}