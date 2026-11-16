namespace HNKC.AIBox.Models;

public class GetParamConfigPageResult
{
    /// <summary>
    /// 主键ID
    /// </summary>
    public string Id { get; set; }

    /// <summary>
    /// 参数名称
    /// </summary>
    public string Name { get; set; }

    /// <summary>
    /// 参数标识
    /// </summary>
    public string Code { get; set; }

    /// <summary>
    /// 参数类型
    /// </summary>
    public string ParamType { get; set; }

    /// <summary>
    /// 参数值
    /// </summary>
    public string ParamValue { get; set; }

    /// <summary>
    /// 描述备注
    /// </summary>
    public string Remark { get; set; }
}