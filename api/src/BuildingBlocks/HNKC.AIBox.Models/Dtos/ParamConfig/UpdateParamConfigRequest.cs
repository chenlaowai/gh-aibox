using System.ComponentModel.DataAnnotations;

namespace HNKC.AIBox.Models;

public class UpdateParamConfigRequest
{
    /// <summary>
    /// Id
    /// </summary>
    public string Id { get; set; }


    /// <summary>
    /// 参数值
    /// </summary>
    [Required(ErrorMessage = "参数值 是必填项")]
    public string ParamValue { get; set; }
}