using HNKC.AIBox.Enums;

namespace HNKC.AIBox.Models;

public class GetAlgoModeDropdownResult
{
    /// <summary>
    /// 算法模型ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 算法名称
    /// </summary>
    public string Name { get; set; }

    /// <summary>
    /// 授权状态
    /// </summary>
    public ModeAuthorizationEnum AuthorizationStatus { get; set; }
}