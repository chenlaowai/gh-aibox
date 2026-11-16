import type { FormInst } from 'naive-ui'

export function useNaiveForm() {
  const formRef = ref<FormInst | null>(null)

  async function validate() {
    await formRef.value?.validate().catch((err) => {
      window.$message.error(err?.[0]?.[0]?.message || '')
      throw err
    })
  }

  async function restoreValidation() {
    formRef.value?.restoreValidation()
  }

  return {
    formRef,
    validate,
    restoreValidation,
  }
}
