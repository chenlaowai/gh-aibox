namespace HNKC.AIBox.Models;

public class GetAlarmTypeDropdownResult
{
    /// <summary>
    /// 告警类别ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 告警类别
    /// </summary>
    public string Code { get; set; }

    /// <summary>
    /// 告警描述
    /// </summary>
    public string Remark { get; set; }
}