using HNKC.AIBox.Entities;
using HNKC.AIBox.Enums;
using HNKC.AIBox.Models;
using Microsoft.AspNetCore.Http.HttpResults;
using SqlSugar;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Services
{
    public class AlarmService(ISqlSugarClient sqlSugarClient,IFileService fileService) : IAlarmService
    {


        /// <summary>
        /// 添加AI预警事件
        /// </summary>
        /// <returns></returns>

        public async Task<Result> AddAIAlarmEventAsync(AddAIAlarmRequest model)
        {

            var taskInfo = await sqlSugarClient.Queryable<Tb_Task>().Where(p => p.Id == model.task_id).FirstAsync();
            if (taskInfo == null)
            {
                return Result.Fail("任务不存在");
            }
            var modelInfo = await sqlSugarClient.Queryable<Tb_Algo_Mode>().Where(p => p.Code == model.model_name).FirstAsync();
            if (modelInfo == null)
            {
                return Result.Fail("模型不存在");
            }
            var typeId = string.Empty;
            //告警类别
            var typeInfo = await sqlSugarClient.Queryable<Tb_Alarm_Type>().Where(p => p.Code == model.report_content).FirstAsync();
            if (typeInfo == null)
            {


                var typeEntity = new Tb_Alarm_Type
                {
                    Id = Guid.NewGuid().ToString(),
                    Code = model.report_content,
                    Remark = model.report_content,
                    AlgoId = modelInfo.Id,
                    Created = DateTime.Now,
                    Modified = DateTime.Now
                };
                await sqlSugarClient.Insertable(typeEntity).ExecuteCommandAsync();
                typeId = typeEntity.Id;
            }
            else
            {
                typeId = typeInfo.Id;
            }
            var alarmInfo = new Tb_Alarm
            {
                TaskId = taskInfo.Id,
                ChannelId = taskInfo.ChannelId,
                TypeId = typeId,
                AlarmTime = model.time,
                ImgPath = model.image,
                VideoPath = model.video,
                ReportNums =model.report_num,
                ReportStatus = ReportStatusEnum.Pending,
                ReportAddress = taskInfo.ReportAddress,
                Modified = DateTime.Now
            };
            await sqlSugarClient.Insertable(alarmInfo).ExecuteCommandAsync();
            await  DeleteSystemAlarmEventAsync();
            return Result.Success();
        }


        /// <summary>
        /// 删除系统AI预警事件
        /// </summary>
        /// <returns></returns>
        public async Task<Result> DeleteSystemAlarmEventAsync()
        {
            var configNum = await sqlSugarClient.Queryable<Tb_Param_Config>().Where(p => p.Code == SysConfCode.RecordKeepAlarmInfo).FirstAsync();
            if (configNum == null)
            {
                return Result.Success();
            }
            var days = 0;
            try
            {
                days = int.Parse(configNum.ParamValue);
            }
            catch (Exception ex)
            {
               Console.WriteLine(ex.Message);
                return Result.Success();
            }
            if (configNum.ParamValue == null || days <= 0)
            {
                return Result.Success();
            }
            
            // 计算过期时间
            var expireDate = DateTime.Now.AddDays(-days);

            // 查询过期数据
            var alarmInfo = await sqlSugarClient.Queryable<Tb_Alarm>()
                .Where(p => p.AlarmTime < expireDate)
                .ToListAsync();

            if (alarmInfo.Count == 0)
            {
                return Result.Success();
            }

            // 先删除数据库记录
            var idsToDelete = alarmInfo.Select(r => r.Id).ToList();
            await sqlSugarClient.Deleteable<Tb_Alarm>().Where(r => idsToDelete.Contains(r.Id)).ExecuteCommandAsync();
            //再删除物理数据
            foreach (var item in alarmInfo)
            {
                fileService.Delete(item.ImgPath);
                fileService.Delete(item.VideoPath);
            }
            return Result.Success();
        }



    }
}
