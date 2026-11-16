using HNKC.AIBox.Enums;

namespace HNKC.AIBox.Models;

public class GetTaskPageResult
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
    /// 视频编号（从Tb_Channel获取）
    /// </summary>
    public string ChannelCode { get; set; }

    /// <summary>
    /// 通道名称
    /// </summary>
    public string ChannelName { get; set; }

    /// <summary>
    /// 任务算法列表
    /// </summary>
    public List<string> ModelNames { get; set; } = new();

    /// <summary>
    /// 任务状态
    /// </summary>
    public TaskStatusEnum Status { get; set; }
}