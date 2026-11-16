namespace HNKC.AIBox.Models;

public class DeviceAlertInfoResult
{
    /// <summary>
    /// 告警类型信息
    /// </summary>
    public List<AlertTypeInfo> AlertTypes { get; set; } = new();

    /// <summary>
    /// 任务告警信息
    /// </summary>
    public List<TaskAlertInfo> TaskAlerts { get; set; } = new();

    /// <summary>
    /// 公告信息
    /// </summary>
    public List<Announcement> Announcements { get; set; } = new();

    /// <summary>
    /// 现场告警
    /// </summary>
    public List<DeviceAlarm> DeviceAlarms { get; set; } = new();
}

/// <summary>
/// 公告信息
/// </summary>
public class Announcement
{
    /// <summary>
    /// 公告类型
    /// </summary>
    public int Type { get; set; }

    /// <summary>
    /// 公告标题
    /// </summary>
    public string Title { get; set; }


    /// <summary>
    /// 发布时间
    /// </summary>
    public DateTime PublishedAt { get; set; }
}

/// <summary>
/// 告警类型信息
/// </summary>
public class AlertTypeInfo
{
    /// <summary>
    /// 告警类型代码
    /// </summary>
    public string AlertType { get; set; }

    /// <summary>
    /// 告警次数
    /// </summary>
    public int AlertCount { get; set; }

    /// <summary>
    /// 告警占比
    /// </summary>
    public double Percentage { get; set; }
}

/// <summary>
/// 任务告警信息
/// </summary>
public class TaskAlertInfo
{
    /// <summary>
    /// 任务ID
    /// </summary>
    public string TaskId { get; set; }

    /// <summary>
    /// 任务名称
    /// </summary>
    public string TaskName { get; set; }

    /// <summary>
    /// 告警次数
    /// </summary>
    public int AlertCount { get; set; }

    /// <summary>
    /// 告警占比
    /// </summary>
    public double Percentage { get; set; }
}

/// <summary>
/// 现场告警
/// </summary>
public class DeviceAlarm
{
    /// <summary>
    /// 标题
    /// </summary>
    public string Title { get; set; }

    /// <summary>
    /// 描述信息
    /// </summary>
    public string Description { get; set; }

    /// <summary>
    /// 时间
    /// </summary>
    public DateTime Time { get; set; }


    public int ReportStatus { get; set; }

    /// <summary>
    /// 图片
    /// </summary>
    public object Image { get; set; }
}