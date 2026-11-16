using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    public class AddAIAlarmRequest
    {

        /// <summary>
        ///任务ID
        /// </summary>
        public string task_id { get; set; }

        /// <summary>
        /// 模型名称
        /// </summary>
        public string model_name { get; set; }

        /// <summary>
        /// 告警类别名称
        /// </summary>
        public string report_content { get; set; }

        /// <summary>
        /// 告警人员数量
        /// </summary>
        public int report_num { get; set; }

        /// <summary>
        /// 告警事件时间
        /// </summary>
        public DateTime time { get; set; }

        /// <summary>
        /// 告警图片绝对路径
        /// </summary>
        public string image { get; set; }


        /// <summary>
        /// 告警视频绝对路径
        /// </summary>
        public string video { get; set; }

    }

}
