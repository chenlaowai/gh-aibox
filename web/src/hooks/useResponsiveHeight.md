# 响应式高度适配方案

## 问题描述

在使用 `autofit.js` 进行页面缩放适配时，当屏幕分辨率超过设计稿尺寸（1920x1080）时，`#app` 元素会被 `transform: scale()` 进行缩放。这导致使用固定高度计算（如 `h-[calc(100vh-124px)]`）的元素出现高度计算错误，滚动区域无法正常工作。

## 解决方案

### 1. useResponsiveHeight Hook

创建了一个响应式高度计算的 composable hook，能够：

- 检测 `#app` 元素的缩放比例
- 根据缩放比例动态调整高度计算
- 在非缩放环境下回退到传统的 `calc()` 方式
- 提供防抖处理避免频繁计算

### 2. 使用方法

```typescript
import { useResponsiveHeight } from '@/hooks/useResponsiveHeight'

// 基础用法 - 默认减去124px的固定高度
const { height: containerHeight } = useResponsiveHeight(124)

// 在模板中使用
<div :style="{ height: containerHeight }">
  <!-- 内容 -->
</div>
```

### 3. 工作原理（优化版本）

#### 智能检测算法

新的算法采用三种情况的智能检测：

1. **Transform缩放检测**：
   - 检查 `#app` 元素的 `transform` 属性
   - 解析 `matrix()` 值获取实际缩放比例
   - 适用于标准的 autofit.js 缩放场景

2. **元素尺寸适配**：
   - 当没有 transform 时，检查 App 元素的实际高度
   - 如果元素高度与视口高度差异较大，使用元素实际高度
   - 适用于 autofit.js 采用其他适配策略的情况

3. **标准布局回退**：
   - 当以上两种情况都不适用时，使用传统的 `calc(100vh - fixedHeight)`
   - 确保向下兼容

#### 关键改进

- **实际缩放优先**：优先使用浏览器计算的实际 transform 值
- **多情况适配**：支持不同的 autofit.js 适配策略
- **精确检测**：通过 `getBoundingClientRect()` 获取准确的元素状态

### 4. 已修复的页面

- `src/views/bi/current-preview/Index.vue` - 实时预览页面
- `src/views/bi/task-management/Config.vue` - 任务配置页面
- `src/views/bi/task-management/AddOrEdit.vue` - 任务编辑页面
- `src/views/bi/alarm-preview/Index.vue` - 告警预览页面

### 5. 调试工具（增强版）

创建了增强的 `ScaleDebugInfo.vue` 组件用于深度分析缩放效果：

- 显示屏幕分辨率和视口尺寸
- 显示App元素实际尺寸和位置
- 显示实际的 transform 值
- 显示理论和实际缩放比例对比
- 显示适配情况分析（Transform缩放/元素适配/标准布局）
- 提供调试建议和说明

## 技术细节

### 监听机制

- `resize` 事件监听窗口大小变化
- `MutationObserver` 监听 `#app` 元素的 style 属性变化
- 防抖处理避免频繁计算

### 性能优化

- 使用 `computed` 返回响应式的高度值
- 防抖函数限制计算频率
- 组件卸载时清理事件监听器

### 兼容性

- 向下兼容：在没有缩放的环境下自动回退到传统方式
- 错误处理：当找不到 `#app` 元素时使用默认计算方式

## 使用建议

1. **替换固定高度**：将所有 `h-[calc(100vh-Npx)]` 替换为响应式高度
2. **合理设置固定高度**：根据页面布局准确计算需要减去的固定高度
3. **测试不同分辨率**：在超过1980x1080的分辨率下验证滚动效果
4. **保留调试信息**：在开发阶段保留 ScaleDebugInfo 组件便于调试
