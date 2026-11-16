using HNKC.AIBox.Enums;
using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 任务表
/// </summary>
[SugarTable("tb_task", "任务表")]
public class Tb_Task : BaseEntity
{
    /// <summary>
    /// 任务编号
    /// </summary>
    [SugarColumn(ColumnName = "code")]
    public string Code { get; set; }

    /// <summary>
    /// 任务状态
    /// </summary>
    [SugarColumn(ColumnName = "status")]
    public TaskStatusEnum Status { get; set; }

    /// <summary>
    /// 描述备注
    /// </summary>
    [SugarColumn(ColumnName = "remark")]
    public string Remark { get; set; }

    /// <summary>
    /// 通道ID(视频源)
    /// </summary>
    [SugarColumn(ColumnName = "channel_id")]
    public string ChannelId { get; set; }

    /// <summary>
    /// 上报地址
    /// </summary>
    [SugarColumn(ColumnName = "report_address")]
    public string ReportAddress { get; set; }

    /// <summary>
    /// 推理间隔帧数
    /// </summary>
    [SugarColumn(ColumnName = "frame_interval")]
    public int FrameInterval { get; set; }


    /// <summary>
    /// 告警触发帧数
    /// </summary>
    [SugarColumn(ColumnName = "detect_threshold")]
    public int DetectThreshold { get; set; }

    /// <summary>
    /// 告警触发时间区间
    /// </summary>
    [SugarColumn(ColumnName = "detect_section")]
    public int DetectSection { get; set; }

    /// <summary>
    /// 告警时间视频存储时长
    /// </summary>
    [SugarColumn(ColumnName = "video_duration")]
    public int VideoDuration { get; set; }


    /// <summary>
    /// 告警上报事件冷却时间
    /// </summary>
    [SugarColumn(ColumnName = "cooldown_time")]
    public int CooldownTime { get; set; }


    

}