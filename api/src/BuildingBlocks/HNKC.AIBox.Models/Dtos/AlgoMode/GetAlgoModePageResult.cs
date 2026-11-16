namespace HNKC.AIBox.Models;

public class GetAlgoModePageResult
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
    /// 模型文件路径
    /// </summary>
    public string FilePath { get; set; }

    /// <summary>
    /// 版本日期
    /// </summary>
    public DateTime VersionDate { get; set; }

    /// <summary>
    /// 备注
    /// </summary>
    public string Remark { get; set; }
}