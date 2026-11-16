using HNKC.AIBox.Enums;

namespace HNKC.AIBox.Models;

public class GetChannelDetailResult
{
    /// <summary>
    /// Id
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 探测协议类型
    /// </summary>
    public ProtocolTypeEnum ProtocolType { get; set; }

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

    /// <summary>
    /// 通道协议配置
    /// </summary>
    public string ChannelConfig { get; set; }

    /// <summary>
    /// 监控描述
    /// </summary>
    public string Remark { get; set; }

    /// <summary>
    /// 服务地址
    /// </summary>
    public string ServiceAddress { get; set; }

    /// <summary>
    /// 账户ID
    /// </summary>
    public int AccountId { get; set; }

    /// <summary>
    /// 账户密钥
    /// </summary>
    public string AccountKey { get; set; }
}