using System.ComponentModel;

namespace HNKC.AIBox.Enums;

/// <summary>
/// 模式授权状态
/// </summary>
public enum ModeAuthorizationEnum
{
    /// <summary>
    /// 未授权
    /// </summary>
    [Description("未授权")]
    Unauthorized = 0,

    /// <summary>
    /// 已授权
    /// </summary>
    [Description("已授权")]
    Authorized = 1,
}