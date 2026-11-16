import type { PaginationProps } from 'naive-ui'

export function usePagination({
  page,
  pageSize,
  pageSizes,
  fn,
  pageSizeChange,
}: {
  page?: number
  pageSize?: number
  pageSizes?: number[]
  fn: () => void
  pageSizeChange?: (pageSize: number) => void
}): PaginationProps {
  const pagination = reactive({
    page: page || 1,
    pageCount: 0,
    pageSize: pageSize || (window.screen.height > 1080 ? 20 : 10),
    itemCount: 0,
    showQuickJumper: false,
    showSizePicker: true,
    pageSizes: pageSizes || [10, 20],
    prefix({ itemCount }: { itemCount?: number }) {
      return `共 ${itemCount} 条`
    },
    onChange: (page: number) => {
      pagination.page = page
      fn()
    },
    onUpdatePageSize: (pageSize: number) => {
      pagination.pageSize = pageSize
      pagination.page = 1
      fn()
      pageSizeChange && pageSizeChange(pageSize)
    },
  })

  return pagination
}
