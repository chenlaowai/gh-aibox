<script setup lang="ts">
import type { RouteLocationNormalized } from 'vue-router'
import ReloadButton from './ReloadButton.vue'
import DropTabs from './DropTabs.vue'
import ContentFullScreen from './ContentFullScreen.vue'
import { useAppStore, useTabStore } from '@/store'
import IconRedo from '~icons/icon-park-outline/redo'
import IconClose from '~icons/icon-park-outline/close'
import IconDelete from '~icons/icon-park-outline/delete-four'
import IconLeft from '~icons/icon-park-outline/to-left'
import IconRight from '~icons/icon-park-outline/to-right'
import IconFullwith from '~icons/icon-park-outline/fullwidth'

const tabStore = useTabStore()
const appStore = useAppStore()

const router = useRouter()
function handleTab(route: RouteLocationNormalized) {
  router.push(route.path)
}
function handleClose(path: string) {
  tabStore.closeTab(path)
}

const options = computed(() => {
  return [
    {
      label: '刷新',
      key: 'reload',
      icon: () => h(IconRedo),
    },
    {
      label: '关闭',
      key: 'closeCurrent',
      icon: () => h(IconClose),
    },
    {
      label: '关闭其他',
      key: 'closeOther',
      icon: () => h(IconDelete),
    },
    {
      label: '关闭左侧',
      key: 'closeLeft',
      icon: () => h(IconLeft),
    },
    {
      label: '关闭右侧',
      key: 'closeRight',
      icon: () => h(IconRight),
    },
    {
      label: '全部关闭',
      key: 'closeAll',
      icon: () => h(IconFullwith),
    },
  ]
})
const showDropdown = ref(false)
const x = ref(0)
const y = ref(0)
const currentRoute = ref()

function handleSelect(key: string) {
  showDropdown.value = false
  interface HandleFn {
    [key: string]: any
  }
  const handleFn: HandleFn = {
    reload() {
      appStore.reloadPage()
    },
    closeCurrent() {
      tabStore.closeTab(currentRoute.value.path)
    },
    closeOther() {
      tabStore.closeOtherTabs(currentRoute.value.path)
    },
    closeLeft() {
      tabStore.closeLeftTabs(currentRoute.value.path)
    },
    closeRight() {
      tabStore.closeRightTabs(currentRoute.value.path)
    },
    closeAll() {
      tabStore.closeAllTabs()
    },
  }
  handleFn[key]()
}
function handleContextMenu(e: MouseEvent, route: RouteLocationNormalized) {
  e.preventDefault()
  if (tabStore.tabs?.length < 2)
    return
  currentRoute.value = route
  showDropdown.value = false
  nextTick().then(() => {
    showDropdown.value = true
    x.value = e.clientX
    y.value = e.clientY
  })
}
function onClickoutside() {
  showDropdown.value = false
}
</script>

<template>
  <div class="wh-full flex items-end">
    <n-tabs
      :key="tabStore.currentTabPath" type="card" size="small" :tabs-padding="10" :value="tabStore.currentTabPath"
      @close="handleClose"
    >
      <n-tab v-for="item in tabStore.pinTabs" :key="item.path" :name="item.path" @click="router.push(item.path)">
        <div class="flex-x-center gap-2">
          <nova-icon :icon="item.meta.icon" /> {{ item.meta.title }}
        </div>
      </n-tab>
      <n-tab
        v-for="item in tabStore.tabs" :key="item.path" closable :name="item.path" @click="handleTab(item)"
        @contextmenu="handleContextMenu($event, item)"
      >
        <div class="flex-x-center gap-2">
          <nova-icon :icon="item.meta.icon" /> {{ item.meta.title }}
        </div>
      </n-tab>
      <template #suffix>
        <ReloadButton />
        <ContentFullScreen />
        <DropTabs />
      </template>
    </n-tabs>
    <n-dropdown
      placement="bottom-start" trigger="manual" :x="x" :y="y" :options="options" :show="showDropdown"
      :on-clickoutside="onClickoutside" @select="handleSelect"
    />
  </div>
</template>
