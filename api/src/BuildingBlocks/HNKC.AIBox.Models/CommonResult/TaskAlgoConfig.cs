using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models;

public class TaskAlgoConfig
{

    /// <summary>
    /// 配置ID
    /// </summary>
    public string ConfigId { get; set; }

    /// 参数名称
    /// </summary>
    public string Name { get; set; }

    /// <summary>
    /// 参数描述
    /// </summary>
    public string Remark { get; set; }

    /// <summary>
    /// 参数标识
    /// </summary>
    public string Code { get; set; }

    /// <summary>
    /// 阈值，配置值,缺省值
    /// </summary>
    public string ConfigValue { get; set; }


    /// <summary>
    /// 值类型
    /// </summary>
    public string ValueType { get; set; }
}

