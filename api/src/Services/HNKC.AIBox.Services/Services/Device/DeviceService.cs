using System.Diagnostics;
using System.Globalization;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Utils;
using Microsoft.Extensions.Logging;

namespace HNKC.AIBox.Services
{
    public class DeviceService(ICpuMetricsService cpuMetricsService, ILogger<DeviceService> logger) : IDeviceService
    {
        /// <summary>
        /// 获取设备标识
        /// </summary>
        /// <returns></returns>
        public string GetDeviceIdentification()
        {
            try
            {
                var macAddress = NetworkInterface.GetAllNetworkInterfaces()
                    .Where(nic => nic.OperationalStatus == OperationalStatus.Up &&
                                  nic.NetworkInterfaceType != NetworkInterfaceType.Loopback)
                    .Select(nic => nic.GetPhysicalAddress().ToString())
                    .FirstOrDefault();

                return !string.IsNullOrEmpty(macAddress) ? macAddress : "Unknown";
            }
            catch
            {
                return "Unknown";
            }
        }

        /// <summary>
        /// 获取系统版本
        /// </summary>
        /// <returns></returns>
        /// <exception cref="NotImplementedException"></exception>
        public async Task<string> GetSystemVersionAsync()
        {
            string systemVersion = "Unknown";
            try
            {
                if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                {
                    if (File.Exists("/etc/os-release"))
                    {
                        var osInfo = await File.ReadAllLinesAsync("/etc/os-release");
                        var prettyName = osInfo.FirstOrDefault(line => line.StartsWith("PRETTY_NAME="));
                        if (!string.IsNullOrEmpty(prettyName))
                        {
                            systemVersion = prettyName.Split('=')[1].Trim('"');
                        }
                    }

                    if (systemVersion == "Unknown")
                    {
                        systemVersion = $"{Environment.OSVersion.VersionString}";
                    }
                }
                else
                {
                    systemVersion = $"{Environment.OSVersion.VersionString} {Environment.OSVersion.ServicePack}";
                }
            }
            catch
            {
                systemVersion = $"{Environment.OSVersion.VersionString}";
            }

            return systemVersion;
        }
        /// <summary>
        /// 获取CPU温度
        /// </summary>
        /// <returns></returns>
        public async Task<int> GetCpuTemperatureAsync()
        {
            const int DEFAULT_TEMP = 0;
            const int ERROR_TEMP = -1;

            try
            {
                if (!RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                {
                    Console.WriteLine($"当前系统不是Linux系统，无法获取CPU温度。当前系统: {RuntimeInformation.OSDescription}");
                    return DEFAULT_TEMP;
                }

                // 定义多个备选路径，按优先级排序
                string[] temperaturePaths = {
            "/host_sys/thermal/thermal_zone0/temp",  // 您的确认路径（最高优先级）
            "/sys/class/thermal/thermal_zone0/temp",  // 标准Linux路径
            "/sys/class/hwmon/hwmon0/temp1_input",   // 硬件监控路径1
            "/sys/class/hwmon/hwmon1/temp1_input"    // 硬件监控路径2
        };

                foreach (string path in temperaturePaths)
                {
                    try
                    {
                        int temperature = await TryReadTemperatureFromPath(path);
                        if (temperature > 0)
                        {
                            Console.WriteLine($"成功从路径 {path} 读取CPU温度: {temperature}°C");
                            return temperature;
                        }
                    }
                    catch (Exception ex)
                    {
                        // 记录路径尝试失败，但不终止循环
                        Console.WriteLine($"尝试路径 {path} 失败: {ex.Message}");
                        continue;
                    }
                }

                Console.WriteLine("所有温度文件路径尝试均失败，请检查Docker挂载配置");
                return ERROR_TEMP;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"获取CPU温度发生异常: {ex.Message}");
                return ERROR_TEMP;
            }
        }

