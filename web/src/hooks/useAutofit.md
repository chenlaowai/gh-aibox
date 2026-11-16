# Autofit 适配解决方案

## 概述

针对项目中使用 `autofit.js` 进行屏幕适配的环境，提供了一套完整的解决方案来处理 Naive UI 组件（如抽屉、模态框）的缩放适配问题。

## 问题背景

在使用 `autofit.js` 的项目中，Naive UI 的弹出组件（如 `n-drawer`, `n-modal`）会被渲染到 `body` 下的 `.v-binder-follower-container` 中，这个容器在 autofit 配置中被设置为忽略缩放，导致这些组件无法跟随页面缩放。

## 解决方案

### 1. useAutofitDrawer Hook

专门用于处理抽屉组件的缩放适配。

```typescript
import { useAutofitDrawer } from '@/hooks'

// 基础用法
const { drawerWidth, onDrawerVisible } = useAutofitDrawer(730)

// 自定义配置
const { drawerWidth, onDrawerVisible } = useAutofitDrawer(
  800,    // 基础宽度
  1920,   // 设计稿宽度
  300,    // 最小宽度
  1000    // 最大宽度
)
```

#### 参数说明

- `baseWidth`: 基础宽度（设计稿中的抽屉宽度），默认 730
- `designWidth`: 设计稿宽度，默认 1920
- `minWidth`: 最小宽度限制，默认 300
- `maxWidth`: 最大宽度限制，默认 800

#### 返回值

- `drawerWidth`: 响应式的抽屉宽度
- `onDrawerVisible`: 在抽屉显示时调用的方法

#### 使用示例

```vue
<script setup lang="ts">
import { useAutofitDrawer } from '@/hooks'

const { drawerWidth, onDrawerVisible } = useAutofitDrawer(730)

watch(() => props.visible, (newValue) => {
  onDrawerVisible(newValue)
})
</script>

<template>
  <n-drawer
    v-model:show="visible"
    :width="drawerWidth"
    to="#app"
  >
    <!-- 抽屉内容 -->
  </n-drawer>
</template>
```

### 2. useAutofitModal Hook

专门用于处理模态框组件的缩放适配。

```typescript
import { useAutofitModal } from '@/hooks'

// 只设置宽度
const { modalWidth, onModalVisible } = useAutofitModal(600)

// 设置宽度和高度
const { modalWidth, modalHeight, onModalVisible } = useAutofitModal(600, 400)
```

### 3. v-autofit 指令

用于处理其他需要缩放适配的元素。

```vue
<template>
  <!-- 基础用法：缩放宽度和高度 -->
  <div v-autofit="{ baseWidth: 300, baseHeight: 200 }">内容</div>
  
  <!-- 只缩放宽度 -->
  <div v-autofit:width="300">内容</div>
  
  <!-- 缩放字体大小 -->
  <div v-autofit:font-size="16">文字内容</div>
  
  <!-- 自定义配置 -->
  <div v-autofit="{
    baseWidth: 300,
    baseHeight: 200,
    minWidth: 200,
    maxWidth: 500,
    properties: ['width', 'height', 'font-size']
  }">内容</div>
</template>
```

## 最佳实践

### 1. 抽屉组件

```vue
<script setup lang="ts">
import { useAutofitDrawer } from '@/hooks'

const { drawerWidth, onDrawerVisible } = useAutofitDrawer(730)

watch(() => props.visible, onDrawerVisible)
</script>

<template>
  <n-drawer
    v-model:show="visible"
    :width="drawerWidth"
    to="#app"
    placement="right"
  >
    <!-- 内容 -->
  </n-drawer>
</template>
```

### 2. 模态框组件

```vue
<script setup lang="ts">
import { useAutofitModal } from '@/hooks'

const { modalWidth, modalHeight, onModalVisible } = useAutofitModal(600, 400)

watch(() => props.visible, onModalVisible)
</script>

<template>
  <n-modal
    v-model:show="visible"
    :style="{ width: `${modalWidth}px`, height: `${modalHeight}px` }"
    to="#app"
  >
    <!-- 内容 -->
  </n-modal>
</template>
```

### 3. 自定义元素缩放

```vue
<template>
  <!-- 固定大小的卡片 -->
  <div v-autofit="{ baseWidth: 300, baseHeight: 200 }" class="card">
    卡片内容
  </div>
  
  <!-- 响应式字体 -->
  <h1 v-autofit:font-size="24">标题</h1>
</template>
```

## 注意事项

1. **必须使用 `to="#app"`**：确保组件渲染在被 autofit 缩放的容器内
2. **及时调用 onDrawerVisible/onModalVisible**：在组件显示状态改变时调用
3. **合理设置最小/最大值**：避免在极端屏幕尺寸下出现异常
4. **性能考虑**：hooks 内部已经优化了 resize 事件监听，无需额外处理

## 迁移指南

### 从原有实现迁移

将原来的手动计算逻辑替换为 hooks：

```vue
<!-- 迁移前 -->
<script>
const drawerWidth = ref(730)

function updateWidth() {
  const appEl = document.getElementById('app')
  // 复杂的计算逻辑...
}

onMounted(() => {
  window.addEventListener('resize', updateWidth)
})
</script>

<!-- 迁移后 -->
<script>
import { useAutofitDrawer } from '@/hooks'

const { drawerWidth, onDrawerVisible } = useAutofitDrawer(730)

watch(() => props.visible, onDrawerVisible)
</script>
```

这套解决方案提供了完整、可复用、易维护的 autofit 适配能力，可以轻松应用到项目中的各种组件。
