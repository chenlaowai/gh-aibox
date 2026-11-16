namespace HNKC.AIBox.Services;

public interface ICpuMetricsService
{
    /// <summary>
    /// 获取Linux CPU使用率
    /// </summary>
    /// <param name="durationMs"></param>
    /// <returns></returns>
    Task<double> GetLinuxCpuUsageAsync(int durationMs);

    /// <summary>
    /// 获取Windows CPU使用率
    /// </summary>
    /// <returns></returns>
    Task<double> GetWindowsCpuUsageAsync();
}