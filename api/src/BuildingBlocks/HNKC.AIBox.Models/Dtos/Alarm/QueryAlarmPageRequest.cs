namespace HNKC.AIBox.Models;

public class QueryAlarmPageRequest : PageRequest
{
    /// <summary>
    /// 告警任务
    /// </summary>
    public string TaskId { get; set; }

    /// <summary>
    /// 告警通道
    /// </summary>
    public string ChannelId { get; set; }

    /// <summary>
    /// 告警类别
    /// </summary>
    public string TypeId { get; set; }

    /// <summary>
    /// 告警开始时间
    /// </summary>
    public DateTime? AlarmStartTime { get; set; }

    /// <summary>
    /// 告警结束时间
    /// </summary>
    public DateTime? AlarmEndTime { get; set; }
}