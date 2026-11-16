using HNKC.AIBox.Models;

namespace HNKC.AIBox.Services
{
    public interface IDeviceService
    {
        /// <summary>
        /// 获取设备标识
        /// </summary>
        /// <returns></returns>
        string GetDeviceIdentification();

        /// <summary>
        /// 获取系统版本
        /// </summary>
        /// <returns></returns>
        Task<string> GetSystemVersionAsync();

        /// <summary>
        /// 获取CPU温度
        /// </summary>
        /// <returns></returns>
        Task<int> GetCpuTemperatureAsync();

        /// <summary>
        /// 获取显卡温度
        /// </summary>
        /// <returns></returns>
        Task<string> GetGpuTemperatureAsync();

        /// <summary>
        /// 获取CPU占用率
        /// </summary>
        /// <returns></returns>
        Task<double> GetCpuUsageAsync();

        /// <summary>
        /// 获取资源信息
        /// </summary>
        /// <returns></returns>
        int GetDiskInfo();
    }
}