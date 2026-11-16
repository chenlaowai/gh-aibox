namespace HNKC.AIBox.Files.Common.Storage;

    public interface IDisk
    {
        FileDescription FileDescription(string path);

        #region 检索文件
        /// <summary>
        /// Get 方法可以用于检索文件的内容，此方法返回该文件的原始字符串内容
        /// 切记，所有文件路径的指定都应该相对于为磁盘配置的 root 目录
        /// </summary>
        /// <param name="path">文件路径</param>
        /// <returns></returns>
        string Get(string path);

        /// <summary>
        /// Exists 方法可以用来判断磁盘上是否存在指定的文件或目录：
        /// </summary>
        /// <param name="path">文件/目录 路径</param>
        /// <returns></returns>
        bool Exists(string path);

        /// <summary>
        /// GetStream 方法获取文件的读取流
        /// 注意 使用此方法获取文件是要注意使用后关闭流
        /// </summary>
        /// <param name="path">文件路径</param>
        Stream GetStream(string path);

        /// <summary>
        /// GetBytes 方法获取文件的byte[]
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        byte[] GetBytes(string path);

        /// <summary>
        /// 获取文件夹大小
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        long GetDirectorySize(string path);
        #endregion

        #region 保存文件
        /// <summary>
        /// Put 方法可用于将原始文件内容保存到磁盘上。
        /// 你也可以传递 StreamReader 给 put 方法，它将使用文件系统下的底层流支持。
        /// 强烈建议在处理大文件时使用流：
        /// </summary>
        /// <param name="path"></param>
        /// <param name="content"></param>
        void Put(string path, string content);
        void Put(string path, Stream content);
        void Put(string path, byte[] content);
        /// <summary>
        /// Append 方法可用于追加片段到文件
        /// </summary>
        /// <param name="path"></param>
        /// <param name="content"></param>
        void Append(string path, Stream content);
        Task AppendAsync(string path, Stream content);
        Stream OpenWriteStream(string path);
        /// <summary>
        /// 写入文件数据
        /// </summary>
        /// <param name="data"></param>
        /// <param name="path"></param>
        void Write(string data, string path);
        #endregion

        #region 复制 & 移动
        /// <summary>
        /// Rename 方法可用于重命名或将现有文件移动到新位置：
        /// </summary>
        /// <param name="path"></param>
        /// <param name="newPath"></param>
        void Rename(string path, string newPath);
        /// <summary>
        /// Copy 方法可用于将现有文件复制到磁盘的新位置
        /// </summary>
        /// <param name="path"></param>
        /// <param name="newPath"></param>
        void Copy(string path, string newPath);
        #endregion

        #region 删除文件
        bool Delete(string path);
        bool Delete(List<string> path);
        #endregion

        #region 解压缩

        /// <summary>
        /// 把流压缩到指定目录
        /// </summary>
        /// <param name="streams"></param>
        /// <param name="zipPath"></param>
        /// <param name="password"></param>
        void Compress(List<Stream> streams, string zipPath, string password = "");

        /// <summary>
        /// 把字节流压缩到指定目录
        /// </summary>
        /// <param name="entitys"></param>
        /// <param name="zipPath"></param>
        /// <param name="password"></param>
        void Compress(Dictionary<string, byte[]> entitys, string zipPath, string password = "");

        /// <summary>
        /// 根据文件路径压缩到制定目录
        /// </summary>
        /// <param name="pathNames"></param>
        /// <param name="zipPath"></param>
        /// <param name="password"></param>
        void Compress(List<string> pathNames, string zipPath, string password = "");

        /// <summary>
        /// 根据文件路径压缩到制定目录(传压缩文件名称)
        /// </summary>
        /// <param name="pathNames"></param>
        /// <param name="zipPath"></param>
        /// <param name="password"></param>
        void Compress(Dictionary<string, string> pathNames, string zipPath, string password = "");

        /// <summary>
        /// 把某个文件夹压缩到指定路径
        /// </summary>
        /// <param name="path"></param>
        /// <param name="zipPath"></param>
        /// <param name="password"></param>
        void Compress(string path, string zipPath, string password = "");

        /// <summary>
        /// 把流解压到指定目录
        /// </summary>
        /// <param name="stream"></param>
        /// <param name="zipPath"></param>
        /// <param name="password"></param>
        /// <returns></returns>
        List<string> DeCompress(Stream stream, string zipPath, string password = "");

        /// <summary>
        /// 把字节流解压到指定目录
        /// </summary>
        /// <param name="bytes"></param>
        /// <param name="zipPath"></param>
        /// <param name="password"></param>
        /// <returns></returns>
        List<string> DeCompress(byte[] bytes, string zipPath, string password = "");

        /// <summary>
        /// 把指定目录下的压缩包解压到指定目录
        /// </summary>
        /// <param name="pathNames"></param>
        /// <param name="zipPath"></param>
        /// <param name="password"></param>
        /// <returns></returns>
        List<string> DeCompress(string pathNames, string zipPath, string password = "");


        /// <summary>
        /// 获取压缩包内容文件
        /// </summary>
        /// <param name="bytes"></param>
        /// <param name="password"></param>
        /// <returns></returns>
        List<byte[]> DeCompress(byte[] bytes, string password = "");

        /// <summary>
        /// 获取压缩包内容文件
        /// </summary>
        /// <param name="pathNames"></param>
        /// <param name="password"></param>
        /// <returns></returns>
        List<byte[]> DeCompress(string pathNames, string password = "");

        /// <summary>
        /// 获取压缩包内容文件
        /// </summary>
        /// <param name="bytes"></param>
        /// <param name="password"></param>
        /// <returns></returns>
        List<byte[]> DeCompress(Stream stream, string password = "");

        #endregion

        #region 获取文件哈希值

        /// <summary>
        /// 获取文件哈希值 MD5加密
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        string GetMD5Key(string path);

        /// <summary>
        /// 获取文件哈希值 MD5加密
        /// </summary>
        /// <param name="stream"></param>
        /// <returns></returns>
        string GetMD5Key(Stream stream);

        /// <summary>
        /// 获取文件哈希值 SHA256加密
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        string GetSHA256Key(string path);

        /// <summary>
        /// 获取文件哈希值 SHA256加密
        /// </summary>
        /// <param name="stream"></param>
        /// <returns></returns>
        string GetSHA256Key(Stream stream);

        /// <summary>
        /// 获取文件哈希值 SHA1加密
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        string GetSHA1Key(string path);

        /// <summary>
        /// 获取文件哈希值 SHA1加密
        /// </summary>
        /// <param name="stream"></param>
        /// <returns></returns>
        string GetSHA1Key(Stream stream);

        #endregion

        #region 目录
        List<string> Files(string directory);
        List<string> AllFiles(string directory);
        List<string> Directories(string directory);
        List<string> AllDirectories(string directory);
        void MakeDirectory(string directory);
        bool DeleteDirectory(string directory);
        #endregion
        string ServerPath(string directory);

        bool ClientConnect();

        /// <summary>
        /// 临时记录日志
        /// </summary>
        /// <param name="data"></param>
        /// <param name="path"></param>
        void WriteLog(string data, string? path = null);
    }