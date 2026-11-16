<script setup lang="ts">
import HelpInfo from '@/components/common/HelpInfo.vue'
import { useBoolean, useLoading } from '@/hooks'

interface Props {
  modalName?: string
  allRoutes: API.MenuTree[]
}

const props = withDefaults(defineProps<Props>(), {
  modalName: '',
})
const emit = defineEmits<{
  open: []
  close: []
}>()

const { bool: modalVisible, setTrue: showModal, setFalse: hiddenModal } = useBoolean(false)
const { loading: submitLoading, startLoading, endLoading } = useLoading(false)
const defaultFormModal: API.AddMenuRequest = {
  name: '',
  url: '',
  icon: '',
  sort: undefined,
  isCatalog: false,
  isHide: false,
  componentPath: '',
}
const formModel = ref()

type ModalType = 'add' | 'view' | 'edit'
const modalType = shallowRef<ModalType>('add')
const modalTitle = computed(() => {
  const titleMap: Record<ModalType, string> = {
    add: '添加',
    view: '查看',
    edit: '编辑',
  }
  return `${titleMap[modalType.value]}${props.modalName}`
})

async function openModal(type: ModalType = 'add', data: API.ModifyMenuRequest) {
  emit('open')
  modalType.value = type
  showModal()
  const handlers = {
    async add() {
      formModel.value = { ...defaultFormModal }
    },
    async view() {
      if (data)
        formModel.value = { ...data }
    },
    async edit() {
      if (data)
        formModel.value = { ...data }
    },
  }
  await handlers[type]()
}

function closeModal() {
  hiddenModal()
  emit('close')
}

defineExpose({
  openModal,
})

const formRef = ref()
async function submitModal() {
  await formRef.value?.validate()
  startLoading()

  const fn = formModel.value.id
    ? api.menu.postMenuModify
    : api.menu.postMenuAdd
  fn({ ...formModel.value } as any).then(() => {
    closeModal()
  }).finally(() => {
    endLoading()
  })
}

const dirTreeOptions = computed(() => {
  return filterDirectory(JSON.parse(JSON.stringify(props.allRoutes)))
})

function filterDirectory(node: any[]) {
  return node.filter((item) => {
    if (item.childrens) {
      const childDir = filterDirectory(item.childrens)
      if (childDir.length > 0)
        item.childrens = childDir
      else
        Reflect.deleteProperty(item, 'childrens')
    }

    return (item.isCatalog)
  })
}

const rules = {
  name: {
    required: true,
    message: '请输入菜单名称',
    trigger: 'blur',
  },
  url: {
    required: true,
    message: '请输入菜单路径',
    trigger: 'blur',
  },
  icon: {
    required: true,
    message: '请选择菜单图标',
    trigger: 'blur',
  },
}
</script>

<template>
  <n-modal
    v-model:show="modalVisible" :mask-closable="false" preset="dialog" :title="modalTitle"
    style="width: 700px;"
    :show-icon="false"
    :segmented="{
      content: true,
      action: true,
    }"
  >
    <n-form
      ref="formRef"
      class="mt-20px"
      :rules="rules" label-placement="top" :label-width="100"
      :model="formModel"
      :disabled="modalType === 'view'"
    >
      <n-grid :cols="2" :x-gap="18">
        <n-form-item-grid-item :span="2" path="parentId">
          <template #label>
            父级目录
            <HelpInfo message="不填写则为顶层菜单" />
          </template>
          <n-tree-select
            v-model:value="formModel.parentId" filterable clearable :options="dirTreeOptions" key-field="id"
            label-field="name" children-field="childrens" placeholder="请选择父级目录"
          />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="2" label="菜单名称" path="name">
          <n-input v-model:value="formModel.name" />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="1" label="图标" path="icon">
          <icon-select v-model:value="formModel.icon" />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="1" path="sort">
          <template #label>
            菜单排序
            <HelpInfo message="数字越小，同级中越靠前" />
          </template>
          <n-input-number v-model:value="formModel.sort" />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="1" label="路由路径" path="url">
          <n-input v-model:value="formModel.url" placeholder="例如: /system/user" />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="1" label="组件路径" path="componentPath">
          <n-input v-model:value="formModel.componentPath" placeholder="例如: /system/user/Index.vue" />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="1" label="是否文件夹" path="isCatalog">
          <n-switch v-model:value="formModel.isCatalog" />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="1" label="侧边菜单隐藏" path="isHide">
          <n-switch v-model:value="formModel.isHide" />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="2" label="备注" path="remark">
          <n-input v-model:value="formModel.remark" />
        </n-form-item-grid-item>
      </n-grid>
    </n-form>
    <template #action>
      <n-space>
        <n-button @click="closeModal">
          取消
        </n-button>
        <n-button type="primary" :loading="submitLoading" @click="submitModal">
          提交
        </n-button>
      </n-space>
    </template>
  </n-modal>
</template>

<style scoped></style>
