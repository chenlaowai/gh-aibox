/**
 * 将给定的数组转换为树形结构。
 * @param arr - 原始数组，其中每个元素包含id和pid属性，pid表示父级id。
 * @returns 返回转换后的树形结构数组。
 */
export function arrayToTree<T extends { id: string, parentId?: string, children?: T[] }>(arr: T[], parentKey = 'parentId' as 'pid' | 'parentId') {
  // 初始化结果数组
  const res: any = []
  // 使用Map存储数组元素，以id为键，元素本身为值
  const map = new Map()

  // 遍历数组，将每个元素以id为键存储到Map中
  arr.forEach((item) => {
    map.set(item.id, item)
  })

  // 再次遍历数组，根据pid将元素组织成树形结构
  arr.forEach((item) => {
    // 获取当前元素的父级元素
    const parent = item[parentKey] && map.get(item[parentKey])
    // 如果有父级元素
    if (parent) {
      // 如果父级元素已有子元素，则将当前元素追加到子元素数组中
      if (parent?.children)
        parent.children.push(item)
      // 如果父级元素没有子元素，则创建子元素数组，并将当前元素作为第一个元素
      else
        parent.children = [item]
    }
    // 如果没有父级元素，则将当前元素直接添加到结果数组中
    else {
      res.push(item)
    }
  })
  // 返回组织好的树形结构数组
  return res
}

// 递归函数来过滤树节点
export function filterTree(tree: Array<any>, filterFunc: (node: any) => boolean) {
  return tree.filter(filterFunc).map((node) => {
    if (node.children && node.children.length > 0) {
      node.children = filterTree(node.children, filterFunc)
    }
    return node
  })
}

/** 删除指定key */
export function deleteKey<T extends { id?: string | number, childrens?: T[] }>(list: T[], key?: string) {
  function del(arr: T[]) {
    for (const i in arr) {
      const item = arr[i]
      if (item.id === key) {
        arr.splice(Number(i), 1)
        return
      }
      else {
        if (item.childrens) {
          del(item.childrens)
        }
      }
    }
  }
  del(list)
  return list
}

/** 删除未全选时的父级id */
export function removeIds<T extends { id: string | number, childrens?: T[] }>(tree: T[], ids: string[]) {
  for (let i = tree.length - 1; i >= 0; i--) {
    const node = tree[i]
    const children = node.childrens

    if (children) {
      removeIds(children, ids) // 递归调用，处理子节点
    }

    const nodeId = node.id
    if (!ids.includes(nodeId.toString())) {
      continue
    }

    if (children) {
      const childIds = children.map((child: T) => child.id)
      if (!childIds.every((id: string | number) => ids.includes(id.toString()))) {
        ids.splice(ids.indexOf(nodeId.toString()), 1) // 删除id数组中的父id
      }
    }
  }

  return ids
}

/** 递归收集所有层级的父级ID */
export function findPathByIds<T extends { id: string | number, childrens?: T[] }>(tree: T[], targetId: string): string[] {
  const dfs = (nodes: T[], path: string[] = []): string[] | null => {
    for (const node of nodes) {
      const currentPath = [...path, node.id.toString()]

      if (node.id.toString() === targetId) {
        return currentPath
      }

      if (node.childrens?.length) {
        const found = dfs(node.childrens, currentPath)
        if (found)
          return found
      }
    }
    return null
  }

  return dfs(tree) || []
}

/** 树转数组 */
export function flattenTree<T extends { children?: T[] }>(tree: T[]): T[] {
  const result: T[] = []

  function traverse(node: T): void {
    result.push(node)
    if (node.children) {
      for (const child of node.children) {
        traverse(child)
      }
    }
  }

  for (const rootNode of tree) {
    traverse(rootNode)
  }

  return result
}

/** 将空数组转换为undefined */
export function convertEmptyArrayToUndefined<T extends Record<string, any>>(
  obj: T | T[],
  key: keyof T,
): T | T[] {
  if (Array.isArray(obj)) {
    return obj.map(item => convertEmptyArrayToUndefined(item, key)) as T[]
  }

  if (typeof obj === 'object' && obj !== null) {
    const newObj = { ...obj } as T
    for (const k in newObj) {
      if (Object.prototype.hasOwnProperty.call(newObj, k)) {
        if (k === key && Array.isArray(newObj[k]) && newObj[k].length === 0) {
          newObj[k] = undefined as unknown as T[typeof k]
        }
        else {
          newObj[k] = convertEmptyArrayToUndefined(newObj[k], key as keyof T) as T[typeof k]
        }
      }
    }
    return newObj
  }

  return obj
}
