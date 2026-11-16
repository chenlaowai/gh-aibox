using System;

namespace HNKC.AIBox.Models.Dtos.Alarm
{
    /// <summary>
    /// 告警上报 DTO
    /// </summary>
    public class AlarmReportDto
    {
        /// <summary>
        /// 告警ID
        /// </summary>
        public string AlarmId { get; set; }

        /// <summary>
        /// 任务名称
        /// </summary>
        public string TaskName { get; set; }

        /// <summary>
        /// 通道名称
        /// </summary>
        public string ChannelName { get; set; }

        /// <summary>
        /// 告警类别名称
        /// </summary>
        public string AlarmTypeName { get; set; }

        /// <summary>
        /// 告警时间
        /// </summary>
        public DateTime AlarmTime { get; set; }

        /// <summary>
        /// 图片 Base64
        /// </summary>
        public string ImageBase64 { get; set; }

        /// <summary>
        /// 视频 Base64
        /// </summary>
        public string VideoBase64 { get; set; }
    }
}
