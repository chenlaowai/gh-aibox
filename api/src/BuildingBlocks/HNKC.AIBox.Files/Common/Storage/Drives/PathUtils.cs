namespace HNKC.AIBox.Files.Common.Storage.Drives;

    public class PathUtils
    {
        public static string Combine(List<string> paths)
        {
            if (paths.Count < 2)
            {
                throw new Exception("paths count must more than 1");
            }

            var rootPath = paths[0];

            for (var i = 1; i < paths.Count; i++)
            {
                var tmpPath = paths[i];
                if (tmpPath.Substring(0, 1) == "/")
                {
                    tmpPath = tmpPath.Substring(1, tmpPath.Length - 1);
                }
                rootPath = Path.Combine(rootPath, tmpPath);
            }

            return rootPath;
        }
        //
        // 摘要:
        //     Combines four strings into a path.
        //
        // 参数:
        //   path1:
        //     The first path to combine.
        //
        //   path2:
        //     The second path to combine.
        //
        //   path3:
        //     The third path to combine.
        //
        //   path4:
        //     The fourth path to combine.
        //
        // 返回结果:
        //     The combined paths.
        //
        // 异常:
        //   T:System.ArgumentException:
        //     path1, path2, path3, or path4 contains one or more of the invalid characters
        //     defined in System.IO.Path.GetInvalidPathChars.
        //
        //   T:System.ArgumentNullException:
        //     path1, path2, path3, or path4 is null.
        public static string Combine(string path1, string path2, string path3, string path4)
        {
            var pathList = new List<string>
            {
                path1,
                path2,
                path3,
                path4
            };
            return Combine(pathList);
        }
        //
        // 摘要:
        //     Combines an array of strings into a path.
        //
        // 参数:
        //   paths:
        //     An array of parts of the path.
        //
        // 返回结果:
        //     The combined paths.
        //
        // 异常:
        //   T:System.ArgumentException:
        //     One of the strings in the array contains one or more of the invalid characters
        //     defined in System.IO.Path.GetInvalidPathChars.
        //
        //   T:System.ArgumentNullException:
        //     One of the strings in the array is null.
        public static string Combine(params string[] paths)
        {
            var pathList = new List<string>();
            pathList.AddRange(paths);
            return Combine(pathList);
        }
        //
        // 摘要:
        //     Combines two strings into a path.
        //
        // 参数:
        //   path1:
        //     The first path to combine.
        //
        //   path2:
        //     The second path to combine.
        //
        // 返回结果:
        //     The combined paths. If one of the specified paths is a zero-length string, this
        //     method returns the other path. If path2 contains an absolute path, this method
        //     returns path2.
        //
        // 异常:
        //   T:System.ArgumentException:
        //     path1 or path2 contains one or more of the invalid characters defined in System.IO.Path.GetInvalidPathChars.
        //
        //   T:System.ArgumentNullException:
        //     path1 or path2 is null.
        public static string Combine(string path1, string path2)
        {
            var pathList = new List<string>
            {
                path1,
                path2
            };
            return Combine(pathList);
        }
        //
        // 摘要:
        //     Combines three strings into a path.
        //
        // 参数:
        //   path1:
        //     The first path to combine.
        //
        //   path2:
        //     The second path to combine.
        //
        //   path3:
        //     The third path to combine.
        //
        // 返回结果:
        //     The combined paths.
        //
        // 异常:
        //   T:System.ArgumentException:
        //     path1, path2, or path3 contains one or more of the invalid characters defined
        //     in System.IO.Path.GetInvalidPathChars.
        //
        //   T:System.ArgumentNullException:
        //     path1, path2, or path3 is null.
        public static string Combine(string path1, string path2, string path3)
        {
            var pathList = new List<string>
            {
                path1,
                path2,
                path3
            };
            return Combine(pathList);
        }
    }