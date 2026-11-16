using System.ComponentModel.DataAnnotations;
using HNKC.AIBox.Enums;

namespace HNKC.AIBox.Models;

public class UpdateTaskAreaRequest
{
    /// <summary>
    /// 主键ID
    /// </summary>
    [Required(ErrorMessage = "更新操作时Id不能为空")]
    public string Id { get; set; }

    /// <summary>
    /// 区域类型
    /// </summary>
    [Required(ErrorMessage = "区域类型 是必填项")]
    [EnumDataType(typeof(RegionTypeEnum), ErrorMessage = "无效的区域类型")]
    public RegionTypeEnum RegionType { get; set; }

    /// <summary>
    /// 任务区域
    /// </summary>
    public List<Point> Points { get; set; } = new();
}