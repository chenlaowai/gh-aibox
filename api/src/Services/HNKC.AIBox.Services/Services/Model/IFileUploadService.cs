using Microsoft.AspNetCore.Http;

namespace HNKC.AIBox.Services;

/// <summary>
/// 模型上传服务
/// </summary>
public interface IFileUploadService
{
    /// <summary>
    /// 获取安全的文件名
    /// </summary>
    /// <param name="fileName"></param>
    /// <returns></returns>
    string? GetSafeFileName(string fileName);

    /// <summary>
    /// 上传文件
    /// </summary>
    /// <param name="file"></param>
    /// <param name="allowedExtensions"></param>
    /// <param name="targetDirectory"></param>
    /// <param name="maxFileSize"></param>
    /// <returns></returns>
    Task<string> UploadFileAsync(IFormFile file, string[] allowedExtensions, string targetDirectory,
        int? maxFileSize = null);

    /// <summary>
    /// 删除文件
    /// </summary>
    /// <param name="filePath"></param>
    /// <returns></returns>
    Task<bool> DeleteFileAsync(string filePath);
}