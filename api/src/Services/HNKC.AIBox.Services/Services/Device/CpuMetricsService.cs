using System.Diagnostics;
using HNKC.AIBox.Exceptions;

namespace HNKC.AIBox.Services;

public class CpuMetricsService : ICpuMetricsService
{
    public async Task<double> GetLinuxCpuUsageAsync(int durationMs)
    {

        return 80.0;
        //string command = $"top -b -n 2 -d {durationMs / 1000.0:F2} | grep 'Cpu(s)' | tail -n 1";

        //using (var process = new Process())
        //{
        //    process.StartInfo = new ProcessStartInfo
        //    {
        //        FileName = "/bin/bash",
        //        Arguments = $"-c \"{command}\"",
        //        RedirectStandardOutput = true,
        //        RedirectStandardError = true,
        //        UseShellExecute = false,
        //        CreateNoWindow = true
        //    };

        //    process.Start();
        //    string output = await process.StandardOutput.ReadToEndAsync();
        //    await process.WaitForExitAsync();

        //    if (process.ExitCode != 0)
        //    {
        //        string error = await process.StandardError.ReadToEndAsync();
        //        throw new ResultMessageException($"top 命令执行失败：{error}");
        //    }

        //    return ParseCpuUsage(output);
        //}
    }

    public async Task<double> GetWindowsCpuUsageAsync()
    {
        var cpuCounter = new PerformanceCounter("Processor", "% Processor Time", "_Total");
        cpuCounter.NextValue();
        await Task.Delay(1000);
        return cpuCounter.NextValue();
    }

    private double ParseCpuUsage(string topOutput)
    {
        int idIndex = topOutput.IndexOf("id");
        if (idIndex == -1)
            throw new ResultMessageException("无效的顶部输出格式");

        int commaIndex = topOutput.LastIndexOf(',', idIndex);
        if (commaIndex == -1)
            throw new ResultMessageException("无效的顶部输出格式");

        string idlePart = topOutput.Substring(commaIndex + 1, idIndex - commaIndex - 1).Trim();

        if (double.TryParse(idlePart, out double idlePercentage))
        {
            // CPU 使用率 = 100% - 空闲百分比
            return 100.0 - idlePercentage;
        }

        throw new ResultMessageException("无法从“top”输出中解析出 CPU 使用率");
    }
}