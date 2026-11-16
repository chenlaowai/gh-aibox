namespace HNKC.AIBox.Models;

public class AddAlgoConfigRequest
{
    /// <summary>
    /// 算法名称
    /// </summary>
    public string Name { get; set; }

    /// <summary>
    /// 阈值，配置值
    /// </summary>
    public float ConfigValue { get; set; }

    /// <summary>
    /// 算法模型ID
    /// </summary>
    public string AlgoId { get; set; }
}