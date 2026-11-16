using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 告警类别表
/// </summary>
[SugarTable("tb_alarm_type", "告警类别表")]
public class Tb_Alarm_Type : BaseEntity
{
    /// <summary>
    /// 告警类别
    /// </summary>
    [SugarColumn(ColumnName = "code")]
    public string Code { get; set; }

    /// <summary>
    /// 所属算法ID
    /// </summary>
    [SugarColumn(ColumnName = "algo_id")]
    public string AlgoId { get; set; }

    /// <summary>
    /// 描述备注
    /// </summary>
    [SugarColumn(ColumnName = "remark")]
    public string Remark { get; set; }
}