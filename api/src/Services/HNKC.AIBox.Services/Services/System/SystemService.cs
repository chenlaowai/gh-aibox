using HNKC.AIBox.Models;
using HNKC.AIBox.Utils;
using SqlSugar;
using System.Reflection;
using System.Xml.Linq;

namespace HNKC.AIBox.Services
{
    public class SystemService: ISystemService
    {
        /// <summary>
        /// 获取系统字典
        /// </summary>
        /// <returns></returns>

        public async Task<Dictionary<string, DictResultDto>> GetSystemDict()
        {
            Dictionary<string, DictResultDto> dictResult = new Dictionary<string, DictResultDto>();
            // 动态加载枚举字典
            var assembly = Assembly.Load("HNKC.AIBox.Enums");
            assembly.GetTypes()
                .Where(t => t.IsEnum)
                .ToList()
                .ForEach(enumType =>
                {
                    string enumKey = ConvertEnumNameToKey(enumType.Name); // 将首字母统一转换为小写
                    dictResult[enumKey] = new DictResultDto
                    {
                        Desc = GetEnumSummary(enumType),//enumKey,
                        Data = Enum.GetValues(enumType)
                            .Cast<Enum>()
                            .Select(e => new DictDto
                            {
                                Value = Convert.ToInt32(e),
                                Label = e.FetchDescription() // 确保扩展方法可访问
                            }).ToList()
                    };
                });
            return dictResult;
        }



        #region 枚举反射辅助方法

        // 辅助方法：将枚举名称转换为键值
        private string ConvertEnumNameToKey(string enumName)
        {
            // 去掉 "Enum_" 前缀
            string nameWithoutPrefix = enumName.Replace("Enum_", "", StringComparison.OrdinalIgnoreCase);
            // 将首字母转换为小写，同时保留后续单词的大小写
            if (string.IsNullOrEmpty(nameWithoutPrefix))
                return nameWithoutPrefix;
            return char.ToLowerInvariant(nameWithoutPrefix[0]) + nameWithoutPrefix.Substring(1);
        }

        /// <summary>
        /// 获取枚举类型的注释（<summary> 标签内容）
        /// </summary>
        /// <param name="enumType">枚举类型</param>
        /// <returns>注释内容</returns>
        private string GetEnumSummary(Type enumType)
        {
            // 获取程序集的 XML 文档路径
            string xmlPath = enumType.Assembly.Location.Replace(".dll", ".xml");

            // 加载 XML 文档
            if (!System.IO.File.Exists(xmlPath))
                return null;

            XDocument xmlDoc = XDocument.Load(xmlPath);
            // 获取枚举类型的完整名称
            string fullName = $"T:{enumType.FullName}";

            // 查找匹配的 <member> 节点
            var memberNode = xmlDoc.Descendants("member")
                .FirstOrDefault(m => m.Attribute("name")?.Value == fullName);

            if (memberNode == null)
                return null;

            // 提取 <summary> 标签内容
            var summaryNode = memberNode.Element("summary");
            if (summaryNode == null)
                return null;

            // 返回注释内容（去除空白字符）
            return summaryNode.Value.Trim();
        }

        #endregion

    }
}
