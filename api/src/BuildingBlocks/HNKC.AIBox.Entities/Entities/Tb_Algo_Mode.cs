using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 算法模型表(模型=算法+模型参数)
/// </summary>
[SugarTable("tb_algo_mode", "算法模型表")]
public class Tb_Algo_Mode : BaseEntity
{
    /// <summary>
    /// 算法名称
    /// </summary>
    [SugarColumn(ColumnName = "name")]
    public string Name { get; set; }

    /// <summary>
    /// 算法模型编码
    /// </summary>
    [SugarColumn(ColumnName = "code")]
    public string Code { get; set; }

    /// <summary>
    /// 模型文件路径
    /// </summary>
    [SugarColumn(ColumnName = "file_path")]
    public string FilePath { get; set; }

    /// <summary>
    /// 版本日期
    /// </summary>
    [SugarColumn(ColumnName = "version_date")]
    public DateTime VersionDate { get; set; }

    /// <summary>
    /// 备注
    /// </summary>
    [SugarColumn(ColumnName = "remark")]
    public string Remark { get; set; }

    /// <summary>
    /// 
    /// </summary>
    [SugarColumn(ColumnName = "major_only")]
    public bool MajorOnly { get; set; }
    
    /// <summary>
    /// 
    /// </summary>
    [SugarColumn(ColumnName = "zone_required")]
    public bool ZoneRequired { get; set; }
}