using System.Text.RegularExpressions;
using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.Utils;
using HNKCCH.Base.Nuget.Files.Common.Storage;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Configuration;
using SqlSugar;

namespace HNKC.AIBox.Services;

public class FileService(
    IStorage storage,
    IConfiguration configuration,
    IIdentityService identityService,
    ISqlSugarClient db) : IFileService
{
    /// <summary>
    /// 上传文件
    /// </summary>
    /// <param name="file"></param>
    /// <param name="dir"></param>
    /// <param name="fileFilter"></param>
    /// <param name="size"></param>
    /// <returns></returns>
    public string Upload(IFormFile file, string dir = "files", string fileFilter = "", long size = 0)
    {
        var fileName = file.FileName;
        if (string.IsNullOrWhiteSpace(fileName))
            throw new ResultMessageException("文件名不可为空");

        #region 图片文件的条件判断

        //文件后缀
        var fileExtension = Path.GetExtension(file.FileName);

        //判断后缀
        if (!string.IsNullOrWhiteSpace(fileExtension))
        {
            if (fileExtension == null)
                throw new ResultMessageException("上传的文件没有后缀");

            if (fileFilter.IndexOf(fileExtension.ToLower(), StringComparison.Ordinal) <= -1)
                throw new ResultMessageException($"请上传 {fileFilter} 格式的文件");
        }

        if (size > 0)
        {
            long length = file.Length;
            if (length > size)
                throw new ResultMessageException("上传的文件体积过大");
        }

        // 禁止文件名遍历攻击（如 "../"）
        if (Path.GetFullPath(fileName).Contains("..") || fileName.Contains("../") || fileName.Contains("..\\"))
            throw new ResultMessageException("文件名包含非法路径");

        // 限制文件名长度
        if (fileName.Length > 255)
            throw new ResultMessageException("文件名过长");

        #endregion

        string saveName = $"/{dir}/{Guid.NewGuid().ToString() + fileExtension}";
        byte[] bytes;
        using (var stream = file.OpenReadStream())
        {
            bytes = new byte[stream.Length];
            stream.Read(bytes, 0, bytes.Length);
            // 设置当前流的位置为流的开始
            stream.Seek(0, SeekOrigin.Begin);
        }

        var disk = storage.Disk("NAS");
        disk.Put(saveName, bytes);

        return saveName;
    }

    /// <summary>
    /// 获取文件
    /// </summary>
    /// <param name="path"></param>
    /// <returns></returns>
    public Stream Download(string path)
    {
        // 输入验证
        if (string.IsNullOrWhiteSpace(path))
            throw new ResultMessageException("路径无效");

        // 禁止目录遍历攻击（如 "../"）
        if (Path.GetFullPath(path).Contains("..") || path.Contains("../") || path.Contains("..\\"))
            throw new ResultMessageException("非法路径");

        var disk = storage.Disk("NAS");
        return disk.GetStream(path);
    }


    /// <summary>
    /// 删除文件
    /// </summary>
    /// <param name="filePath">文件路径</param>
    /// <returns>是否删除成功</returns>
    public bool Delete(string filePath)
    {
        var disk = storage.Disk("NAS");

        if (disk.Exists(filePath))
        {
            disk.Delete(filePath);
            return true;
        }

        return false;
    }


    /// <summary>
    /// 获取预览链接
    /// </summary>
    /// <param name="id">附件ID</param>
    /// <returns></returns>
    public async Task<Result> GetPreviewUrlAsync(string filePath)
    {
        try
        {
            var user = await db.Queryable<Tb_User>().Where(a => a.Id == identityService.UserId).FirstAsync();

            // 1. 拼接解密代理接口的 URL（业务系统内部接口）
            var fileDomain = configuration.GetSection("kkFileview")["FileDomain"];
            string proxyUrl = $"{fileDomain}{filePath}?fullfilename={EncodedFileName(Path.GetFileName(filePath))}";
            //string proxyUrl = $"{Request.Scheme}://10.10.140.23:17009/api/file/DownloadPreviewFile/{id}?fullfilename={ExcelUtil.EncodedFileName(accessory.FileName)}";

            // 对url进行base64编码，防止url过长及特殊字符问题
            Console.WriteLine("下载链接：" + proxyUrl);
            var base64 = Base64Util.Base64Encode(proxyUrl);
            // 构建水印文本（确保正确编码）
            string watermark = Uri.EscapeDataString($"{user?.Name} {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
            var previewUrl = $"{configuration.GetSection("kkFileview")["Domain"]}" +
                             $"{base64}&watermarkTxt={watermark}";

            return Result.Success(previewUrl, "获取预览链接成功");
        }
        catch (Exception ex)
        {
            return Result.Fail($"获取预览链接失败: {ex.Message}");
        }
    }

    /// <summary>
    /// 对文件名进行编码（中文字符不能直接写入Header）
    /// </summary>
    /// <param name="fileName"></param>
    /// <returns></returns>
    public static string EncodedFileName(string fileName)
    {
        fileName = Regex.Replace(fileName, @"[^\u4e00-\u9fa5a-zA-Z0-9_.]", "_");
        string cleanFileName = new string(fileName.Where(c => !char.IsControl(c)).ToArray());
        string encodedFileName = Uri.EscapeDataString(cleanFileName);
        return encodedFileName;
    }
}