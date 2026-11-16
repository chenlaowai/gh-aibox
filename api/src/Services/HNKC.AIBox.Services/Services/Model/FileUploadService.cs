using HNKC.AIBox.Exceptions;
using Microsoft.AspNetCore.Http;
using System.IO;

namespace HNKC.AIBox.Services;

public class FileUploadService : IFileUploadService
{
    /// <summary>
    /// 获取安全的文件名
    /// </summary>
    /// <param name="fileName"></param>
    /// <returns></returns>
    public string? GetSafeFileName(string fileName)
    {
        var safeFileName = Path.GetFileName(fileName);
        return string.IsNullOrEmpty(safeFileName) || safeFileName.Contains("..") ? null : safeFileName;
    }

    /// <summary>
    /// 上传文件
    /// </summary>
    /// <param name="file"></param>
    /// <param name="allowedExtensions"></param>
    /// <param name="targetDirectory"></param>
    /// <param name="maxFileSize"></param>
    /// <returns></returns>
    public async Task<string> UploadFileAsync(IFormFile file, string[] allowedExtensions, string targetDirectory,
        int? maxFileSize = null)
    {
        if (file == null || file.Length == 0)
            throw new ArgumentException("没有文件上传");

        var safeFileName = GetSafeFileName(file.FileName);
        if (string.IsNullOrEmpty(safeFileName))
            throw new ArgumentException("文件名无效");

        if (maxFileSize.HasValue && file.Length > maxFileSize.Value)
            throw new ResultMessageException("文件大小超过最大限制");

        var fileExtension = Path.GetExtension(safeFileName).ToLowerInvariant();
        if (!allowedExtensions.Contains(fileExtension))
            throw new ResultMessageException("不支持的文件类型");

        if (!Directory.Exists(targetDirectory))
        {
            Directory.CreateDirectory(targetDirectory);
        }

        var fullFilePath = Path.Combine(targetDirectory, safeFileName);

        if (File.Exists(fullFilePath))
        {
            var fileNameWithoutExt = Path.GetFileNameWithoutExtension(safeFileName);
            var extension = Path.GetExtension(safeFileName);
            var timestamp = DateTime.Now.ToString("yyyyMMddHHmmss");
            safeFileName = $"{fileNameWithoutExt}_{timestamp}{extension}";
            fullFilePath = Path.Combine(targetDirectory, safeFileName);
        }
        
        await using var fileStream = file.OpenReadStream();
        await using var targetStream = File.Create(fullFilePath);
        await fileStream.CopyToAsync(targetStream);

        return Path.GetRelativePath(Directory.GetCurrentDirectory(), fullFilePath);
    }

    /// <summary>
    /// 删除文件
    /// </summary>
    /// <param name="filePath"></param>
    /// <returns></returns>
    public Task<bool> DeleteFileAsync(string filePath)
    {
        if (File.Exists(filePath))
        {
            File.Delete(filePath);
            return Task.FromResult(true);
        }

        return Task.FromResult(false);
    }
}