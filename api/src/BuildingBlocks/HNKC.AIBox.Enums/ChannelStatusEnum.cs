using System.ComponentModel;

namespace HNKC.AIBox.Enums;

/// <summary>
/// 通道状态
/// </summary>
public enum ChannelStatusEnum
{
    /// <summary>
    /// 在线
    /// </summary>
    [Description("在线")] 
    Online = 1,

    /// <summary>
    /// 离线
    /// </summary>
    [Description("离线")] 
    Offline = 2,
}