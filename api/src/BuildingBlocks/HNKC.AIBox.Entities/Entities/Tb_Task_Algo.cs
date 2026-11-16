using System.ComponentModel.DataAnnotations.Schema;
using System.Text.Json;
using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 任务算法授权关联表
/// </summary>
[SugarTable("tb_task_algo", "任务表")]
public class Tb_Task_Algo : BaseEntity
{
    /// <summary>
    /// 任务ID
    /// </summary>
    [SugarColumn(ColumnName = "task_id")]
    public string TaskId { get; set; }

    /// <summary>
    /// 算法ID
    /// </summary>
    [SugarColumn(ColumnName = "algo_id")]
    public string AlgoId { get; set; }

    /// <summary>
    /// 值配置
    /// </summary>
    [SugarColumn(ColumnName = "value_config")]
    public string ValueConfig { get; set; }
}