        /// <summary>
        /// 尝试从指定路径读取温度值
        /// </summary>
        private async Task<int> TryReadTemperatureFromPath(string filePath)
        {
            if (!File.Exists(filePath))
            {
                Console.WriteLine($"温度文件不存在: {filePath}");
                return -1;
            }

            try
            {
                string tempContent = await File.ReadAllTextAsync(filePath);
                if (string.IsNullOrWhiteSpace(tempContent))
                {
                    Console.WriteLine($"温度文件内容为空: {filePath}");
                    return -1;
                }

                string trimmedContent = tempContent.Trim();
                if (int.TryParse(trimmedContent, out int millidegrees) && millidegrees > 0)
                {
                    int celsius = millidegrees / 1000;

                    // 简单合理性检查（CPU温度通常在0-100°C之间）
                    if (celsius >= 0 && celsius <= 120)
                    {
                        return celsius;
                    }
                    else
                    {
                        Console.WriteLine($"温度值不合理: {celsius}°C，原始值: {millidegrees}");
                        return -1;
                    }
                }
                else
                {
                    Console.WriteLine($"温度内容解析失败: {trimmedContent}");
                    return -1;
                }
            }
            catch (UnauthorizedAccessException ex)
            {
                Console.WriteLine($"没有权限读取温度文件 {filePath}: {ex.Message}");
                return -1;
            }
            catch (IOException ex)
            {
                Console.WriteLine($"读取温度文件时发生IO错误 {filePath}: {ex.Message}");
                return -1;
            }
        }


        #region 获取显卡温度

        /// <summary>
        /// 获取显卡温度（对于无独立显卡的嵌入式系统，通常返回SoC或CPU温度）
        /// </summary>
        /// <returns>返回温度值（如"45.3°C"）或"无"</returns>
        public async Task<string> GetGpuTemperatureAsync()
        {
            try
            {
                // 方法1: 优先尝试读取Linux thermal zone中的GPU温度
                string gpuTemp = await ReadGPUTemperatureFromThermalZoneAsync();
                if (!string.IsNullOrEmpty(gpuTemp))
                {
                    return gpuTemp;
                }

                // 方法2: 如果没有明确的GPU热区，尝试读取CPU温度作为替代
                string cpuTemp = await ReadCPUTemperatureFromThermalZoneAsync();
                if (!string.IsNullOrEmpty(cpuTemp))
                {
                    return cpuTemp;
                }

                // 方法3: 尝试使用sensors命令（如果已安装lm-sensors）
                string sensorsTemp = await ReadTemperatureFromSensorsCommandAsync();
                if (!string.IsNullOrEmpty(sensorsTemp))
                {
                    return sensorsTemp;
                }

                return "无";
            }
            catch (Exception ex)
            {
                // 记录日志
                Console.WriteLine($"获取温度时发生错误: {ex.Message}");
                return "无";
            }
        }

