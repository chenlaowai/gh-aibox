using System.ComponentModel.DataAnnotations;

namespace HNKC.AIBox.Models;

public class AddParamConfigRequest
{
    /// <summary>
    /// 参数名称
    /// </summary>
    [Required(ErrorMessage = "参数名称 是必填项")]
    public string Name { get; set; }


    /// <summary>
    /// 参数值
    /// </summary>
    [Required(ErrorMessage = "参数值 是必填项")]
    public string ParamValue { get; set; }

    /// <summary>
    /// 描述备注
    /// </summary>
    public string Remark { get; set; }

    /// <summary>
    /// 参数标识
    /// </summary>
    public string Code { get; set; }
}