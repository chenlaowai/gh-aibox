using System.ComponentModel.DataAnnotations;
using HNKC.AIBox.Enums;

namespace HNKC.AIBox.Models;

public class AddTaskAreaRequest
{
    /// <summary>
    /// 任务ID
    /// </summary>
    [Required(ErrorMessage = "任务Id 是必填项")]
    public string TaskId { get; set; }

    /// <summary>
    /// 任务区域集合
    /// </summary>
    public List<AddAreaRule> AreaRules { get; set; } = new();
}

/// <summary>
/// 任务区域添加
/// </summary>
public class AddAreaRule
{
    
    /// <summary>
    /// 区域标识
    /// </summary>
    [Required(ErrorMessage = "区域标识 是必填项")]
    public string Name { get; set; }

    /// <summary>
    /// 规则类型
    /// </summary>
    [Required(ErrorMessage = "规则类型 是必填项")]
    [EnumDataType(typeof(RuleTypeEnum), ErrorMessage = "无效的规则类型")]
    public RuleTypeEnum RuleType { get; set; }

    /// <summary>
    /// 区域类型
    /// </summary>
    [Required(ErrorMessage = "区域类型 是必填项")]
    [EnumDataType(typeof(RegionTypeEnum), ErrorMessage = "无效的区域类型")]
    public RegionTypeEnum RegionType { get; set; }

    /// <summary>
    /// 算法模型ID
    /// </summary>
    [Required(ErrorMessage = "算法模型ID 是必填项")]
    public List<string> AlgoModeId { get; set; } = new List<string>();

    /// <summary>
    /// 区域坐标
    /// </summary>
    public List<Point> Points { get; set; } = new();
}