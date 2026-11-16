declare namespace VO {
  type RoleType = 'super' | 'admin' | 'user'

  interface Role {
    /** 用户id */
    id?: number
    /** 用户名 */
    role?: RoleType
  }

}
