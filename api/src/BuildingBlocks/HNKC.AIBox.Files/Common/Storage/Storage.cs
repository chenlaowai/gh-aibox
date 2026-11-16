using System.Collections.Concurrent;
using HNKC.AIBox.Files.Common.Storage.Drives;
using Microsoft.Extensions.Configuration;

namespace HNKC.AIBox.Files.Common.Storage;

    public class Storage : IStorage, IDisk
    {
        private readonly IConfiguration Configuration;

        private readonly string DefaultDiskName;

        private readonly ConcurrentDictionary<string, IDisk> Disks = new ConcurrentDictionary<string, IDisk>();

        public Storage(IConfiguration configuration)
        {
            Configuration = configuration;


            DefaultDiskName = Configuration.GetSection("Storage").GetSection("Default").Value;

            var diskConfigSections = Configuration.GetSection("Storage:Disks").GetChildren();

            foreach (var diskConfigSection in diskConfigSections)
            {
                var driveType = diskConfigSection.GetSection("Drive").Value;
                IDisk drive;
                switch (driveType)
                {
                    case "Local":
                        drive = new LocalDrive(diskConfigSection);
                        break;
                    default:
                        throw new Exception("Not Found Drive");
                }
                Disks.AddOrUpdate(diskConfigSection.Key, drive, (k, v) => v = drive);
            }
        }


        public IDisk Disk()
        {
            string defaultDiskName = GetDefaultDiskName();
            return Disk(defaultDiskName);
        }

        public IDisk Disk(string diskName)
        {
            IDisk disk;
            var result = Disks.TryGetValue(diskName, out disk);
            if (result)
            {
                return disk;
            }
            else
            {
                throw new Exception("无效的driver");
                //throw new NotImplementedException();
            }
        }

        private string GetDefaultDiskName()
        {
            return DefaultDiskName;
        }

        public FileDescription FileDescription(string path)
        {
            return Disk().FileDescription(path);
        }

        public string Get(string path)
        {
            return Disk().Get(path);
        }

        public bool Exists(string path)
        {
            return Disk().Exists(path);
        }

        public Stream GetStream(string path)
        {
            return Disk().GetStream(path);
        }

        public byte[] GetBytes(string path)
        {
            return Disk().GetBytes(path);
        }

        public void Put(string path, string content)
        {
            Disk().Put(path, content);
        }

        public void Put(string path, byte[] content)
        {
            Disk().Put(path, content);
        }

        public void Put(string path, Stream content)
        {
            Disk().Put(path, content);
        }

        public Stream OpenWriteStream(string path)
        {
            return Disk().OpenWriteStream(path);
        }

        public void Rename(string path, string newPath)
        {
            Disk().Rename(path, newPath);
        }

        public void Copy(string path, string newPath)
        {
            Disk().Copy(path, newPath);
        }

        public bool Delete(string path)
        {
            return Disk().Delete(path);
        }

        public bool Delete(List<string> path)
        {
            return Disk().Delete(path);
        }

        public List<string> Files(string directory)
        {
            return Disk().Files(directory);
        }

        public List<string> AllFiles(string directory)
        {
            return Disk().AllFiles(directory);
        }

        public List<string> Directories(string directory)
        {
            return Disk().Directories(directory);
        }

        public List<string> AllDirectories(string directory)
        {
            return Disk().AllDirectories(directory);
        }

        public void MakeDirectory(string directory)
        {
            Disk().MakeDirectory(directory);
        }

        public bool DeleteDirectory(string directory)
        {
            return Disk().DeleteDirectory(directory);
        }

        public string ServerPath(string path)
        {
            return Disk().ServerPath(path);
        }

        public bool ClientConnect()
        {
            return Disk().ClientConnect();
        }

        public void Compress(List<Stream> streams, string zipPath, string password = "")
        {
            Disk().Compress(streams, zipPath, password);
        }

        public void Compress(List<string> pathNames, string zipPath, string password = "")
        {
            Disk().Compress(pathNames, zipPath, password);
        }

        public List<string> DeCompress(Stream stream, string zipPath, string password = "")
        {
            return Disk().DeCompress(stream, zipPath, password);
        }

        public List<string> DeCompress(string pathNames, string zipPath, string password = "")
        {
            return Disk().DeCompress(pathNames, zipPath, password);
        }

        public string GetMD5Key(string path)
        {
            return Disk().GetMD5Key(path);
        }

        public string GetMD5Key(Stream stream)
        {
            return Disk().GetMD5Key(stream);
        }

        public string GetSHA256Key(string path)
        {
            return Disk().GetSHA256Key(path);
        }

        public string GetSHA256Key(Stream stream)
        {
            return Disk().GetSHA256Key(stream);
        }

        public string GetSHA1Key(string path)
        {
            return Disk().GetSHA1Key(path);
        }

        public string GetSHA1Key(Stream stream)
        {
            return Disk().GetSHA1Key(stream);
        }


        public List<string> DeCompress(byte[] bytes, string zipPath, string password = "")
        {
            return Disk().DeCompress(bytes, zipPath, password);
        }

        public void Compress(Dictionary<string, byte[]> entitys, string zipPath, string password = "")
        {
            Disk().Compress(entitys, zipPath, password);
        }

        public void Write(string data, string path)
        {
            Disk().Write(data, path);
        }

        public void Compress(string path, string zipPath, string password = "")
        {
            Disk().Compress(path, zipPath, password);
        }

        public void Compress(Dictionary<string, string> pathNames, string zipPath, string password = "")
        {
            Disk().Compress(pathNames, zipPath, password);
        }

        public List<byte[]> DeCompress(byte[] bytes, string password = "")
        {
            return Disk().DeCompress(bytes, password);
        }

        public List<byte[]> DeCompress(string pathNames, string password = "")
        {
            return Disk().DeCompress(pathNames, password);
        }

        public List<byte[]> DeCompress(Stream stream, string password = "")
        {
            return Disk().DeCompress(stream, password);
        }

        public void WriteLog(string data, string? path = null)
        {
            Disk().WriteLog(data, path);
        }

        public void Append(string path, Stream content)
        {
            Disk().Append(path, content);
        }

        public async Task AppendAsync(string path, Stream content)
        {
            await Disk().AppendAsync(path, content);
        }

        public long GetDirectorySize(string path)
        {
            return Disk().GetDirectorySize(path);
        }
    }