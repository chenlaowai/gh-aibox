using HNKC.AIBox.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Services
{ 
    public interface ISystemService
    {

        /// <summary>
        /// 获取系统字典
        /// </summary>
        /// <returns></returns>

        Task<Dictionary<string, DictResultDto>> GetSystemDict();

    }
}