        /// <summary>
        /// 从Linux thermal zone读取GPU温度
        /// </summary>
        private async Task<string> ReadGPUTemperatureFromThermalZoneAsync()
        {
            try
            {
                // 遍历thermal_zone目录，查找GPU相关的热区[1,2](@ref)
                for (int i = 0; i < 10; i++) // 通常不会超过10个热区
                {
                    string zonePath = $"/sys/class/thermal/thermal_zone{i}";
                    string typePath = Path.Combine(zonePath, "type");
                    string tempPath = Path.Combine(zonePath, "temp");

                    if (File.Exists(typePath) && File.Exists(tempPath))
                    {
                        string type = (await File.ReadAllTextAsync(typePath)).Trim();

                        // 检查是否为GPU热区[1,2](@ref)
                        if (type.Contains("gpu", StringComparison.OrdinalIgnoreCase) ||
                            type.Contains("gpu-thermal", StringComparison.OrdinalIgnoreCase))
                        {
                            string tempContent = await File.ReadAllTextAsync(tempPath);
                            if (int.TryParse(tempContent.Trim(), out int tempMilliCelsius))
                            {
                                double tempCelsius = tempMilliCelsius / 1000.0;
                                return tempCelsius.ToString();
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"读取GPU thermal zone失败: {ex.Message}");
            }

            return null;
        }

        /// <summary>
        /// 从Linux thermal zone读取CPU温度
        /// </summary>
        private async Task<string> ReadCPUTemperatureFromThermalZoneAsync()
        {
            try
            {
                // 遍历thermal_zone目录，查找CPU相关的热区[1,9](@ref)
                for (int i = 0; i < 10; i++)
                {
                    string zonePath = $"/sys/class/thermal/thermal_zone{i}";
                    string typePath = Path.Combine(zonePath, "type");
                    string tempPath = Path.Combine(zonePath, "temp");

                    if (File.Exists(typePath) && File.Exists(tempPath))
                    {
                        string type = (await File.ReadAllTextAsync(typePath)).Trim();

                        // 检查是否为CPU热区[1,9](@ref)
                        if (type.Contains("cpu", StringComparison.OrdinalIgnoreCase) ||
                            type.Contains("soc", StringComparison.OrdinalIgnoreCase) ||
                            type.Contains("package", StringComparison.OrdinalIgnoreCase) ||
                            type.Contains("CPU-therm", StringComparison.OrdinalIgnoreCase))
                        {
                            string tempContent = await File.ReadAllTextAsync(tempPath);
                            if (int.TryParse(tempContent.Trim(), out int tempMilliCelsius))
                            {
                                double tempCelsius = tempMilliCelsius / 1000.0;
                                return tempCelsius.ToString();
                            }
                        }
                    }
                }

                // 如果没找到特定类型的热区，尝试读取第一个可用的热区
                for (int i = 0; i < 10; i++)
                {
                    string tempPath = $"/sys/class/thermal/thermal_zone{i}/temp";
                    if (File.Exists(tempPath))
                    {
                        string tempContent = await File.ReadAllTextAsync(tempPath);
                        if (int.TryParse(tempContent.Trim(), out int tempMilliCelsius))
                        {
                            double tempCelsius = tempMilliCelsius / 1000.0;
                            return tempCelsius.ToString();
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"读取CPU thermal zone失败: {ex.Message}");
            }

            return null;
        }

        /// <summary>
        /// 使用sensors命令读取温度（需要安装lm-sensors）
        /// </summary>
        private async Task<string> ReadTemperatureFromSensorsCommandAsync()
        {
            try
            {
                using var process = new System.Diagnostics.Process();
                process.StartInfo.FileName = "sensors";
                process.StartInfo.Arguments = "";
                process.StartInfo.RedirectStandardOutput = true;
                process.StartInfo.UseShellExecute = false;
                process.StartInfo.CreateNoWindow = true;

                process.Start();
                string output = await process.StandardOutput.ReadToEndAsync();
                await process.WaitForExitAsync();

                if (process.ExitCode == 0 && !string.IsNullOrWhiteSpace(output))
                {
                    // 解析sensors输出，查找温度信息[3,5,6](@ref)
                    var lines = output.Split('\n');
                    foreach (var line in lines)
                    {
                        if ((line.ToLower().Contains("gpu") || line.ToLower().Contains("core") ||
                             line.ToLower().Contains("cpu")) && line.Contains("°C"))
                        {
                            var match = System.Text.RegularExpressions.Regex.Match(line, @"([+-]?\d+\.?\d*)°C");
                            if (match.Success)
                            {
                                return $"{match.Groups[1].Value}°C";
                            }
                        }
                    }
                }
            }
            catch (System.ComponentModel.Win32Exception)
            {
                // sensors命令不存在，忽略错误
            }
            catch (Exception ex)
            {
                Console.WriteLine($"执行sensors命令失败: {ex.Message}");
            }

            return null;
        }

        #endregion

        /// <summary>
        /// 获取CPU占用率
        /// </summary>
        /// <returns></returns>
        /// <exception cref="NotImplementedException"></exception>
        public async Task<double> GetCpuUsageAsync()
        {
            try
            {
                if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                {
                    return await cpuMetricsService.GetLinuxCpuUsageAsync(1000);
                }

                if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                {
                    return await cpuMetricsService.GetWindowsCpuUsageAsync();
                }
            }
            catch (Exception ex)
            {
                logger.LogError(ex, "获取CPU占用率失败:{message}", ex.Message);
                return 0.0;
            }

            return 0.0;
        }

        /// <summary>
        /// 获取资源信息
        /// </summary>
        /// <returns></returns>
        public int GetDiskInfo()
        {
            try
            {
                string drivePath = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "C:\\" : "/";
                var drive = new DriveInfo(drivePath);
                var totalSpace = drive.TotalSize / (1024 * 1024 * 1024); // GB
                return (int)totalSpace;
            }
            catch
            {
                return 0;
            }
        }
    }
}