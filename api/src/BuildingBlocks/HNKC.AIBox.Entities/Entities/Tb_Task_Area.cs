using HNKC.AIBox.Enums;
using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 任务区域关联表
/// </summary>
[SugarTable("tb_task_area", "任务区域关联表")]
public class Tb_Task_Area : BaseEntity
{
    /// <summary>
    /// 任务ID
    /// </summary>
    [SugarColumn(ColumnName = "task_id")]
    public string TaskId { get; set; }

    /// <summary>
    /// 模型算法ID
    /// </summary>
    [SugarColumn(ColumnName = "algo_mode_id")]
    public string AlgoModeId { get; set; }

    /// <summary>
    /// 区域名称
    /// </summary>
    [SugarColumn(ColumnName = "name")]
    public string Name { get; set; }

    /// <summary>
    /// 规则类型
    /// </summary>
    [SugarColumn(ColumnName = "rule_type")]
    public RuleTypeEnum RuleType { get; set; }

    /// <summary>
    /// 区域类型
    /// </summary>
    [SugarColumn(ColumnName = "region_type")]
    public RegionTypeEnum RegionType { get; set; }

    /// <summary>
    /// 矩形区域
    /// </summary>
    [SugarColumn(ColumnName = "points")]
    public string Points { get; set; }
}