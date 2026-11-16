import type { GlobalThemeOverrides } from 'naive-ui'

export const biTheme: GlobalThemeOverrides = {
  Button: {
    // 自定义按钮样式 - 针对 info 类型按钮
    textColorInfo: '#fff',
    textColorPrimary: '#fff',
    textColorError: '#fff',
    textColorHoverInfo: '#fff',
    textColorPressedInfo: '#fff',
    textColorFocusInfo: '#fff',
    textColorDisabledInfo: '#fff',
    textColorDisabledPrimary: '#fff',
    textColorDisabledError: '#fff',
    colorInfo: '#0C5CFFFF',
    colorHoverInfo: '#0C5CFFFF',
    colorPressedInfo: '#0C5CFFFF',
    colorFocusInfo: '#0C5CFFFF',
    borderInfo: '1px solid #0C5CFFFF',
    borderHoverInfo: '1px solid #0C5CFFFF',
    borderPressedInfo: '1px solid #0C5CFFFF',
    borderFocusInfo: '1px solid #0C5CFFFF',
    borderRadius: '0',
    borderRadiusSmall: '0',
    borderRadiusMedium: '0',
    borderRadiusLarge: '0',
    borderRadiusHuge: '0',
  },

  DataTable: {
    thColor: '#0a1c46',
    thColorModal: '#0a1c46',
    tdColorStripedModal: '#0a1c46',
    tdColor: '#020f29',
    tdColorStriped: '#0a1c46',
    tdColorModal: '#020f29',
    tdColorHover: '#26262a',
    tdColorHoverModal: '#26262a',
    tdColorHoverPopover: '#26262a',
    borderColor: '#1C3988FF',
    borderColorPopover: 'none',
    borderColorModal: 'none',
  },

  Drawer: {
    // Drawer 主题配置，使其背景色与UI主题色一致
    color: '#020924FF',
    colorEmbedded: '#020924FF',
    borderRadius: '0',
    // 标题和分割线的颜色配置
    titleTextColor: '#fff',
    titleFontWeight: '600',
    titleFontSize: '16px',
    // 内容区域配置
    bodyPadding: '20px',
    headerPadding: '20px 20px 12px 20px',
    footerPadding: '12px 20px 20px 20px',
    // 关闭按钮配置
    closeBorderRadius: '4px',
    closeColorHover: 'rgba(255, 255, 255, 0.1)',
    closeColorPressed: 'rgba(255, 255, 255, 0.15)',
    closeIconColor: '#fff',
    closeIconColorHover: '#fff',
    closeIconColorPressed: '#fff',
  },

  Divider: {
    // 分割线颜色，确保在深色背景下可见
    color: 'rgba(255, 255, 255, 0.1)',
    textColor: 'rgba(255, 255, 255, 0.7)',
  },

  Input: {
    // Input 输入框主题配置
    color: '#0E173AFF',
    colorFocus: '#0E173AFF',
    colorFocusWarning: '#0E173AFF',
    colorFocusError: '#0E173AFF',
    textColor: '#fff',
    textColorDisabled: 'rgba(255, 255, 255, 0.5)',
    placeholderColor: 'rgba(255, 255, 255, 0.6)',
    placeholderColorDisabled: 'rgba(255, 255, 255, 0.3)',
    border: '1px solid #374A70FF',
    borderHover: '1px solid #4A5A7FFF',
    borderFocus: '1px solid #5A6A8FFF',
    borderDisabled: '1px solid rgba(55, 74, 112, 0.5)',
    borderWarning: '1px solid #f0a020',
    borderError: '1px solid #d03050',
    borderRadius: '0',
    heightTiny: '22px',
    heightSmall: '28px',
    heightMedium: '40px',
    heightLarge: '48px',
    // 清除按钮等图标的颜色
    clearColorHover: 'rgba(255, 255, 255, 0.8)',
    clearColorPressed: 'rgba(255, 255, 255, 0.9)',
    iconColor: 'rgba(255, 255, 255, 0.7)',
    iconColorHover: 'rgba(255, 255, 255, 0.9)',
    iconColorPressed: 'rgba(255, 255, 255, 1)',
    suffixTextColor: 'rgba(255, 255, 255, 0.7)',
  },

  Select: {
    // Select 选择框主题配置
    peers: {
      InternalSelection: {
        // 选择框输入区域配置
        color: '#0E173AFF',
        colorActive: '#0E173AFF',
        border: '1px solid #374A70FF',
        borderHover: '1px solid #4A5A7FFF',
        borderFocus: '1px solid #5A6A8FFF',
        borderActive: '1px solid #5A6A8FFF',
        borderRadius: '0',
        textColor: '#fff',
        placeholderColor: 'rgba(255, 255, 255, 0.6)',
        arrowColor: 'rgba(255, 255, 255, 0.7)',
        clearColorHover: 'rgba(255, 255, 255, 0.8)',
        clearColorPressed: 'rgba(255, 255, 255, 0.9)',
        heightTiny: '22px',
        heightSmall: '28px',
        heightMedium: '40px',
        heightLarge: '48px',
      },
      InternalSelectMenu: {
        // 下拉菜单配置
        color: '#0E173AFF',
        borderRadius: '0',
        // 选项配置
        optionTextColor: '#fff',
        optionTextColorActive: '#fff',
        // 分组标题
        groupHeaderTextColor: 'rgba(255, 255, 255, 0.7)',
        // 加载状态
        loadingColor: 'rgba(255, 255, 255, 0.7)',
      },
    },
  },

  // 同时添加Dropdown组件的配置（Select内部使用）
  Dropdown: {
    color: '#0E173AFF',
    borderRadius: '0',
    boxShadow: '0 4px 16px rgba(0, 0, 0, 0.3)',
    dividerColor: 'rgba(255, 255, 255, 0.1)',
    optionTextColor: '#fff',
    optionTextColorHover: '#fff',
    optionTextColorActive: '#fff',
    optionColorHover: 'rgba(255, 255, 255, 0.1)',
    optionColorActive: 'rgba(255, 255, 255, 0.15)',
    groupHeaderTextColor: 'rgba(255, 255, 255, 0.7)',
  },

  InputNumber: {
    // InputNumber 数字输入框主题配置
    color: '#0E173AFF',
    colorFocus: '#0E173AFF',
    textColor: '#fff',
    textColorDisabled: 'rgba(255, 255, 255, 0.5)',
    placeholderColor: 'rgba(255, 255, 255, 0.6)',
    border: '1px solid #374A70FF',
    borderHover: '1px solid #4A5A7FFF',
    borderFocus: '1px solid #5A6A8FFF',
    borderRadius: '0',
    // 按钮颜色
    buttonColor: 'rgba(255, 255, 255, 0.1)',
    buttonColorHover: 'rgba(255, 255, 255, 0.2)',
    buttonColorPressed: 'rgba(255, 255, 255, 0.3)',
    buttonTextColor: 'rgba(255, 255, 255, 0.7)',
    buttonTextColorHover: 'rgba(255, 255, 255, 0.9)',
    buttonTextColorPressed: '#fff',
    // 尺寸
    heightTiny: '22px',
    heightSmall: '28px',
    heightMedium: '40px',
    heightLarge: '48px',
  },

  TreeSelect: {
    // TreeSelect 树形选择框主题配置
    peers: {
      InternalSelection: {
        // 选择框输入区域配置
        color: '#0E173AFF',
        border: '1px solid #374A70FF',
        borderHover: '1px solid #4A5A7FFF',
        borderFocus: '1px solid #5A6A8FFF',
        borderRadius: '0',
        textColor: '#fff',
        placeholderColor: 'rgba(255, 255, 255, 0.6)',
        arrowColor: 'rgba(255, 255, 255, 0.7)',
        clearColorHover: 'rgba(255, 255, 255, 0.8)',
        heightTiny: '22px',
        heightSmall: '28px',
        heightMedium: '40px',
        heightLarge: '48px',
      },
      Tree: {
        // 树形下拉菜单配置
        nodeColorHover: 'rgba(255, 255, 255, 0.1)',
        nodeColorPressed: 'rgba(255, 255, 255, 0.15)',
        nodeColorActive: 'rgba(255, 255, 255, 0.15)',
        nodeTextColor: '#fff',
        // 连接线和图标
        lineColor: 'rgba(255, 255, 255, 0.2)',
        arrowColor: 'rgba(255, 255, 255, 0.7)',
        loadingColor: 'rgba(255, 255, 255, 0.7)',
      },
    },
  },

  Form: {
    // Form 表单主题配置
    labelTextColor: '#fff',
    labelTextColorSizeSmall: '#fff',
    labelTextColorSizeMedium: '#fff',
    labelTextColorSizeLarge: '#fff',
    asteriskColor: '#d03050',
    feedbackTextColor: '#fff',
    feedbackTextColorError: '#d03050',
    feedbackTextColorWarning: '#f0a020',
    feedbackTextColorSuccess: '#18a058',
  },

  Space: {
    // Space 间距组件，确保在深色背景下正常显示L
    gapSmall: '8px',
    gapMedium: '16px',
    gapLarge: '24px',
  },
}
