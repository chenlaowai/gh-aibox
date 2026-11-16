import type { App } from 'vue'

export function install(app: App) {
  app.directive('trim', {
    mounted(el) {
      const input = el.querySelector('input, textarea')
      if (!input)
        return

      input.addEventListener('blur', () => {
        if (input.value) {
          const trimmed = input.value.trim()
          if (trimmed !== input.value) {
            input.value = trimmed
            // 触发 v-model 更新
            const event = new Event('input', { bubbles: true })
            input.dispatchEvent(event)
          }
        }
      })
    },
  })
}
