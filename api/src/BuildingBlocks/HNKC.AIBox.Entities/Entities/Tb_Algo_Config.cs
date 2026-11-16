using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 算法模型阈值表
/// </summary>
[SugarTable("tb_algo_config", "算法配置表")]
public class Tb_Algo_Config : BaseEntity
{
    /// <summary>
    /// 参数名称
    /// </summary>
    [SugarColumn(ColumnName = "name")]
    public string Name { get; set; }

    /// <summary>
    /// 参数描述
    /// </summary>
    [SugarColumn(ColumnName = "remark")]
    public string Remark { get; set; }

    /// <summary>
    /// 参数标识
    /// </summary>
    [SugarColumn(ColumnName = "code")]
    public string Code { get; set; }


    /// <summary>
    /// 阈值，配置值,缺省值
    /// </summary>
    [SugarColumn(ColumnName = "config_value")]
    public string ConfigValue { get; set; }


    /// <summary>
    /// 值类型
    /// </summary>
    [SugarColumn(ColumnName = "value_type")]
    public string ValueType { get; set; }

    /// <summary>
    /// 算法模型ID
    /// </summary>
    [SugarColumn(ColumnName = "algo_id")]
    public string AlgoId { get; set; }




}