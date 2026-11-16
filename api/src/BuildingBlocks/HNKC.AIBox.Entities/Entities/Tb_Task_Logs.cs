using System;
using SqlSugar;
using System.Linq;
using System.Text;

namespace HNKC.AIBox.Entities
{
    /// <summary>
    ///  任务日志表
    /// </summary>
    [SugarTable("tb_task_logs", "任务日志表")]
    public class Tb_Task_Logs : BaseEntity
    {
        /// <summary>
        ///  任务ID
        ///</summary>
        [SugarColumn(ColumnName = "task_id")]
        public string? TaskId { get; set; }

        /// <summary>
        ///  日志错误描述
        ///</summary>
        [SugarColumn(ColumnName = "log_msg")]
        public string? LogMsg { get; set; }

        /// <summary>
        ///  描述备注
        ///</summary>
        [SugarColumn(ColumnName = "remark")]
        public string? Remark { get; set; }



        /// <summary>
        ///  描述备注
        ///</summary>
        [SugarColumn(ColumnName = "status")]
        public int? Status { get; set; }

    }
}