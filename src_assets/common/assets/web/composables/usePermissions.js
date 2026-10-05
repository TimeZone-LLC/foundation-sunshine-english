import { computed, ref } from 'vue'
import { apiJson } from '../utils/apiFetch.js'

const PERMISSION_ORDER = ['screen_recording', 'input', 'microphone', 'local_network']

export const orderPermissions = (permissions) => permissions.slice().sort((left, right) => {
  const leftRank = PERMISSION_ORDER.indexOf(left.id)
  const rightRank = PERMISSION_ORDER.indexOf(right.id)
  return (leftRank < 0 ? PERMISSION_ORDER.length : leftRank) -
    (rightRank < 0 ? PERMISSION_ORDER.length : rightRank)
})

export function usePermissions({
  fetchStatuses = () => apiJson('/api/permissions'),
  requestAccess = (id) => apiJson('/api/permissions/request', {
    method: 'POST',
    body: { id },
  }),
} = {}) {
  const permissions = ref([])
  const busyPermission = ref('')
  const error = ref('')

  const orderedPermissions = computed(() => orderPermissions(permissions.value))
  const missingRequired = computed(() => permissions.value.filter((permission) =>
    permission.required && permission.verifiable && permission.status !== 'granted'
  ))

  const refreshPermissions = async () => {
    try {
      const result = await fetchStatuses()
      permissions.value = Array.isArray(result?.permissions) ? result.permissions : []
      error.value = ''
    } catch (cause) {
      error.value = cause?.message || 'Unable to check permissions.'
    }
  }

  const requestPermission = async (id) => {
    if (busyPermission.value) return false
    busyPermission.value = id
    error.value = ''
    try {
      await requestAccess(id)
      await refreshPermissions()
      return true
    } catch (cause) {
      error.value = cause?.message || 'Unable to request permission.'
      return false
    } finally {
      busyPermission.value = ''
    }
  }

  return {
    permissions,
    orderedPermissions,
    missingRequired,
    busyPermission,
    error,
    refreshPermissions,
    requestPermission,
  }
}
