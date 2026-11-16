using System.ComponentModel;

namespace HNKC.AIBox.Enums;

/// <summary>
/// 区域类型
/// </summary>
public enum RegionTypeEnum
{
    /// <summary>
    /// 辅助线
    /// </summary>
    [Description("辅助线")] 
    Auxiliary = 0,
    
    /// <summary>
    /// 兴趣区域
    /// </summary>
    [Description("兴趣区域")] 
    RegionOfInterest = 1,

    /// <summary>
    /// 专用算法区域
    /// </summary>
    [Description("专用算法区域")] 
    SpecialAlgorithmRegion = 2
}