namespace HNKC.AIBox.Models;

public class GetAlgoConfigPageResult
{
    /// <summary>
    /// 主键ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 算法名称
    /// </summary>
    public string Name { get; set; }

    /// <summary>
    /// 阈值，配置值
    /// </summary>
    public string ConfigValue { get; set; }
    
    /// <summary>
    /// 算法模型名称(从Tb_Algo_Mode获取)
    /// </summary>
    public string AlgoModeName { get; set; }
    
    /// <summary>
    /// 算法模型ID
    /// </summary>
    public string AlgoId { get; set; }
}