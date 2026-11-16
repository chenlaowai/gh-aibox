namespace HNKC.AIBox.Models;

public class GetTaskDetailResult
{
    /// <summary>
    /// 主键ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 任务编号
    /// </summary>
    public string Code { get; set; }

    /// <summary>
    /// 任务描述
    /// </summary>
    public string Remark { get; set; }

    /// <summary>
    /// 通道ID
    /// </summary>
    public string ChannelId { get; set; }

    /// <summary>
    /// 视频编号（从Tb_Channel获取）
    /// </summary>
    public string ChannelCode { get; set; }

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
    public List<TaskAlgoResult> TaskAlgoResult { get; set; } = new();
}

/// <summary>
/// 任务算法关联
/// </summary>
public class TaskAlgoResult
{
    /// <summary>
    /// 算法ID
    /// </summary>
    public string AlgoId { get; set; }

    /// <summary>
    /// 算法名称
    /// </summary>
    public string AlgoName { get; set; }

    /// <summary>
    /// 值配置
    /// </summary>
    public List<TaskAlgoConfig> ValueConfig { get; set; }
}