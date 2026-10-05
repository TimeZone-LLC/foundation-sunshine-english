<template>
  <section v-if="permissions.length || error" id="permissions" class="card permissions-panel mb-4" aria-labelledby="permissions-title">
    <div class="card-header permissions-header">
      <h2 id="permissions-title" class="h5 mb-0">
        <i class="fas fa-shield-halved me-2" aria-hidden="true"></i>
        {{ t('troubleshooting.permissions_title') }}
      </h2>
      <button type="button" class="btn btn-outline-secondary btn-sm" @click="refreshPermissions">
        <i class="fas fa-rotate me-2" aria-hidden="true"></i>
        {{ t('troubleshooting.permissions_refresh') }}
      </button>
    </div>
    <div class="card-body">
      <p class="text-muted mb-3">{{ t('troubleshooting.permissions_desc') }}</p>
      <div v-if="error" class="alert alert-danger" role="alert">{{ error }}</div>

      <div v-if="permissions.length" class="table-responsive permissions-table-shell">
        <table class="table align-middle mb-0">
          <thead>
            <tr>
              <th scope="col">{{ t('troubleshooting.permissions_name') }}</th>
              <th scope="col">{{ t('troubleshooting.permissions_status') }}</th>
              <th scope="col">{{ t('troubleshooting.permissions_requirement') }}</th>
              <th scope="col" class="permission-action-column">
                <span class="visually-hidden">{{ t('troubleshooting.permissions_action') }}</span>
              </th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="permission in orderedPermissions" :key="permission.id"
                :class="{ 'permission-row-missing': isMissing(permission) }">
              <th scope="row">
                <strong>{{ permissionName(permission.id) }}</strong>
                <span class="permission-detail">{{ permissionDescription(permission.id) }}</span>
                <span v-if="helpPermission === permission.id" class="permission-help" role="status">
                  {{ permissionHelp(permission.id) }}
                </span>
              </th>
              <td>
                <span class="permission-status" :class="statusClass(permission.status)">
                  {{ permissionStatus(permission.status) }}
                </span>
              </td>
              <td>{{ t(permission.required ? 'troubleshooting.permissions_required' : 'troubleshooting.permissions_optional') }}</td>
              <td class="permission-action-column">
                <button v-if="permission.status !== 'granted' && permission.requestable"
                        type="button"
                        class="btn btn-outline-primary btn-sm"
                        :disabled="busyPermission === permission.id"
                        @click="request(permission.id)">
                  {{ t(permission.id === 'local_network'
                    ? 'troubleshooting.permissions_open_settings'
                    : 'troubleshooting.permissions_request') }}
                </button>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </section>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { usePermissions } from '../composables/usePermissions.js'

const { t, te } = useI18n()
const helpPermission = ref('')

const {
  permissions,
  orderedPermissions,
  busyPermission,
  error,
  refreshPermissions,
  requestPermission,
} = usePermissions()

const translatedPermissionField = (id, suffix = '') => {
  const key = `troubleshooting.permission_${id}${suffix}`
  return te(key) ? t(key) : id
}

const permissionName = (id) => translatedPermissionField(id)
const permissionDescription = (id) => translatedPermissionField(id, '_desc')
const permissionHelp = (id) => translatedPermissionField(id, '_help')
const permissionStatus = (status) => t(`troubleshooting.permissions_status_${status}`)

const isMissing = (permission) =>
  permission.required && permission.verifiable && permission.status !== 'granted'

const statusClass = (status) => ({
  'permission-status-denied': status === 'denied',
  'permission-status-warning': status === 'not_determined' || status === 'unknown',
})

const request = async (id) => {
  if (await requestPermission(id)) {
    helpPermission.value = id
  }
}

onMounted(refreshPermissions)
</script>

<style scoped>
.permissions-panel {
  border-radius: 0;
}

.permissions-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--spacing-md);
  border-bottom: 1px solid var(--ui-border);
}

.permissions-table-shell {
  border: 1px solid var(--ui-border);
}

.permissions-table-shell table {
  min-width: 42rem;
}

.permissions-table-shell tbody th {
  min-width: 18rem;
  font-weight: normal;
}

.permission-detail,
.permission-help {
  display: block;
  margin-top: var(--spacing-xs);
  color: var(--ui-text-secondary);
  font-size: var(--font-size-sm);
  font-weight: normal;
}

.permission-help {
  color: var(--ui-text-primary);
}

.permission-status {
  color: var(--ui-text-secondary);
  font-weight: 600;
}

.permission-status-denied {
  color: var(--ui-danger-text);
}

.permission-status-warning {
  color: var(--ui-warning-text);
}

.permission-row-missing > *:first-child {
  border-left: 2px solid var(--ui-warning-border);
}

.permission-action-column {
  text-align: right;
  white-space: nowrap;
}

.btn {
  border-radius: 0;
  transition: background-color 100ms, color 100ms, border-color 100ms;
}
</style>
