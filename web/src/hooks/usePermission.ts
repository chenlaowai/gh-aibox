import { useAuthStore } from '@/store'

/** 权限判断 */
export function usePermission() {
  const authStore = useAuthStore()

  function hasPermission(
    permission: string,
  ) {
    if (!permission)
      return true

    if (!authStore.userInfo)
      return false
    const { buttons } = authStore.userInfo.permission

    const btnPermissions = buttons?.map(i => i.code)
    const has = btnPermissions?.includes(permission)

    return has
  }

  return {
    hasPermission,
  }
}
