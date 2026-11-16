namespace HNKC.AIBox.Models;

public class GetAlarmTypePageResult
{
    /// <summary>
    /// 主键ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 告警类别
    /// </summary>
    public string Code { get; set; }

    /// <summary>
    /// 所属算法（从tb_algo_mode表中获取）
    /// </summary>
    public string AlgoModeName { get; set; }

    /// <summary>
    /// 告警描述
    /// </summary>
    public string Remark { get; set; }
}