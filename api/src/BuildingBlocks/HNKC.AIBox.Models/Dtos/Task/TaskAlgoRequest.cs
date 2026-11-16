namespace HNKC.AIBox.Models;

public class TaskAlgoRequest
{
    /// <summary>
    /// 算法ID
    /// </summary>
    public string AlgoId { get; set; }

    /// <summary>
    /// 算法参数
    /// </summary>
    public List<TaskAlgoConfig> ValueConfig { get; set; }
}


