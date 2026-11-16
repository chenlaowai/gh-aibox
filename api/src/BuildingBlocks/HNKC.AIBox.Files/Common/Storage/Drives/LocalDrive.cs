using Microsoft.Extensions.Configuration;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;

namespace HNKC.AIBox.Files.Common.Storage.Drives;

class LocalDrive : IDisk
{
    private readonly string Root;

    public LocalDrive(IConfigurationSection configuration)
    {
        Root = configuration.GetSection("Root").Value;
    }

    public List<string> AllDirectories(string directory)
    {
        var path = PathUtils.Combine(Root, directory);
        IEnumerable<string> dirsEnumerable = Directory.EnumerateDirectories(path, "*", SearchOption.AllDirectories);
        List<string> dirsList = new List<string>();
        foreach (string dir in dirsEnumerable)
        {
            var relativePath = Path.GetRelativePath(path, dir);
            dirsList.Add(relativePath);
        }

        return dirsList;
    }

    public List<string> AllFiles(string directory)
    {
        var path = PathUtils.Combine(Root, directory);
        IEnumerable<string> dirsEnumerable = Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories);
        List<string> dirsList = new List<string>();
        foreach (string dir in dirsEnumerable)
        {
            var relativePath = Path.GetRelativePath(path, dir);
            dirsList.Add(relativePath);
        }

