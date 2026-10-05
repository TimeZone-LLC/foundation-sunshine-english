import { ref } from 'vue'

function isTrue(value) {
  return value === true || value === 'true'
}

/**
 * 设置向导组合式函数
 */
export function useSetupWizard() {
  const showSetupWizard = ref(false)
  const adapters = ref([])
  const displayDevices = ref([])

  // 检查是否需要显示设置向导
  const checkSetupWizard = (config) => {
    const setupCompleted = isTrue(config.setup_wizard_completed)
    
    if (setupCompleted) {
      return false
    }

    showSetupWizard.value = true
    adapters.value = config.adapters || []
    displayDevices.value = config.display_devices || []
    return true
  }

  // 设置完成回调
  const onSetupComplete = (config) => {
    console.log('Setup complete:', config)
    // 用户点击"配置应用程序"按钮后会自动跳转到 /apps
  }

  return {
    showSetupWizard,
    adapters,
    displayDevices,
    checkSetupWizard,
    onSetupComplete,
  }
}

