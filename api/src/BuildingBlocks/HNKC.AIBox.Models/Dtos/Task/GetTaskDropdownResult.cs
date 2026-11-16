namespace HNKC.AIBox.Models;
public class GetTaskDropdownResult
{
    /// <summary>
    /// 主键ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 任务编号
    /// </summary>
    public string Code { get; set; }

    /// <summary>
    /// 子节点
    /// </summary>
    public List<GetTaskDropdownResult> Children { get; set; }
}