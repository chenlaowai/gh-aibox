using System.ComponentModel.DataAnnotations;
using HNKC.AIBox.Enums;
using HNKC.AIBox.Models.Attributes;

namespace HNKC.AIBox.Models;

public class AddChannelRequest
{
    /// <summary>
    /// 探测协议类型
    /// </summary>
    [Required(ErrorMessage = "探测协议类型 是必填项")]
    [EnumDataType(typeof(ProtocolTypeEnum), ErrorMessage = "无效的协议类型")]
    public ProtocolTypeEnum ProtocolType { get; set; }

    /// <summary>
    /// 视频地址
    /// </summary>
    [Required(ErrorMessage = "视频地址 是必填项")]
    public string Address { get; set; }

    /// <summary>
    /// 监控编码
    /// </summary>
    [Required(ErrorMessage = "监控编码 是必填项")]
    [Trim]
    public string Code { get; set; }

    /// <summary>
    /// 通道名称
    /// </summary>
    [Required(ErrorMessage = "通道名称 是必填项")]
    [Trim]

    public string Name { get; set; }

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