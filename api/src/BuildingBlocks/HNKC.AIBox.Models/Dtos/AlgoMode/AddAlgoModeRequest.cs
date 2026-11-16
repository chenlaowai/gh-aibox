using System.ComponentModel.DataAnnotations;
using Microsoft.AspNetCore.Http;

namespace HNKC.AIBox.Models;

public class AddAlgoModeRequest
{
    /// <summary>
    /// 算法名称
    /// </summary>
    [Required(ErrorMessage = "算法名称 是必填项")]
    public string Name { get; set; }

    /// <summary>
    /// 模型
    /// </summary>
    public IFormFile Model { get; set; }

    /// <summary>
    /// 备注
    /// </summary>
    public string Remark { get; set; }
}