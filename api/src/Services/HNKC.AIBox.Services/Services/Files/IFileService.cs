using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Services;

public interface IFileService
{
    /// <summary>
    /// 上传文件
    /// </summary>
    /// <param name="file"></param>
    /// <param name="dir"></param>
    /// <param name="fileFilter"></param>
    /// <param name="size"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    string Upload(IFormFile file, string dir = "files", string fileFilter = "", long size = 4 * 1024 * 1024);

    /// <summary>
    /// 获取文件
    /// </summary>
    /// <param name="path"></param>
    /// <returns></returns>
    Stream Download(string path);

    /// <summary>
    /// 删除文件
    /// </summary>
    /// <param name="path"></param>
    /// <returns></returns>
    bool Delete(string path);

    /// <summary>
    /// 获取预览链接
    /// </summary>
    /// <param name="filePath">附件ID</param>
    /// <returns></returns>
    Task<Result> GetPreviewUrlAsync(string filePath);
}