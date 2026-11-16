using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Enums
{
    public static class SysConfCode
    {

        /// <summary>
        /// 预警信息记录保存天数
        /// </summary>
        public  const string RecordKeepAlarmInfo  = "RecordKeepAlarmInfo";

        /// <summary>
        /// 视频路数
        /// </summary>
        public const string VideoRoute = "VideoRoute";

        /// <summary>
        /// 告警产生后，多少秒后开始上传
        /// </summary>
        public const string AlarmReportDelaySeconds = "AlarmReportDelaySeconds";

        /// <summary>
        /// 告警上传失败后，多少秒进行重新上传
        /// </summary>
        public const string AlarmReportRetryIntervalSeconds = "AlarmReportRetryIntervalSeconds";
    }
}
