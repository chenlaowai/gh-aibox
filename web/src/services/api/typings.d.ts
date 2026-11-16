declare namespace API {
  type AddAIAlarmRequest = {
    /** 任务ID */
    task_id?: string;
    /** 模型名称 */
    model_name?: string;
    /** 告警类别名称 */
    report_content?: string;
    /** 告警人员数量 */
    report_num?: number;
    /** 告警事件时间 */
    time?: string;
    /** 告警图片绝对路径 */
    image?: string;
    /** 告警视频绝对路径 */
    video?: string;
  };

  type AddAlarmTypeRequest = {
    /** 告警类别 */
    code: string;
    /** 所属算法ID */
    algoId?: string;
    /** 描述备注 */
    remark?: string;
  };

  type AddAreaRule = {
    /** 区域标识 */
    name: string;
    ruleType: RuleTypeEnum;
    regionType: RegionTypeEnum;
    /** 算法模型ID */
    algoModeId: string[];
    /** 区域坐标 */
    points?: Point[];
  };

  type AddChannelRequest = {
    protocolType: ProtocolTypeEnum;
    /** 视频地址 */
    address: string;
    /** 监控编码 */
    code: string;
    /** 通道名称 */
    name: string;
    /** 监控描述 */
    remark?: string;
    /** 服务地址 */
    serviceAddress?: string;
    /** 账户ID */
    accountId?: number;
    /** 账户密钥 */
    accountKey?: string;
  };

  type AddParamConfigRequest = {
    /** 参数名称 */
    name: string;
    /** 参数值 */
    paramValue: string;
    /** 描述备注 */
    remark?: string;
    /** 参数标识 */
    code?: string;
  };

  type AddTaskAreaRequest = {
    /** 任务ID */
    taskId: string;
    /** 任务区域集合 */
    areaRules?: AddAreaRule[];
  };

  type AddTaskRequest = {
    /** 任务编号 */
    code: string;
    /** 任务描述 */
    remark?: string;
    /** 通道ID(视频源) */
    channelId?: string;
    /** 上报地址 */
    reportAddress?: string;
    /** 推理间隔帧数 */
    frameInterval?: number;
    /** 告警触发帧数 */
    detectThreshold?: number;
    /** 告警触发时间区间 */
    detectSection?: number;
    /** 告警时间视频存储时长 */
    videoDuration?: number;
    /** 告警上报事件冷却时间 */
    cooldownTime?: number;
    /** 任务算法关联 */
    taskAlgoRequest?: TaskAlgoRequest[];
  };

  type AddUserRequest = {
    /** 账号 */
    account: string;
    /** 密码 */
    password?: string;
    /** 姓名 */
    name?: string;
    /** 手机号 */
    mobile?: string;
    /** 备注 */
    remark?: string;
  };

  type AlgoMode = {
    /** 算法模型ID */
    id?: string;
    /** 算法名称 */
    name?: string;
    majorOnly?: boolean;
    zoneRequired?: boolean;
  };

  type AreaRule = {
    /** 区域ID */
    id?: string;
    /** 区域标识 */
    name?: string;
    ruleType?: RuleTypeEnum;
    regionType?: RegionTypeEnum;
    /** 算法模型ID */
    algoModeId?: string[];
    /** 区域坐标 */
    points?: Point[];
  };

  type ChannelStatusEnum = 1 | 2;

  type DeleteAlarmRequest = {
    ids?: string[];
  };

  type deleteAlarmTypeDeleteAlarmTypeIdParams = {
    id: string;
  };

  type deleteAlgoModeDeleteAlgoModeIdParams = {
    id: string;
  };

  type deleteChannelDeleteChannelIdParams = {
    id: string;
  };

  type deleteParamConfigDeleteParamConfigIdParams = {
    id: string;
  };

  type deleteTaskDeleteTaskIdParams = {
    id: string;
  };

  type deleteUserDeleteIdParams = {
    id: string;
  };

  type DeviceInfoResult = {
    /** 设备标识 */
    deviceIdentification?: string;
    /** 授权状态（0：未授权，1：已授权） */
    authorizationStatus?: number;
    /** 软件版本 */
    softwareVersion?: string;
    /** 资源信息 */
    resourceInformation?: number;
    /** 系统版本 */
    systemVersion?: string;
    /** 盒子时间 */
    boxTime?: string;
    /** 芯片温度 */
    chipTemperature?: number;
    /** 显卡温度 */
    graphicsCardTemperature?: string;
    /** 占用率 */
    usages?: UsageResult[];
  };

  type DictDto = {
    /** 字典值 */
    value?: number;
    /** 名称 */
    label?: string;
  };

  type DictResultDto = {
    /** 名称 */
    desc?: string;
    /** 字典值 */
    data?: DictDto[];
  };

  type getAITaskAlgoConfigDetailParams = {
    task_id?: string;
  };

  type GetAlarmDetailResult = {
    /** 主键 */
    id?: string;
    /** 任务名称(从Tb_Task获取) */
    taskCode?: string;
    /** 通道地址(从Tb_Channel获取) */
    channelAddress?: string;
    /** 告警内容(从Tb_Alarm_Type获取) */
    alarmTypeCode?: string;
    /** 告警描述(从Tb_Alarm_Type获取) */
    alarmTypeRemark?: string;
    reportStatus?: ReportStatusEnum;
    /** 上报地址 */
    reportAddress?: string;
    /** 图片路径 */
    imgPath?: string;
    /** 视频路径 */
    videoPath?: string;
    /** 告警人数 */
    reportNums?: number;
  };

  type getAlarmGetAlarmDetailIdParams = {
    id: string;
  };

  type GetAlarmTypeDropdownResult = {
    /** 告警类别ID */
    id?: string;
    /** 告警类别 */
    code?: string;
    /** 告警描述 */
    remark?: string;
  };

  type getAlarmTypeGetAlarmTypePageParams = {
    /** 当前页 */
    PageIndex?: number;
    /** 每页行数 */
    PageSize?: number;
  };

  type GetAlarmTypePageResult = {
    /** 主键ID */
    id?: string;
    /** 告警类别 */
    code?: string;
    /** 所属算法（从tb_algo_mode表中获取） */
    algoModeName?: string;
    /** 告警描述 */
    remark?: string;
  };

  type GetAlarmTypePageResultPage = {
    /** 页码 */
    pageIndex?: number;
    /** 页大小 */
    pageSize?: number;
    totalCount?: number;
    list?: GetAlarmTypePageResult[];
  };

  type getAlgoConfigGetAlgoConfigPageParams = {
    /** 当前页 */
    PageIndex?: number;
    /** 每页行数 */
    PageSize?: number;
  };

  type GetAlgoConfigPageResult = {
    /** 主键ID */
    id?: string;
    /** 算法名称 */
    name?: string;
    /** 阈值，配置值 */
    configValue?: string;
    /** 算法模型名称(从Tb_Algo_Mode获取) */
    algoModeName?: string;
    /** 算法模型ID */
    algoId?: string;
  };

  type GetAlgoConfigPageResultPage = {
    /** 页码 */
    pageIndex?: number;
    /** 页大小 */
    pageSize?: number;
    totalCount?: number;
    list?: GetAlgoConfigPageResult[];
  };

  type GetAlgoModeDropdownResult = {
    /** 算法模型ID */
    id?: string;
    /** 算法名称 */
    name?: string;
    authorizationStatus?: ModeAuthorizationEnum;
  };

  type getAlgoModeGetAlgoModelConfigParams = {
    algoId?: string;
  };

  type getAlgoModeGetAlgoModePageParams = {
    /** 当前页 */
    PageIndex?: number;
    /** 每页行数 */
    PageSize?: number;
  };

  type GetAlgoModePageResult = {
    /** 算法模型ID */
    id?: string;
    /** 算法名称 */
    name?: string;
    /** 模型文件路径 */
    filePath?: string;
    /** 版本日期 */
    versionDate?: string;
    /** 备注 */
    remark?: string;
  };

  type GetAlgoModePageResultPage = {
    /** 页码 */
    pageIndex?: number;
    /** 页大小 */
    pageSize?: number;
    totalCount?: number;
    list?: GetAlgoModePageResult[];
  };

  type GetChannelDetailResult = {
    /** Id */
    id?: string;
    protocolType?: ProtocolTypeEnum;
    /** 视频地址 */
    address?: string;
    /** 监控编码 */
    code?: string;
    /** 通道名称 */
    name?: string;
    /** 通道协议配置 */
    channelConfig?: string;
    /** 监控描述 */
    remark?: string;
    /** 服务地址 */
    serviceAddress?: string;
    /** 账户ID */
    accountId?: number;
    /** 账户密钥 */
    accountKey?: string;
  };

  type GetChannelDropdownResult = {
    /** 通道ID */
    id?: string;
    /** 通道名称 */
    name?: string;
  };

  type getChannelGetChannelDetailIdParams = {
    id: string;
  };

  type getChannelGetChannelsPageParams = {
    /** 当前页 */
    PageIndex?: number;
    /** 每页行数 */
    PageSize?: number;
  };

  type GetChannelPageResult = {
    /** Id */
    id?: string;
    channelStatus?: ChannelStatusEnum;
    /** 视频地址 */
    address?: string;
    /** 监控编码 */
    code?: string;
    /** 通道名称 */
    name?: string;
  };

  type GetChannelPageResultPage = {
    /** 页码 */
    pageIndex?: number;
    /** 页大小 */
    pageSize?: number;
    totalCount?: number;
    list?: GetChannelPageResult[];
  };

  type getParamConfigGetParamConfigPageParams = {
    /** 当前页 */
    PageIndex?: number;
    /** 每页行数 */
    PageSize?: number;
  };

  type GetParamConfigPageResult = {
    /** 主键ID */
    id?: string;
    /** 参数名称 */
    name?: string;
    /** 参数标识 */
    code?: string;
    /** 参数类型 */
    paramType?: string;
    /** 参数值 */
    paramValue?: string;
    /** 描述备注 */
    remark?: string;
  };

  type GetParamConfigPageResultPage = {
    /** 页码 */
    pageIndex?: number;
    /** 页大小 */
    pageSize?: number;
    totalCount?: number;
    list?: GetParamConfigPageResult[];
  };

  type getPreviewGetVideoParams = {
    token?: string;
  };

  type GetTaskAreaResult = {
    /** base 64图片 */
    photo?: string;
    /** 任务ID */
    taskId?: string;
    /** 任务编码 */
    taskCode?: string;
    /** 算法模型 */
    algoModes?: AlgoMode[];
    /** 任务区域集合 */
    areaRules?: AreaRule[];
  };

  type GetTaskDetailResult = {
    /** 主键ID */
    id?: string;
    /** 任务编号 */
    code?: string;
    /** 任务描述 */
    remark?: string;
    /** 通道ID */
    channelId?: string;
    /** 视频编号（从Tb_Channel获取） */
    channelCode?: string;
    /** 上报地址 */
    reportAddress?: string;
    /** 推理间隔帧数 */
    frameInterval?: number;
    /** 告警触发帧数 */
    detectThreshold?: number;
    /** 告警触发时间区间 */
    detectSection?: number;
    /** 告警时间视频存储时长 */
    videoDuration?: number;
    /** 告警上报事件冷却时间 */
    cooldownTime?: number;
    /** 任务算法关联 */
    taskAlgoResult?: TaskAlgoResult[];
  };

  type GetTaskDropdownResult = {
    /** 主键ID */
    id?: string;
    /** 任务编号 */
    code?: string;
    /** 子节点 */
    children?: GetTaskDropdownResult[];
  };

  type getTaskGetTaskAreasTaskIdParams = {
    taskId: string;
  };

  type getTaskGetTaskDetailIdParams = {
    id: string;
  };

  type getTaskGetTaskPageParams = {
    /** 当前页 */
    PageIndex?: number;
    /** 每页行数 */
    PageSize?: number;
  };

  type GetTaskPageResult = {
    /** 主键ID */
    id?: string;
    /** 任务编号 */
    code?: string;
    /** 视频编号（从Tb_Channel获取） */
    channelCode?: string;
    /** 通道名称 */
    channelName?: string;
    /** 任务算法列表 */
    modelNames?: string[];
    status?: TaskStatusEnum;
  };

  type getUserUserPageParams = {
    /** 姓名 */
    Name?: string;
    /** 账号 */
    Account?: string;
    /** 手机号 */
    Mobile?: string;
    /** 当前页 */
    PageIndex?: number;
    /** 每页行数 */
    PageSize?: number;
  };

  type LoginRequest = {
    /** 账号 */
    account: string;
    /** 密码 */
    password: string;
    /** 滑块验证码 */
    sliderCaptcha: number[];
  };

  type ModeAuthorizationEnum = 0 | 1;

  type ModifyUserRequest = {
    /** Id */
    id: string;
    /** 姓名 */
    name: string;
    account: string;
    /** 所属组织；人员所属组织 和人员组织查询内容无关 */
    orgId?: string;
    /** 备注 */
    remark?: string;
    /** 手机号 */
    mobile?: string;
  };

  type patchTaskStopTaskIdStopParams = {
    id: string;
  };

  type patchTaskUpdateTaskStatusIdStatusParams = {
    /** 任务ID */
    id: string;
    /** 新状态 */
    status: TaskStatusEnum;
  };

  type Point = {
    /** 点X坐标 */
    x?: number;
    /** 点Y坐标 */
    y?: number;
  };

  type postTestDeleteFilePathTestParams = {
    path?: string;
  };

  type ProtocolTypeEnum = 0 | 1 | 2 | 3 | 4 | 5;

  type QueryAlarmPageRequest = {
    /** 当前页 */
    pageIndex?: number;
    /** 每页行数 */
    pageSize?: number;
    /** 告警任务 */
    taskId?: string;
    /** 告警通道 */
    channelId?: string;
    /** 告警类别 */
    typeId?: string;
    /** 告警开始时间 */
    alarmStartTime?: string;
    /** 告警结束时间 */
    alarmEndTime?: string;
  };

  type QueryAlarmPageResult = {
    /** 主键 */
    id?: string;
    /** 任务名称(从Tb_Task获取) */
    taskCode?: string;
    /** 通道监控编码(从Tb_Channel获取) */
    channelCode?: string;
    /** 通道名称 */
    channelName?: string;
    /** 告警内容(从Tb_Alarm_Type获取) */
    alarmTypeCode?: string;
    /** 告警描述 */
    alarmTypeRemark?: string;
    /** 图片路径 */
    imgPath?: string;
    /** 告警时间 */
    alarmTime?: string;
    reportStatus?: ReportStatusEnum;
  };

  type QueryAlarmPageResultPage = {
    /** 页码 */
    pageIndex?: number;
    /** 页大小 */
    pageSize?: number;
    totalCount?: number;
    list?: QueryAlarmPageResult[];
  };

  type RefreshTokenRequest = {
    /** 刷新Token */
    refreshToken?: string;
  };

  type RegionTypeEnum = 0 | 1 | 2;

  type ReportStatusEnum = 0 | 1 | 2;

  type ResetPasswordRequest = {
    /** 用户Id */
    userId?: string;
    /** 新密码 */
    newPassword?: string;
  };

  type RuleTypeEnum = 0 | 1 | 2;

  type TaskAlgoConfig = {
    /** 配置ID */
    configId?: string;
    name?: string;
    /** 参数描述 */
    remark?: string;
    /** 参数标识 */
    code?: string;
    /** 阈值，配置值,缺省值 */
    configValue?: string;
    /** 值类型 */
    valueType?: string;
    min?: number;
    max?: number;
    step?: number;
    options?: { label: string; value: string }[];
  };

  type TaskAlgoConfigDetail = {
    /** 任务ID */
    taskId?: string;
    /** 任务名称（Code） */
    taskName?: string;
    /** 通道地址 */
    channelAddress?: string;
    /** 推理间隔帧数 */
    frameInterval?: number;
    /** 告警触发帧数 */
    detectThreshold?: number;
    /** 告警触发时间区间 */
    detectSection?: number;
    /** 告警时间视频存储时长 */
    videoDuration?: number;
    /** 告警上报事件冷却时间 */
    cooldownTime?: number;
    /** 模型列表 */
    model?: TaskAlgoModel[];
  };

  type TaskAlgoModel = {
    /** 模型ID */
    alogId?: string;
    /** 算法名称 */
    algoName?: string;
    /** 算法模型代码 */
    algoCode?: string;
    /** 模型算法配置列表 */
    algoConfig?: TaskAlgoConfig[];
    /** 任务区域列表 */
    taskArea?: TaskArea[];
  };

  type TaskAlgoRequest = {
    /** 算法ID */
    algoId?: string;
    /** 算法参数 */
    valueConfig?: TaskAlgoConfig[];
  };

  type TaskAlgoResult = {
    /** 算法ID */
    algoId?: string;
    /** 算法名称 */
    algoName?: string;
    /** 值配置 */
    valueConfig?: TaskAlgoConfig[];
  };

  type TaskArea = {
    /** 图形类型（0=辅助线，1=矩形，2=多边形） */
    regionType?: number;
    /** 任务区区域点列表 */
    points?: string;
    /** 算法模型ID */
    algoModeId?: string;
  };

  type TaskStatusEnum = 1 | 2 | 3;

  type TokenInfoResult = {
    /** 登录Token */
    accessToken?: string;
    /** 刷新Token */
    refreshToken?: string;
    /** 过期时间 */
    expires?: number;
  };

  type UpdateAlgoConfigRequest = {
    /** 主键ID */
    id?: string;
    /** 阈值，配置值 */
    configValue?: string;
  };

  type UpdateChannelRequest = {
    /** Id */
    id: string;
    protocolType: ProtocolTypeEnum;
    /** 视频地址 */
    address: string;
    /** 监控编码 */
    code: string;
    /** 通道名称 */
    name: string;
    /** 监控描述 */
    remark?: string;
    /** 服务地址 */
    serviceAddress?: string;
    /** 账户ID */
    accountId?: number;
    /** 账户密钥 */
    accountKey?: string;
  };

  type UpdateParamConfigRequest = {
    /** Id */
    id?: string;
    /** 参数值 */
    paramValue: string;
  };

  type UpdateTaskRequest = {
    /** 主键ID */
    id: string;
    /** 任务编号 */
    code: string;
    /** 任务描述 */
    remark?: string;
    /** 通道ID(视频源) */
    channelId?: string;
    /** 上报地址 */
    reportAddress?: string;
    /** 推理间隔帧数 */
    frameInterval?: number;
    /** 告警触发帧数 */
    detectThreshold?: number;
    /** 告警触发时间区间 */
    detectSection?: number;
    /** 告警时间视频存储时长 */
    videoDuration?: number;
    /** 告警上报事件冷却时间 */
    cooldownTime?: number;
    /** 任务算法关联 */
    taskAlgoRequest?: TaskAlgoRequest[];
  };

  type UsageResult = {
    /** 名称 */
    name?: string;
    /** 占用率 */
    percent?: number;
  };

  type UserBaseInfo = {
    /** 主键ID */
    id?: string;
    /** 账号 */
    account?: string;
    /** 姓名 */
    name?: string;
    /** 手机号 */
    mobile?: string;
    /** 备注 */
    remark?: string;
  };

  type UserBaseInfoPage = {
    /** 页码 */
    pageIndex?: number;
    /** 页大小 */
    pageSize?: number;
    totalCount?: number;
    list?: UserBaseInfo[];
  };

  type UserInfo = {
    /** 主键ID */
    id?: string;
    /** 账号 */
    account?: string;
    /** 姓名 */
    name?: string;
    /** 手机号 */
    mobile?: string;
    /** 备注 */
    remark?: string;
  };
}
