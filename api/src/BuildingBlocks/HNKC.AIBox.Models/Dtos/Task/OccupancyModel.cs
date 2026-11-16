using System;

namespace HNKC.AIBox.Models.Dtos.Task
{
    /// <summary>
    /// 资源占用率模型
    /// </summary>
    public class OccupancyModel
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
        /// CPU占用率
        /// </summary>
        public int cpu { get; set; }

        /// <summary>
        /// 内存占用率
        /// </summary>
        public int mem { get; set; }

        /// <summary>
        /// TPU占用率
        /// </summary>
        public int tpu { get; set; }

        /// <summary>
        /// VPU 1占用率
        /// </summary>
        public int vpu_1 { get; set; }

        /// <summary>
        /// VPU 2占用率
        /// </summary>
        public int vpu_2 { get; set; }

        /// <summary>
        /// VPU 3占用率
        /// </summary>
        public int vpu_3 { get; set; }

        /// <summary>
        /// VPP 1占用率
        /// </summary>
        public int vpp_1 { get; set; }

        /// <summary>
        /// VPP 2占用率
        /// </summary>
        public int vpp_2 { get; set; }

        /// <summary>
        /// 内存占用率（百分比）
        /// </summary>
        public int memory { get; set; }

		/// <summary>
		/// 存储剩余空间
		/// </summary>

		public decimal disk_free { get; set; }

		/// <summary>
		/// 芯片温度
		/// </summary>
		public decimal chip_temp { get; set; }

        /// <summary>
        /// 板卡温度
        /// </summary>
        public decimal board_temp { get; set; }


    }
}