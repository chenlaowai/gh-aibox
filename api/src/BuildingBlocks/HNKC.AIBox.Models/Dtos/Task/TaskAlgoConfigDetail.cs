using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    /// <summary>
    /// 获取任务算法配置详情
    /// </summary>
    /// </summary>
    public class TaskAlgoConfigDetail
    {
        /// <summary>
        /// 任务ID
        /// </summary>
        public string TaskId { get; set; }
        /// <summary>
        /// 任务名称（Code）
        /// </summary>
        public string TaskName { get; set; }

        /// <summary>
        /// 通道地址
        /// </summary>
        public string ChannelAddress { get; set; }

        /// <summary>
        /// 推理间隔帧数
        /// </summary>
        public int FrameInterval { get; set; }


        /// <summary>
        /// 告警触发帧数
        /// </summary>
        public int DetectThreshold { get; set; }

        /// <summary>
        /// 告警触发时间区间
        /// </summary>
        public int DetectSection { get; set; }

        /// <summary>
        /// 告警时间视频存储时长
        /// </summary>
        public int VideoDuration { get; set; }

        /// <summary>
        /// 告警上报事件冷却时间
        /// </summary>
        public int CooldownTime { get; set; }
        /// <summary>
        /// 模型列表
        /// </summary>
        public List<TaskAlgoModel> Model { get; set; }

    }

    public class TaskAlgoModel
    {
        /// <summary>
        /// 模型ID
        /// </summary>
        public string AlogId { get; set; }
        /// <summary>
        /// 算法名称
        /// </summary>
        public string AlgoName { get; set; }


        /// <summary>
        /// 算法模型代码
        /// </summary>
        public string AlgoCode { get; set; }

        /// <summary>
        /// 模型算法配置列表
        /// </summary>
        public List<TaskAlgoConfig> AlgoConfig { get; set; }

        /// <summary>
        /// 任务区域列表
        /// </summary>
        public List<TaskArea> TaskArea { get; set; }

    }

    /// <summary>
    /// 任务区域
    /// </summary>
    public class TaskArea
    {
        /// <summary>
        /// 图形类型（0=辅助线，1=矩形，2=多边形）
        /// </summary>
        public int RegionType { get; set; }
        /// <summary>
        /// 任务区区域点列表
        /// </summary>
        public string Points { get; set; }

        /// <summary>
        /// 算法模型ID
        /// </summary>
        public string AlgoModeId { get; set; }
    }
}
