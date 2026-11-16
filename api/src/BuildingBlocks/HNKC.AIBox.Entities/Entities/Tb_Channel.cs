using HNKC.AIBox.Enums;
using SqlSugar;

namespace HNKC.AIBox.Entities;

/// <summary>
/// 通道表
/// </summary>
[SugarTable("tb_channel", "通道表")]
public class Tb_Channel : BaseEntity
{
    /// <summary>
    /// 探测协议类型
    /// </summary>
    [SugarColumn(ColumnName = "protocol_type")]
    public ProtocolTypeEnum ProtocolType { get; set; }

    /// <summary>
    /// 视频地址
    /// </summary>
    [SugarColumn(ColumnName = "address")]
    public string Address { get; set; }

    /// <summary>
    /// 监控编码
    /// </summary>
    [SugarColumn(ColumnName = "code")]
    public string Code { get; set; }



    /// <summary>
    /// 通道名称
    /// </summary>
    [SugarColumn(ColumnName = "name")]
    public string Name { get; set; }
    /// <summary>
    /// 通道协议配置
    /// </summary>
    [SugarColumn(ColumnName = "channel_config")]
    public string ChannelConfig { get; set; }

    /// <summary>
    /// 监控描述
    /// </summary>
    [SugarColumn(ColumnName = "remark")]
    public string Remark { get; set; }

    /// <summary>
    /// 服务地址
    /// </summary>
    [SugarColumn(ColumnName = "service_address")]
    public string ServiceAddress { get; set; }

    /// <summary>
    /// 账户ID
    /// </summary>
    [SugarColumn(ColumnName = "account_id")]
    public int AccountId { get; set; }

    /// <summary>
    /// 账户密钥
    /// </summary>
    [SugarColumn(ColumnName = "account_key")]
    public string AccountKey { get; set; }
}