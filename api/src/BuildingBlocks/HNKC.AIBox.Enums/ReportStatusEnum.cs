using System.ComponentModel;

namespace HNKC.AIBox.Enums;

/// <summary>
/// 报警上传状态枚举
/// </summary>
public enum ReportStatusEnum
{
    /// <summary>
    /// 待上传
    /// </summary>
    [Description("待上传")]
     Pending = 0,

    /// <summary>
    /// 上传成功
    /// </summary>
    [Description("上传成功")]
    Success = 1,

    /// <summary>
    /// 上传失败
    /// </summary>
    [Description("上传失败")]
    Failed = 2,

  
}