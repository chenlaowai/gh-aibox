using HNKC.AIBox.Enums;

namespace HNKC.AIBox.Models;

public class GetTaskAreaResult
{
    //TODO 先写死,后面从通道获取
    /// <summary>
    /// base 64图片
    /// </summary>
    public string Photo { get; set; } 
    /// <summary>
    /// 任务ID
    /// </summary>
    public string TaskId { get; set; }

    /// <summary>
    /// 任务编码
    /// </summary>
    public string TaskCode { get; set; }

    /// <summary>
    /// 算法模型
    /// </summary>
    public List<AlgoMode> AlgoModes { get; set; } = new();

    /// <summary>
    /// 任务区域集合
    /// </summary>
    public List<AreaRule> AreaRules { get; set; } = new();
}

/// <summary>
/// 算法模型
/// </summary>
public class AlgoMode
{
    /// <summary>
    /// 算法模型ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 算法名称
    /// </summary>
    public string Name { get; set; }

    /// <summary>
    /// 
    /// </summary>
    public bool MajorOnly { get; set; }

    /// <summary>
    /// 
    /// </summary>
    public bool ZoneRequired { get; set; }
}

/// <summary>
/// 任务区域
/// </summary>
public class AreaRule
{
    /// <summary>
    /// 区域ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 区域标识
    /// </summary>
    public string Name { get; set; }

    /// <summary>
    /// 规则类型
    /// </summary>
    public RuleTypeEnum RuleType { get; set; }

    /// <summary>
    /// 区域类型
    /// </summary>
    public RegionTypeEnum RegionType { get; set; }

    /// <summary>
    /// 算法模型ID
    /// </summary>
    public List<string> AlgoModeId { get; set; } = new List<string>();

    /// <summary>
    /// 区域坐标
    /// </summary>
    public List<Point> Points { get; set; } = new();
}

public class Point
{
    /// <summary>
    /// 点X坐标
    /// </summary>
    public float X { get; set; }

    /// <summary>
    /// 点Y坐标
    /// </summary>
    public float Y { get; set; }
}