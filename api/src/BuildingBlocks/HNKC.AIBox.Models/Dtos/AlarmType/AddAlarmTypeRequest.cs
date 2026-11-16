using System.ComponentModel.DataAnnotations;

namespace HNKC.AIBox.Models;

public class AddAlarmTypeRequest
{
    /// <summary>
    /// 告警类别
    /// </summary>
    [Required(ErrorMessage = "告警类别 是必填项")]
    public string Code { get; set; }

    /// <summary>
    /// 所属算法ID
    /// </summary>
    public string AlgoId { get; set; }

    /// <summary>
    /// 描述备注
    /// </summary>
    public string Remark { get; set; }
}