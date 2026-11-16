using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    /// <summary>
    /// 设备信息
    /// </summary>
    public class DeviceInfoResult
    {
        /// <summary>
        /// 设备标识
        /// </summary>
        public string DeviceIdentification { get; set; }

        /// <summary>
        /// 授权状态（0：未授权，1：已授权）
        /// </summary>
        public int AuthorizationStatus { get; set; }

        /// <summary>
        /// 软件版本
        /// </summary>
        public string SoftwareVersion { get; set; }

        /// <summary>
        /// 资源信息
        /// </summary>
        public int ResourceInformation { get; set; }

        /// <summary>
        /// 系统版本
        /// </summary>
        public string SystemVersion { get; set; }

        /// <summary>
        /// 盒子时间
        /// </summary>
        public DateTime BoxTime { get; set; }

        /// <summary>
        /// 芯片温度
        /// </summary>
        public float ChipTemperature { get; set; }

        /// <summary>
        /// 显卡温度
        /// </summary>
        public string GraphicsCardTemperature { get; set; }

        /// <summary>
        /// 占用率
        /// </summary>
        public List<UsageResult> Usages { get; set; } = new();
    }

    public class UsageResult
    {
        /// <summary>
        /// 名称
        /// </summary>
        public string Name { get; set; }

        /// <summary>
        /// 占用率
        /// </summary>
        public double Percent { get; set; }
    }
}