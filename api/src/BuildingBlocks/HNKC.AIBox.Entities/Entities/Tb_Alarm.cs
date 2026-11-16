using HNKC.AIBox.Enums;
using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 预警表
/// </summary>
[SugarTable("tb_alarm", "预警表")]
public class Tb_Alarm
{
    /// <summary>
    ///  主键
    ///</summary>
    [SugarColumn(IsPrimaryKey = true, ColumnName = "id")]
    public string Id { get; set; }

    /// <summary>
    ///  是否删除
    ///</summary>
    [SugarColumn(ColumnName = "is_delete")]
    public bool IsDelete { get; set; }

    /// <summary>
    /// 任务ID
    /// </summary>
    [SugarColumn(ColumnName = "task_id")]
    public string TaskId { get; set; }

    /// <summary>
    /// 通道ID
    /// </summary>
    [SugarColumn(ColumnName = "channel_id")]
    public string ChannelId { get; set; }

    /// <summary>
    /// 告警类别ID
    /// </summary>
    [SugarColumn(ColumnName = "type_id")]
    public string TypeId { get; set; }

    /// <summary>
    /// 告警时间
    /// </summary>
    [SugarColumn(ColumnName = "alarm_time")]
    public DateTime AlarmTime { get; set; }

    /// <summary>
    /// 上报状态
    /// </summary>
    [SugarColumn(ColumnName = "report_status")]
    public ReportStatusEnum ReportStatus { get; set; }

    /// <summary>
    /// 上报地址
    /// </summary>
    [SugarColumn(ColumnName = "report_address")]
    public string ReportAddress { get; set; }


    /// <summary>
    /// 告警人数
    /// </summary>
    [SugarColumn(ColumnName = "report_nums")]
    public int? ReportNums { get; set; }

    /// <summary>
    /// 图片路径
    /// </summary>
    [SugarColumn(ColumnName = "img_path")]
    public string ImgPath { get; set; }

    /// <summary>
    /// 视频路径
    /// </summary>
    [SugarColumn(ColumnName = "video_path")]
    public string VideoPath { get; set; }

    /// <summary>
    /// 更新时间
    /// </summary>
    [SugarColumn(ColumnName = "modified")]
    public DateTime Modified { get; set; }
}