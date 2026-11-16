import { createDiscreteApi } from 'naive-ui'
// import { router } from '@/router'

export function install() {
    const { message: $message, dialog: $dialog, loadingBar: $loadingBar, modal: $modal, notification: $notification } = createDiscreteApi(['message', 'dialog', 'loadingBar', 'modal', 'notification'])

    window.$loadingBar = $loadingBar
    window.$dialog = $dialog
    window.$message = $message
    window.$modal = $modal
    window.$notification = $notification

    //   系统错误提示
    utils.emitter.on('API:SYSTEM_ERROR', (message: string) => $message.error(message))
}