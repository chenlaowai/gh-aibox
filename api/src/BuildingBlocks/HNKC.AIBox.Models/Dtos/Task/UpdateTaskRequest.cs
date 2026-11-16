using System.ComponentModel.DataAnnotations;
using HNKC.AIBox.Models.Attributes;

namespace HNKC.AIBox.Models;

public class UpdateTaskRequest
{
    /// <summary>
    /// 主键ID
    /// </summary>
    [Required(ErrorMessage = "更新操作时Id不能为空")]
    public string Id { get; set; }

    /// <summary>
    /// 任务编号
    /// </summary>
    [Required(ErrorMessage = "任务编号 是必填项")]
    [Trim]
    public string Code { get; set; }

    /// <summary>
    /// 任务描述
    /// </summary>
    public string Remark { get; set; }

    /// <summary>
    /// 通道ID(视频源)
    /// </summary>
    public string ChannelId { get; set; }

    /// <summary>
    /// 上报地址
    /// </summary>
    public string ReportAddress { get; set; }

    /// <summary>
    /// 推理间隔帧数
    /// </summary>
    public int FrameInterval { get; set; }


    /// <summary>
    /// 告警触发帧数
    /// </summary>
    public int DetectThreshold { get; set; }

    /// <summary>
    /// 告警触发时间区间
    /// </summary>
    public int DetectSection { get; set; }

    /// <summary>
    /// 告警时间视频存储时长
    /// </summary>
    public int VideoDuration { get; set; }


    /// <summary>
    /// 告警上报事件冷却时间
    /// </summary>
    public int CooldownTime { get; set; }


    /// <summary>
    /// 任务算法关联
    /// </summary>
    public List<TaskAlgoRequest> TaskAlgoRequest { get; set; } = new();
}