        return dirsList;
    }

    public void Copy(string path, string newPath)
    {
        path = PathUtils.Combine(Root, path);
        newPath = PathUtils.Combine(Root, newPath);
        var dirName = Path.GetDirectoryName(newPath);
        if (!Directory.Exists(dirName))
        {
            Directory.CreateDirectory(dirName);
        }

        File.Copy(path, newPath);
    }

    public bool Delete(string path)
    {
        if (!Exists(path))
        {
            throw new Exception("文件不存在");
        }

        var filePath = PathUtils.Combine(Root, path);
        try
        {
            File.Delete(filePath);
            return true;
        }
        catch (FileNotFoundException)
        {
            return false;
        }
    }

    public bool Delete(List<string> paths)
    {
        var result = true;
        foreach (var path in paths)
        {
            if (!Delete(path))
            {
                result = false;
            }
        }

        return result;
    }

    public bool DeleteDirectory(string directory)
    {
        var directoryPath = PathUtils.Combine(Root, directory);
        try
        {
            Directory.Delete(directoryPath, true);
            return true;
        }
        catch (DirectoryNotFoundException)
        {
            return false;
        }
    }

    public List<string> Directories(string directory)
    {
        var path = PathUtils.Combine(Root, directory);
        IEnumerable<string> dirsEnumerable = Directory.EnumerateDirectories(path);
        List<string> dirsList = new List<string>();
        foreach (string dir in dirsEnumerable)
        {
            var relativePath = Path.GetRelativePath(path, dir);
            dirsList.Add(relativePath);
        }

        return dirsList;
    }

    public bool Exists(string path)
    {
        var filePath = PathUtils.Combine(Root, path);

        return Directory.Exists(filePath) || File.Exists(filePath);
    }

    public List<string> Files(string directory)
    {
        List<string> dirsList = new List<string>();
        var path = PathUtils.Combine(Root, directory);
        if (Directory.Exists(path))
        {
            IEnumerable<string> dirsEnumerable = Directory.EnumerateFiles(path);

            foreach (string dir in dirsEnumerable)
            {
                var relativePath = Path.GetRelativePath(path, dir);
                dirsList.Add(relativePath);
            }
        }

        return dirsList;
    }

    public FileDescription FileDescription(string path)
    {
        var filePath = PathUtils.Combine(Root, path);
        var fileInfo = new FileInfo(filePath);
        return new FileDescription
        {
            Size = fileInfo.Length
        };
    }

    public string Get(string path)
    {
        if (!Exists(path))
        {
            throw new Exception("文件不存在");
        }

        var filePath = PathUtils.Combine(Root, path);
        return File.ReadAllText(filePath);
    }

    public Stream GetStream(string path)
    {
        if (!Exists(path))
        {
            throw new Exception("文件不存在");
        }

        var filePath = PathUtils.Combine(Root, path);
        return File.OpenRead(filePath);
    }

    public byte[] GetBytes(string path)
    {
        if (!Exists(path))
        {
            throw new Exception("文件不存在");
        }

        var filePath = PathUtils.Combine(Root, path);
        using (var stream = File.OpenRead(filePath))
        {
            byte[] bytes = new byte[stream.Length];
            stream.Read(bytes, 0, bytes.Length);
            return bytes;
        }
    }

    public void MakeDirectory(string directory)
    {
        var directoryPath = PathUtils.Combine(Root, directory);
        Directory.CreateDirectory(directoryPath);
    }

    public void Put(string path, string content)
    {
        var filePath = PathUtils.Combine(Root, path);
        var dirName = Path.GetDirectoryName(filePath);
        if (!Directory.Exists(dirName))
        {
            Directory.CreateDirectory(dirName);
        }

        File.WriteAllText(filePath, content);
    }

    public void Put(string path, Stream content)
    {
        var filePath = PathUtils.Combine(Root, path);
        var dirName = Path.GetDirectoryName(filePath);
        if (!Directory.Exists(dirName))
        {
            Directory.CreateDirectory(dirName);
        }

        using (Stream fileStream = File.Create(filePath))
        {
            content.CopyTo(fileStream);
        }
    }

    public void Put(string path, byte[] content)
    {
        var filePath = PathUtils.Combine(Root, path);
        var dirName = Path.GetDirectoryName(filePath);
        if (!Directory.Exists(dirName))
        {
            Directory.CreateDirectory(dirName);
        }

        using (var fs = new FileStream(filePath, FileMode.Create))
        {
            fs.Write(content, 0, content.Length);
        }
    }

    public void Rename(string path, string newPath)
    {
        var oldAbsolutePath = PathUtils.Combine(Root, path);
        var newAbsolutePath = PathUtils.Combine(Root, newPath);

        FileAttributes attr = File.GetAttributes(oldAbsolutePath);
        var dirName = Path.GetDirectoryName(newAbsolutePath);
        if (!Directory.Exists(dirName))
        {
            Directory.CreateDirectory(dirName);
        }

        //detect whether its a directory or file
        if ((attr & FileAttributes.Directory) == FileAttributes.Directory)
            Directory.Move(oldAbsolutePath, newAbsolutePath);
        else
            File.Move(oldAbsolutePath, newAbsolutePath);
    }

    public string ServerPath(string path)
    {
        return PathUtils.Combine(Root, path);
    }

    public Stream OpenWriteStream(string path)
    {
        var filePath = PathUtils.Combine(Root, path);
        return File.Open(filePath, FileMode.Create);
    }

    public bool ClientConnect()
    {
        return true;
    }

    public void Compress(List<Stream> streams, string zipPath, string password = "")
    {
        if (Exists(zipPath))
        {
            throw new Exception("压缩文件已存在");
        }

        zipPath = PathUtils.Combine(Root, zipPath);
        DirectoryInfo root = new DirectoryInfo(zipPath);
        var name = root.Name; //文件名
        var zipDirectory = zipPath.Replace(name, ""); //文件目录
        //创建文件夹
        if (!Directory.Exists(zipDirectory))
        {
            Directory.CreateDirectory(zipDirectory);
        }

        using (FileStream zipFileStream = new FileStream(zipPath, FileMode.Create))
        {
            using (ZipArchive archive = new ZipArchive(zipFileStream, ZipArchiveMode.Create))
            {
                foreach (var item in streams)
                {
                    // 获取文件名
                    var filename = new DirectoryInfo(((FileStream)item).Name).Name;

                    // 在ZIP中创建新条目
                    ZipArchiveEntry entry = archive.CreateEntry(filename);

                    // 使用using确保流正确关闭
                    using (Stream entryStream = entry.Open())
                    {
                        // 将原始流的数据复制到ZIP条目中
                        item.CopyTo(entryStream);

                        // 重要：重置流的位置（如果需要重复使用流）
                        item.Position = 0;
                    }
                }
            }
        }
    }


    public void Compress(List<string> pathNames, string zipPath, string password = "")
    {
        foreach (var item in pathNames)
        {
            if (!Exists(item))
            {
                throw new Exception($"{item} 文件不存在");
            }
        }

        if (Exists(zipPath))
        {
            throw new Exception("压缩文件已存在");
        }

        zipPath = PathUtils.Combine(Root, zipPath);
        var root = new DirectoryInfo(zipPath);
        var name = root.Name; //文件名
        var zipDirectory = zipPath.Replace(name, ""); //文件目录
        //创建文件夹
        if (!Directory.Exists(zipDirectory))
        {
            Directory.CreateDirectory(zipDirectory);
        }

        using (Ionic.Zip.ZipFile zip = new Ionic.Zip.ZipFile(Encoding.UTF8))
        {
            if (!string.IsNullOrEmpty(password))
            {
                zip.Password = password;
            }

            foreach (var item in pathNames)
            {
                //源文件路径
                var pathName = PathUtils.Combine(Root, item);
                //文件名
                var filename = new DirectoryInfo(item).Name;
                zip.AddEntry(filename, File.OpenRead(pathName));
            }

            zip.Save(zipPath);
        }
    }


 

    public string GetMD5Key(string path)
    {
        path = PathUtils.Combine(Root, path);
        var hash = MD5.Create();
        using (var stream = new FileStream(path, FileMode.Open))
        {
            byte[] hashByte = hash.ComputeHash(stream);
            return BitConverter.ToString(hashByte).Replace("-", "");
        }
    }

    public string GetMD5Key(Stream stream)
    {
        var hash = MD5.Create();
        using (stream)
        {
            byte[] hashByte = hash.ComputeHash(stream);
            return BitConverter.ToString(hashByte).Replace("-", "");
        }
    }

    public string GetSHA256Key(string path)
    {
        path = PathUtils.Combine(Root, path);
        var hash = SHA256.Create();
        using (var stream = new FileStream(path, FileMode.Open))
        {
            byte[] hashByte = hash.ComputeHash(stream);
            return BitConverter.ToString(hashByte).Replace("-", "");
        }
    }

    public string GetSHA256Key(Stream stream)
    {
        var hash = SHA256.Create();
        using (stream)
        {
            byte[] hashByte = hash.ComputeHash(stream);
            return BitConverter.ToString(hashByte).Replace("-", "");
        }
    }

    public string GetSHA1Key(string path)
    {
        var hash = SHA1.Create();
        using (var stream = new FileStream(path, FileMode.Open))
        {
            byte[] hashByte = hash.ComputeHash(stream);
            return BitConverter.ToString(hashByte).Replace("-", "");
        }
    }

    public string GetSHA1Key(Stream stream)
    {
        var hash = SHA1.Create();
        using (stream)
        {
            byte[] hashByte = hash.ComputeHash(stream);
            return BitConverter.ToString(hashByte).Replace("-", "");
        }
    }

    public void Write(string data, string path)
    {
        path = PathUtils.Combine(Root, path);
        //判断是否已经有了这个文件
        if (!File.Exists(path))
        {
            //没有则创建这个文件
            using (var fs = new FileStream(path, FileMode.Create, FileAccess.Write))
            {
                using (var sw = new StreamWriter(fs))
                {
                    sw.WriteLine(data); //开始写入值
                }
            }
        }
        else
        {
            using (var fs = new FileStream(path, FileMode.Open, FileAccess.Write))
            {
                using (var sw = new StreamWriter(fs))
                {
                    sw.WriteLine(data); //开始写入值
                }
            }
        }
    }

    public void WriteLog(string data, string? path = null)
    {
        if (string.IsNullOrEmpty(path))
        {
            path = $"/log/{DateTime.Now:yyyyMMdd}.log";
        }

        path = PathUtils.Combine(Root, path);

        DirectoryInfo root = new(path);
        string? name = root.Name; //文件名
        string? directory = path.Replace(name, ""); //文件目录
        //创建文件夹
        if (!Directory.Exists(directory))
        {
            Directory.CreateDirectory(directory);
        }

        // 日志加时间戳
        data = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss}\r\n{data}\r\n";

        //判断是否已经有了这个文件
        if (!File.Exists(path))
        {
            //没有则创建这个文件
            using (var fs = new FileStream(path, FileMode.Create, FileAccess.Write))
            {
                using (var sw = new StreamWriter(fs))
                {
                    sw.WriteLine(data); //开始写入值
                }
            }
        }
        else
        {
            using (var fs = new FileStream(path, FileMode.Append, FileAccess.Write))
            {
                using (var sw = new StreamWriter(fs))
                {
                    sw.WriteLine(data); //开始写入值
                }
            }
        }
    }

    public void Append(string path, Stream content)
    {
        var filePath = PathUtils.Combine(Root, path);
        var dirName = Path.GetDirectoryName(filePath);
        if (!Directory.Exists(dirName))
        {
            Directory.CreateDirectory(dirName);
        }

        // 追加片段到现有文件
        using (FileStream stream = new FileStream(filePath, FileMode.Append))
        {
            content.CopyTo(stream);
        }
    }

    public async Task AppendAsync(string path, Stream content)
    {
        var filePath = PathUtils.Combine(Root, path);
        var dirName = Path.GetDirectoryName(filePath);
        if (!Directory.Exists(dirName))
        {
            Directory.CreateDirectory(dirName);
        }

        // 追加片段到现有文件
        using (FileStream stream = new FileStream(filePath, FileMode.Append))
        {
            await content.CopyToAsync(stream);
        }
    }

    public long GetDirectorySize(string path)
    {
        var filePath = PathUtils.Combine(Root, path);
        DirectoryInfo directoryInfo = new DirectoryInfo(filePath);
        return directoryInfo.EnumerateFiles("*", SearchOption.AllDirectories).Sum(f => f.Length);
    }
}