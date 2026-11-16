namespace HNKC.AIBox.Models;

public class UpdateAlgoConfigRequest
{
    /// <summary>
    /// 主键ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 阈值，配置值
    /// </summary>
    public string ConfigValue { get; set; }
}