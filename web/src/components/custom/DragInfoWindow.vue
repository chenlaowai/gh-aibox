<template>
  <Teleport :to="to" v-if="showWindow">
    <div class="drag-wrapper absolute" ref="dragRef" v-bind="$attrs">
      <n-layout size="small">
        <div class="info-window">
          <div class="info-window-head drag-handle">
            <div class="info-window-title">
              <slot name="title" />
            </div>
            <div class="info-window-extra">
              <slot name="extra" />
            </div>
            <div class="icon-wrap" @click="handleClose">
              <n-icon :size="16" :component="IconParkOutlineClose" />
            </div>
          </div>

          <div class="info-window-body">
            <slot name="body" />
          </div>
        </div>
      </n-layout>

    </div>
  </Teleport>
</template>

<script setup lang='ts'>
import interact from 'interactjs'
import IconParkOutlineClose from '~icons/icon-park-outline/close';

const dragRef = ref()

const props = withDefaults(
  defineProps<{
    show: boolean
    to: string
  }>(),
  {
    show: false,
    to: 'body'
  }
)

const emits = defineEmits<{
  (e: 'update:value', value: boolean): void
  (e: 'close'): void
  (e: 'move'): void
}>()

const showWindow = computed({
  get() {
    return props.show
  },
  set(val: boolean) {
    emits('update:value', val)
  }
})

function handleClose() {
  showWindow.value = false
  emits('close')
}

function dragMoveListener(event: any) {
  const target = event.target
  // keep the dragged position in the data-x/data-y attributes
  const x = (parseFloat(target.getAttribute('data-x')) || 0) + event.dx
  const y = (parseFloat(target.getAttribute('data-y')) || 0) + event.dy

  // translate the element
  target.style.webkitTransform = target.style.transform = 'translate(' + x + 'px, ' + y + 'px)'

  // update the posiion attributes
  target.setAttribute('data-x', x)
  target.setAttribute('data-y', y)

  emits('move')
}

function initInteract() {
  interact(dragRef.value).draggable({
    ignoreFrom: '.icon-wrap',
    allowFrom: '.drag-handle',
    inertia: true,
    onmove: dragMoveListener,
    // keep the element within the element
    modifiers: [
      interact.modifiers.restrict({
        // keep the element within the element
        restriction: 'parent',  // 'parent' | 'self'
        endOnly: true,
        elementRect: { left: 0, right: 1, top: 0, bottom: 1 }
      })
    ]
  })
}

function init() {
  initInteract()
}

watch(() => showWindow.value, (newValue) => {
  if (newValue) nextTick(init)
})

</script>

<style scoped>
.drag-wrapper {
  position: absolute;
  left: 0;
  top: 0;
  z-index: 999;
  border-radius: 6px;
  overflow: hidden;
  box-shadow: 0 0 10px #eee
}

.info-window {
  position: relative;
  width: 100%;
  height: 100%;
}

.info-window-head {
  position: relative;
  width: 100%;
  height: 45px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  cursor: move;
  user-select: none;
}

.info-window-title {
  font-size: 16px;
}

.info-window-extra {}

.info-window-head,
.info-window-body {
  padding: 16px;
}

.info-window-body {
  height: calc(100% - 45px);
}

.icon-wrap {
  cursor: pointer;
}
</style>