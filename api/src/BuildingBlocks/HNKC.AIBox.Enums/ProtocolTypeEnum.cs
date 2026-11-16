using System.ComponentModel;

namespace HNKC.AIBox.Enums;

/// <summary>
/// 探测协议类型
/// </summary>
public enum ProtocolTypeEnum
{
    /// <summary>
    /// 手动填写 RTSP/RTMP/HLS/设备内文件路径 作为视频地址
    /// </summary>
    [Description("手动填写-手动填写 RTSP/RTMP/HLS/设备内文件路径 作为视频地址")]
    ManualFilling = 0,

    /// <summary>
    /// 私有协议
    /// </summary>
    [Description("DSS-私有协议")] DSS = 1,

    /// <summary>
    /// 通过萤石云动态获取特定相机的视频地址
    /// </summary>
    [Description("EZV-通过萤石云动态获取特定相机的视频地址")]
    EZV = 2,

    /// <summary>
    /// 通过海康综合安防管理平台(iSecure Center)动态获取特定相机的视频地址
    /// </summary>
    [Description("ISC-通过海康综合安防管理平台(iSecure Center)动态获取特定相机的视频地址")]
    ISC = 3,

    /// <summary>
    /// 获取注册到 GB28181 本地服务的视频通道
    /// </summary>
    [Description("ISIP-获取注册到 GB28181 本地服务的视频通道")]
    ISIP = 4,

    /// <summary>
    /// 通过Onvif协议探测相机内可用的视频地址
    /// </summary>
    [Description("ONVIF-通过Onvif协议探测相机内可用的视频地址")]
    ONVIF = 5,
}