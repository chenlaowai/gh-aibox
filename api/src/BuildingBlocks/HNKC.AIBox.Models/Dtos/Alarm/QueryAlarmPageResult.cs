using HNKC.AIBox.Enums;

namespace HNKC.AIBox.Models.Dtos.Alarm;

public class QueryAlarmPageResult
{
    /// <summary>
    /// 主键
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 任务名称(从Tb_Task获取)
    /// </summary>
    public string TaskCode { get; set; }

    /// <summary>
    /// 通道监控编码(从Tb_Channel获取)
    /// </summary>
    public string ChannelCode { get; set; }

    /// <summary>
    /// 通道名称
    /// </summary>
    public string ChannelName { get; set; }

    /// <summary>
    /// 告警内容(从Tb_Alarm_Type获取)
    /// </summary>
    public string AlarmTypeCode { get; set; }

    /// <summary>
    /// 告警描述
    /// </summary>
    public string AlarmTypeRemark { get; set; }

    /// <summary>
    /// 图片路径
    /// </summary>
    public string ImgPath { get; set; }

    /// <summary>
    /// 告警时间
    /// </summary>
    public DateTime AlarmTime { get; set; }

    /// <summary>
    /// 上报状态
    /// </summary>
    public ReportStatusEnum ReportStatus { get; set; }
}