using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 参数配置表
/// </summary>
[SugarTable("tb_param_config", "参数配置表")]
public class Tb_Param_Config : BaseEntity
{
    /// <summary>
    /// 参数名称
    /// </summary>
    [SugarColumn(ColumnName = "name")]
    public string Name { get; set; }

    /// <summary>
    /// 参数标识
    /// </summary>
    [SugarColumn(ColumnName = "code")]
    public string Code { get; set; }

    /// <summary>
    /// 参数类型
    /// </summary>
    [SugarColumn(ColumnName = "param_type")]
    public string ParamType { get; set; }

    /// <summary>
    /// 参数值
    /// </summary>
    [SugarColumn(ColumnName = "param_value")]
    public string ParamValue { get; set; }

    /// <summary>
    /// 描述备注
    /// </summary>
    [SugarColumn(ColumnName = "remark")]
    public string Remark { get; set; }

            /// <summary>
        ///  是否展示
        ///</summary>
        [SugarColumn(ColumnName = "is_show")]
        public bool IsShow { get; set; }
}