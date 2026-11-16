using HNKC.AIBox.Enums;
namespace HNKC.AIBox.Models;

public class GetAlarmDetailResult
{
    /// <summary>
    ///  主键
    ///</summary>
    public string Id { get; set; }

    /// <summary>
    /// 任务名称(从Tb_Task获取)
    /// </summary>
    public string TaskCode { get; set; }

    /// <summary>
    /// 通道地址(从Tb_Channel获取)
    /// </summary>
    public string ChannelAddress { get; set; }

    /// <summary>
    /// 告警内容(从Tb_Alarm_Type获取)
    /// </summary>
    public string AlarmTypeCode { get; set; }

    /// <summary>
    /// 告警描述(从Tb_Alarm_Type获取)
    /// </summary>
    public string AlarmTypeRemark { get; set; }

    /// <summary>
    /// 上报状态
    /// </summary>
    public ReportStatusEnum ReportStatus { get; set; }

    /// <summary>
    /// 上报地址
    /// </summary>
    public string ReportAddress { get; set; }

    /// <summary>
    /// 图片路径
    /// </summary>
    public string ImgPath { get; set; }

    /// <summary>
    /// 视频路径
    /// </summary>
    public string VideoPath { get; set; }
    
    /// <summary>
    /// 告警人数
    /// </summary>
    public int? ReportNums { get; set; }
}