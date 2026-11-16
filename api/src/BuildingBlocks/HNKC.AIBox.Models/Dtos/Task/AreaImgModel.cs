using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    /// <summary>
    /// 获取任务区域图片结果
    /// </summary>
    public class AreaImgModel
    {
        /// <summary>
        /// 状态
        /// </summary>
        public string status { get; set; }

        /// <summary>
        /// 消息
        /// </summary>
        public string msg { get; set; }

        /// <summary>
        /// 区域图片
        /// </summary>
        public string frame_area { get; set; }
    }
}
