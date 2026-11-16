using HNKC.AIBox.Enums;

namespace HNKC.AIBox.Models;

public class GetChannelPageResult
{
    /// <summary>
    /// Id
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 通道状态
    /// </summary>
    public ChannelStatusEnum ChannelStatus { get; set; }

    /// <summary>
    /// 视频地址
    /// </summary>
    public string Address { get; set; }

    /// <summary>
    /// 监控编码
    /// </summary>
    public string Code { get; set; }

    /// <summary>
    /// 通道名称
    /// </summary>
    public string Name { get; set; }
}