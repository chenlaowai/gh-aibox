using HNKC.AIBox.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Services
{
    public interface IAlarmService
    {
        /// <summary>
        /// 添加AI预警事件
        /// </summary>
        /// <returns></returns>

        Task<Result> AddAIAlarmEventAsync(AddAIAlarmRequest model);
    }
}
