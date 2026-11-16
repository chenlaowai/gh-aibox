using System.ComponentModel;

namespace HNKC.AIBox.Enums;

/// <summary>
/// 规则类型
/// </summary>
public enum RuleTypeEnum
{
    /// <summary>
    /// 辅助线
    /// </summary>
    [Description("辅助线")] 
    Auxiliary = 0,

    /// <summary>
    /// 矩形
    /// </summary>
    [Description("矩形")] 
    Rectangle = 1,

    /// <summary>
    /// 多边形
    /// </summary>
    [Description("多边形")] 
    Polygon = 2
}