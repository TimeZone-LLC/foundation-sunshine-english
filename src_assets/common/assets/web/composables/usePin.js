import { computed, ref, reactive } from 'vue'
import { getBootstrapConfig } from '../config/bootstrapData.js'
import { apiFetch, apiJson, apiPostJson } from '../utils/apiFetch.js'

const STATUS_RESET_DELAY = 5000

export const isColorProfileFile = (fileName) =>
  typeof fileName === 'string' && /\.(?:icc|icm)$/i.test(fileName)

export const formatHdrNits = (value, fractionDigits = 1, trimTrailingZeros = false) => {
  const number = Number(value)
  if (!Number.isFinite(number)) return '—'

  const formatted = number.toFixed(fractionDigits)
  return trimTrailingZeros
    ? formatted.replace(/(?:\.0+|(\.\d*?[1-9])0+)$/, '$1')
    : formatted
}

const isSuccessfulStatus = (status) => {
  const normalized = status?.toString().toLowerCase()
  return normalized === '1' || normalized === 'true'
}

/**
 * PIN 配对组合式函数
 */
export function usePin() {
  const pairingDeviceName = ref('')
  const pairingPin = ref('')
  const pendingPairings = ref([])
  const selectedPairingId = ref('')
  const pairingStatus = ref('')
  const pairingBusy = ref(false)
  const pendingPairingsLoading = ref(false)
  const unpairAllPressed = ref(false)
  const unpairAllStatus = ref(null)
  const showApplyMessage = ref(false)
  const config = ref(null)
  const clients = ref([])
  const hdrProfileList = ref([])
  const hasIccFileList = ref(false)
  const loading = ref(false)
  const saving = ref(false)
  const deleting = ref(new Set())
  const editingStates = reactive({})
  const originalValues = reactive({})
  const selectedPairing = computed(() =>
    pendingPairings.value.find((pairing) => pairing.id === selectedPairingId.value) ?? null,
  )

  const initClientEditingState = (client) => {
    if (!editingStates[client.uuid]) {
      editingStates[client.uuid] = false
      originalValues[client.uuid] = { ...client }
    }
  }

  const clearEditingState = (uuid) => {
    delete editingStates[uuid]
    delete originalValues[uuid]
  }

  const clearAllEditingStates = () => {
    Object.keys(editingStates).forEach(clearEditingState)
  }

  const parseClients = () => {
    try {
      return JSON.parse(config.value?.clients || '[]')
    } catch {
      return []
    }
  }

  const persistedClient = (client) => {
    const { hdrBrightnessRuntime: _runtime, ...settings } = client
    return settings
  }

  const serialize = (listArray = []) => {
    const nl = '\n'
    return '[' + nl + '    ' + listArray.map((item) => JSON.stringify(item)).join(',' + nl + '    ') + nl + ']'
  }

  const refreshClients = async () => {
    loading.value = true
    try {
      const data = await apiJson('/api/clients/list')

      if (data.status === 'true' && data.named_certs?.length) {
        clients.value = data.named_certs
      }

      const tmpClients = parseClients()
      clients.value = clients.value.map((client) => {
        const stored = tmpClients.find(({ uuid }) => uuid === client.uuid)
        const merged = { ...client, ...(stored ? persistedClient(stored) : {}) }
        // 如果客户端没有deviceSize，设置默认值为medium
        if (!merged.deviceSize) {
          merged.deviceSize = 'medium'
        }
        merged.hdrBrightnessMode ||= 'auto'
        merged.hdrBrightnessMaxNits ??= 1000
        merged.hdrBrightnessMinNits ??= 0.001
        merged.hdrBrightnessMaxFullFrameNits ??= 1000
        initClientEditingState(merged)
        return merged
      })
    } catch (error) {
      console.error('Failed to refresh clients:', error)
    } finally {
      loading.value = false
    }
  }

  const unpairAll = async () => {
    unpairAllPressed.value = true
    try {
      const data = await apiJson('/api/clients/unpair-all', { method: 'POST' })
      showApplyMessage.value = true
      unpairAllStatus.value = data.status.toString() === 'true'

      if (unpairAllStatus.value) {
        clearAllEditingStates()
        await refreshClients()
      }

      setTimeout(() => {
        unpairAllStatus.value = null
      }, STATUS_RESET_DELAY)
    } catch (error) {
      console.error('Failed to unpair all:', error)
      unpairAllStatus.value = false
    } finally {
      unpairAllPressed.value = false
    }
  }

  const unpairSingle = async (uuid) => {
    deleting.value.add(uuid)
    try {
      const data = await apiPostJson('/api/clients/unpair', { uuid })
      const status = data.status?.toString().toLowerCase()
      if (status === '1' || status === 'true') {
        showApplyMessage.value = true
        clearEditingState(uuid)
        await refreshClients()
        return true
      }
      return false
    } catch (error) {
      console.error('Failed to unpair client:', error)
      return false
    } finally {
      deleting.value.delete(uuid)
    }
  }

  const startEdit = (uuid) => {
    const client = clients.value.find((c) => c.uuid === uuid)
    if (client && !originalValues[uuid]) {
      originalValues[uuid] = { ...client }
    }
    editingStates[uuid] = true
  }

  const cancelEdit = (uuid) => {
    const client = clients.value.find((c) => c.uuid === uuid)
    if (client && originalValues[uuid]) {
      Object.assign(client, originalValues[uuid])
    }
    editingStates[uuid] = false
  }

  const saveClient = async (uuid) => {
    if (!config.value) return false

    saving.value = true
    try {
      const tmpClients = parseClients()
      const client = clients.value.find((c) => c.uuid === uuid)
      if (!client) return false

      const index = tmpClients.findIndex((c) => c.uuid === uuid)
      if (index >= 0) {
        tmpClients[index] = persistedClient(client)
      } else {
        tmpClients.push(persistedClient(client))
      }

      config.value.clients = serialize(tmpClients)
      const data = await apiJson('/api/clients/list', {
        method: 'POST',
        body: config.value,
      })

      if (isSuccessfulStatus(data.status)) {
        editingStates[uuid] = false
        originalValues[uuid] = { ...client }
        return true
      }
      return false
    } catch (error) {
      console.error('Failed to save client:', error)
      return false
    } finally {
      saving.value = false
    }
  }

  const save = async () => {
    if (!config.value) return false

    saving.value = true
    try {
      config.value.clients = serialize(clients.value.map(persistedClient))
      const data = await apiJson('/api/clients/list', {
        method: 'POST',
        body: config.value,
      })

      if (isSuccessfulStatus(data.status)) {
        clients.value.forEach((client) => {
          editingStates[client.uuid] = false
          originalValues[client.uuid] = { ...client }
        })
        return true
      }
      return false
    } catch (error) {
      console.error('Failed to save clients:', error)
      return false
    } finally {
      saving.value = false
    }
  }

  const hasUnsavedChanges = (uuid) => {
    const client = clients.value.find((c) => c.uuid === uuid)
    const original = originalValues[uuid]
    const editableFields = [
      'hdrProfile',
      'deviceSize',
      'hdrBrightnessMode',
      'hdrBrightnessMaxNits',
      'hdrBrightnessMinNits',
      'hdrBrightnessMaxFullFrameNits',
    ]
    return client && original && editableFields.some((field) => client[field] !== original[field])
  }

  const selectPairing = (pairingId) => {
    const pairing = pendingPairings.value.find((entry) => entry.id === pairingId)
    if (!pairing) return
    selectedPairingId.value = pairing.id
    pairingDeviceName.value = pairing.name || pairingDeviceName.value
    pairingStatus.value = ''
  }

  const refreshPendingPairings = async ({ silent = false } = {}) => {
    if (!silent) pendingPairingsLoading.value = true
    try {
      const data = await apiJson('/api/pin')
      pendingPairings.value = Array.isArray(data.pairings) ? data.pairings : []

      const selectionStillExists = pendingPairings.value.some(
        (pairing) => pairing.id === selectedPairingId.value,
      )
      if (!selectionStillExists) {
        const first = pendingPairings.value[0]
        selectedPairingId.value = first?.id ?? ''
        if (first?.name) pairingDeviceName.value = first.name
      }
      return true
    } catch (error) {
      console.error('Failed to load pairing requests:', error)
      if (!silent) pairingStatus.value = 'network'
      return false
    } finally {
      if (!silent) pendingPairingsLoading.value = false
    }
  }

  const approvePairing = async () => {
    if (!selectedPairingId.value || !/^\d{4}$/.test(pairingPin.value) || !pairingDeviceName.value) {
      pairingStatus.value = 'invalid'
      return false
    }

    pairingBusy.value = true
    pairingStatus.value = ''
    try {
      const data = await apiPostJson('/api/pin', {
        pairing_id: selectedPairingId.value,
        pin: pairingPin.value,
        name: pairingDeviceName.value,
      })
      const success = isSuccessfulStatus(data.status)
      pairingStatus.value = success ? 'success' : 'failed'
      if (success) pairingPin.value = ''
      await refreshPendingPairings({ silent: true })
      if (success) await refreshClients()
      return success
    } catch (error) {
      console.error('Pairing failed:', error)
      pairingStatus.value = 'network'
      return false
    } finally {
      pairingBusy.value = false
    }
  }

  const cancelPairingRequest = async () => {
    if (!selectedPairingId.value) return false
    const pairingId = selectedPairingId.value
    pairingBusy.value = true
    pairingStatus.value = ''
    try {
      const data = await apiJson('/api/pin', {
        method: 'DELETE',
        body: { pairing_id: pairingId },
      })
      const success = isSuccessfulStatus(data.status)
      pairingStatus.value = success ? 'cancelled' : 'missing'
      await refreshPendingPairings({ silent: true })
      return success
    } catch (error) {
      console.error('Failed to cancel pairing request:', error)
      pairingStatus.value = 'network'
      return false
    } finally {
      pairingBusy.value = false
    }
  }

  const clickedApplyBanner = async () => {
    showApplyMessage.value = false
    try {
      await apiFetch('/api/restart', { method: 'POST' })
    } catch (error) {
      console.error('Failed to restart:', error)
    }
  }

  const loadConfig = async () => {
    try {
      const data = await getBootstrapConfig()
      config.value = data
      pairingDeviceName.value = data.pair_name ?? ''
    } catch (error) {
      console.error('Failed to load config:', error)
    }
  }

  const loadColorProfiles = async () => {
    try {
      const data = await apiJson('/api/color-profiles')
      if (isSuccessfulStatus(data.status) && data.supported !== false) {
        hasIccFileList.value = true
        hdrProfileList.value = (data.profiles || []).filter(isColorProfileFile)
        return
      }
    } catch (error) {
      console.error('Failed to load color profiles:', error)
    }

    // Older desktop shells expose the same information through their bridge.
    if (globalThis.window?.electron?.getIccFileList) {
      hasIccFileList.value = true
      globalThis.window.electron.getIccFileList((files = []) => {
        hdrProfileList.value = files.filter(isColorProfileFile)
      })
    } else {
      hasIccFileList.value = false
    }
  }

  return {
    pairingDeviceName,
    pairingPin,
    pendingPairings,
    selectedPairing,
    selectedPairingId,
    pairingStatus,
    pairingBusy,
    pendingPairingsLoading,
    unpairAllPressed,
    unpairAllStatus,
    showApplyMessage,
    config,
    clients,
    hdrProfileList,
    hasIccFileList,
    loading,
    saving,
    deleting,
    editingStates,
    originalValues,
    refreshClients,
    unpairAll,
    unpairSingle,
    save,
    saveClient,
    startEdit,
    cancelEdit,
    hasUnsavedChanges,
    serialize,
    selectPairing,
    refreshPendingPairings,
    approvePairing,
    cancelPairingRequest,
    clickedApplyBanner,
    loadConfig,
    loadColorProfiles,
  }
}
