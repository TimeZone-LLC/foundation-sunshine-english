<template>
  <div class="pin-page">
    <Navbar />
    <main id="content" class="pin-content">
      <h1 class="pin-page-title page-title">{{ $t('pin.pin_pairing') }}</h1>

      <div class="pairing-methods">
        <!-- QR Code Pairing -->
        <section class="pairing-section qr-pairing-section" aria-labelledby="qr-pairing-title">
          <div class="pairing-section-heading">
            <div>
              <h2 id="qr-pairing-title"><i class="fas fa-qrcode" aria-hidden="true"></i>{{ $t('pin.qr_pairing') }}</h2>
              <p>{{ $t('pin.qr_pairing_desc') }}</p>
            </div>
          </div>

          <div class="pairing-notice pairing-notice--danger" role="note">
            <i class="fas fa-exclamation-triangle" aria-hidden="true"></i>
            <span>{{ $t('pin.qr_pairing_warning') }}</span>
          </div>

          <!-- QR Code Display -->
          <div v-if="qrActive" class="qr-display">
            <div class="qr-image-wrapper">
              <img :src="qrDataUrl" alt="QR Code" class="qr-image" />
            </div>
            <div class="qr-info">
              <div class="qr-pin-row">
                <span>{{ $t('navbar.pin') }}</span>
                <strong>{{ qrPin }}</strong>
              </div>
              <div class="qr-expiry" :class="{ 'is-expiring': qrRemaining <= 30 }">
                <i class="fas fa-clock" aria-hidden="true"></i>
                {{ $t('pin.qr_expires_in', { seconds: qrRemaining }) }}
              </div>
              <div class="pairing-actions">
                <button class="btn btn-outline-primary btn-sm" @click="generateQrCode" :disabled="qrLoading">
                  <i class="fas fa-sync-alt me-1" aria-hidden="true"></i>{{ $t('pin.qr_refresh') }}
                </button>
                <button class="btn btn-outline-secondary btn-sm" @click="cancelQrCode">
                  <i class="fas fa-times me-1" aria-hidden="true"></i>{{ $t('_common.cancel') }}
                </button>
              </div>
            </div>
          </div>

          <!-- Pairing Success -->
          <div v-else-if="qrPaired" class="pairing-status-row pairing-status-row--success" role="status">
            <i class="fas fa-check-circle" aria-hidden="true"></i>
            <span>{{ $t('pin.qr_paired_success') }}</span>
            <button class="btn btn-outline-primary btn-sm" @click="qrPaired = false">
              {{ $t('_common.ok') }}
            </button>
          </div>

          <!-- Generate Button -->
          <div v-else class="qr-generate-row">
            <div v-if="qrError" class="pairing-status-row pairing-status-row--danger" role="alert">
              <i class="fas fa-exclamation-triangle" aria-hidden="true"></i>
              <span>{{ qrError }}</span>
            </div>
            <button class="btn btn-primary" @click="generateQrCode" :disabled="qrLoading">
              <span v-if="qrLoading" class="spinner-border spinner-border-sm me-2"></span>
              <i v-else class="fas fa-qrcode me-2" aria-hidden="true"></i>
              {{ $t('pin.qr_generate') }}
            </button>
          </div>
        </section>

        <section class="pairing-section manual-pairing-section" aria-labelledby="manual-pairing-title">
          <div class="pairing-section-heading">
            <div>
              <h2 id="manual-pairing-title">{{ $t('pin.pending_requests') }}</h2>
              <p>{{ $t('pin.pending_requests_desc') }}</p>
            </div>
            <button
              type="button"
              class="btn btn-outline-secondary btn-sm"
              :disabled="pendingPairingsLoading || pairingBusy"
              @click="refreshPendingPairings()"
            >
              {{ $t('pin.refresh_requests') }}
            </button>
          </div>

          <div v-if="pendingPairingsLoading" class="pairing-empty" role="status">
            {{ $t('pin.loading_requests') }}
          </div>
          <div v-else-if="pendingPairings.length === 0" class="pairing-empty">
            {{ $t('pin.no_pending_requests') }}
          </div>
          <div v-else class="pairing-request-list" role="list" :aria-label="$t('pin.pending_requests')">
            <button
              v-for="pairing in pendingPairings"
              :key="pairing.id"
              type="button"
              class="pairing-request"
              :class="{ 'is-selected': pairing.id === selectedPairingId }"
              :aria-pressed="pairing.id === selectedPairingId"
              role="listitem"
              @click="selectPairing(pairing.id)"
            >
              <span class="pairing-request-token">{{ pairing.id.slice(0, 8) }}</span>
              <span class="pairing-request-details">
                <strong>{{ pairing.name || $t('pin.unknown_client') }}</strong>
                <span>{{ pairing.address || $t('pin.unknown_address') }}</span>
              </span>
            </button>
          </div>

          <form class="pairing-approval-form" @submit.prevent="approvePairing">
            <label for="pin-input" class="form-label">{{ $t('navbar.pin') }}</label>
            <input
              id="pin-input"
              v-model="pairingPin"
              type="text"
              inputmode="numeric"
              autocomplete="one-time-code"
              pattern="[0-9]{4}"
              maxlength="4"
              class="form-control font-monospace"
              :disabled="!selectedPairing || pairingBusy"
              required
            />

            <label for="name-input" class="form-label">{{ $t('pin.device_name') }}</label>
            <input
              id="name-input"
              v-model="pairingDeviceName"
              type="text"
              maxlength="128"
              class="form-control"
              :disabled="!selectedPairing || pairingBusy"
              required
            />

            <div class="pairing-actions">
              <button type="submit" class="btn btn-primary" :disabled="!selectedPairing || pairingBusy">
                {{ $t('pin.approve_request') }}
              </button>
              <button
                type="button"
                class="btn btn-outline-secondary"
                :disabled="!selectedPairing || pairingBusy"
                @click="cancelPairingRequest"
              >
                {{ $t('pin.cancel_request') }}
              </button>
            </div>
          </form>

          <div class="pairing-notice pairing-notice--warning" role="note">
            <i class="fas fa-exclamation-triangle" aria-hidden="true"></i>
            <span><b>{{ $t('_common.warning') }}</b> {{ $t('pin.warning_msg') }}</span>
          </div>
          <div v-if="pairingStatus === 'success'" class="pairing-status-row pairing-status-row--success" role="status">
            <i class="fas fa-check-circle" aria-hidden="true"></i><span>{{ $t('pin.pairing_success') }}</span>
          </div>
          <div v-else-if="pairingStatus === 'cancelled'" class="pairing-status-row" role="status">
            <i class="fas fa-ban" aria-hidden="true"></i><span>{{ $t('pin.pairing_cancelled') }}</span>
          </div>
          <div v-else-if="pairingStatus === 'invalid'" class="pairing-status-row pairing-status-row--danger" role="alert">
            <i class="fas fa-exclamation-triangle" aria-hidden="true"></i><span>{{ $t('pin.pairing_invalid') }}</span>
          </div>
          <div v-else-if="pairingStatus === 'failed'" class="pairing-status-row pairing-status-row--danger" role="alert">
            <i class="fas fa-exclamation-triangle" aria-hidden="true"></i><span>{{ $t('pin.pairing_failed') }}</span>
          </div>
          <div v-else-if="pairingStatus === 'missing'" class="pairing-status-row pairing-status-row--danger" role="alert">
            <i class="fas fa-exclamation-triangle" aria-hidden="true"></i><span>{{ $t('pin.pairing_missing') }}</span>
          </div>
          <div v-else-if="pairingStatus === 'network'" class="pairing-status-row pairing-status-row--danger" role="alert">
            <i class="fas fa-exclamation-triangle" aria-hidden="true"></i><span>{{ $t('pin.pairing_network_error') }}</span>
          </div>
        </section>
      </div>

      <!-- Unpair all Clients -->
      <section class="paired-clients-section" aria-labelledby="unpair">
        <div class="paired-clients-heading">
          <div>
            <h2 id="unpair">{{ $t('troubleshooting.unpair_title') }}</h2>
            <p>{{ $t('pin.remove_paired_devices_desc') }}</p>
          </div>
          <button class="btn btn-danger" :disabled="unpairAllPressed || loading" @click="handleUnpairAll">
            <span v-if="unpairAllPressed" class="spinner-border spinner-border-sm me-2" role="status"></span>
            {{ $t('troubleshooting.unpair_all') }}
          </button>
        </div>
        <div
          id="apply-alert"
          class="pairing-status-row pairing-status-row--success"
          :style="{ display: showApplyMessage ? 'flex !important' : 'none !important' }"
        >
          <i class="fas fa-check-circle" aria-hidden="true"></i>
          <span><b>{{ $t('_common.success') }}</b> {{ $t('troubleshooting.unpair_single_success') }}</span>
          <button class="btn btn-secondary ms-auto apply" @click="clickedApplyBanner">
            {{ $t('_common.dismiss') }}
          </button>
        </div>
        <div v-if="unpairAllStatus === true" class="pairing-status-row pairing-status-row--success" role="status">
          <i class="fas fa-check-circle" aria-hidden="true"></i>
          <span>{{ $t('troubleshooting.unpair_all_success') }}</span>
        </div>
        <div v-if="unpairAllStatus === false" class="pairing-status-row pairing-status-row--danger" role="alert">
          <i class="fas fa-exclamation-triangle" aria-hidden="true"></i>
          <span>{{ $t('troubleshooting.unpair_all_error') }}</span>
        </div>

        <!-- 加载状态 -->
        <div v-if="loading && clients.length === 0" class="paired-clients-loading" role="status">
          <div class="spinner-border spinner-border-sm" aria-hidden="true"></div>
          <span>{{ $t('pin.loading_clients') }}</span>
        </div>

        <!-- 客户端列表 -->
        <div id="client-list" v-else-if="clients && clients.length > 0" class="client-list-container">
            <div class="brightness-guidance" role="note">
              <i class="fas fa-wand-magic-sparkles" aria-hidden="true"></i>
              <span>{{ $t('pin.hdr_brightness_auto_info') }}</span>
            </div>
            <div class="table-responsive">
              <table class="table table-hover table-bordered align-middle mb-0">
                <thead class="table-dark">
                  <tr>
                    <th scope="col" width="20%" class="ps-3">{{ $t('pin.client_name') }}</th>
                    <th scope="col" width="29%" class="ps-3">{{ $t('pin.hdr_brightness') }}</th>
                    <th scope="col" class="ps-3">
                      <span class="d-inline-flex align-items-center gap-1">
                        {{ $t('pin.color_profile_override') }}
                        <i
                          class="fas fa-info-circle text-info"
                          data-tooltip="hdr-profile"
                          style="cursor: help; font-size: 0.875rem;"
                        ></i>
                      </span>
                    </th>
                    <th scope="col" class="ps-3">
                      <span class="d-inline-flex align-items-center gap-1">
                        {{ $t('pin.device_size') }}
                        <i
                          class="fas fa-info-circle text-info"
                          data-tooltip="device-size"
                          style="cursor: help; font-size: 0.875rem;"
                        ></i>
                      </span>
                    </th>
                    <th scope="col" width="16%" class="text-center">{{ $t('pin.actions') }}</th>
                  </tr>
                </thead>
                <tbody>
                  <tr
                    v-for="client in clients"
                    :key="client.uuid"
                    :class="{ 'table-warning': editingStates[client.uuid] }"
                  >
                    <td class="fw-medium ps-3" :data-label="$t('pin.client_name')">{{ client.name || $t('pin.unknown_client') }}</td>
                    <td class="ps-3 hdr-brightness-cell" :data-label="$t('pin.hdr_brightness')">
                      <div class="cell-control">
                        <select
                          v-if="editingStates[client.uuid]"
                          class="form-select form-select-sm"
                          v-model="client.hdrBrightnessMode"
                          :aria-label="$t('pin.hdr_brightness')"
                        >
                          <option value="auto">{{ $t('pin.hdr_brightness_auto') }}</option>
                          <option value="manual">{{ $t('pin.hdr_brightness_manual') }}</option>
                        </select>
                        <span v-else class="setting-pill">
                          <i
                            :class="client.hdrBrightnessMode === 'manual' ? 'fas fa-sliders' : 'fas fa-wand-magic-sparkles'"
                            aria-hidden="true"
                          ></i>
                          {{ $t(client.hdrBrightnessMode === 'manual' ? 'pin.hdr_brightness_manual' : 'pin.hdr_brightness_auto') }}
                        </span>

                        <div
                          v-if="editingStates[client.uuid] && client.hdrBrightnessMode === 'manual'"
                          class="manual-brightness-grid"
                        >
                          <label class="brightness-field">
                            <span>{{ $t('pin.hdr_brightness_peak') }}</span>
                            <input class="form-control form-control-sm" type="number" min="1" max="10000" step="1"
                              v-model.number="client.hdrBrightnessMaxNits" />
                          </label>
                          <label class="brightness-field">
                            <span>{{ $t('pin.hdr_brightness_min') }}</span>
                            <input class="form-control form-control-sm" type="number" min="0" max="100" step="0.001"
                              v-model.number="client.hdrBrightnessMinNits" />
                          </label>
                          <label class="brightness-field">
                            <span>{{ $t('pin.hdr_brightness_full_frame') }}</span>
                            <input class="form-control form-control-sm" type="number" min="1" max="10000" step="1"
                              v-model.number="client.hdrBrightnessMaxFullFrameNits" />
                          </label>
                        </div>

                        <div v-if="client.hdrBrightnessRuntime?.active" class="brightness-runtime">
                          <span class="runtime-source">
                            <i class="fas fa-circle-check" aria-hidden="true"></i>
                            {{ $t(`pin.hdr_brightness_source_${client.hdrBrightnessRuntime.source}`) }}
                          </span>
                          <span class="runtime-values">
                            {{ $t('pin.hdr_brightness_peak') }} {{ formatHdrNits(client.hdrBrightnessRuntime.maxNits) }} ·
                            {{ $t('pin.hdr_brightness_min') }} {{ formatHdrNits(client.hdrBrightnessRuntime.minNits, 4, true) }} ·
                            {{ $t('pin.hdr_brightness_full_frame') }} {{ formatHdrNits(client.hdrBrightnessRuntime.maxFullFrameNits) }}
                          </span>
                        </div>
                      </div>
                    </td>
                    <td class="ps-3" :data-label="$t('pin.color_profile_override')">
                      <select
                        v-if="editingStates[client.uuid]"
                        class="form-select form-select-sm"
                        v-model="client.hdrProfile"
                      >
                        <option v-if="!hasIccFileList" value="" disabled>{{ $t('pin.modify_in_gui') }}</option>
                        <option v-else value="">{{ $t('pin.system_color_profile') }}</option>
                        <option v-for="item in hdrProfileList" :value="item" :key="item">{{ item }}</option>
                      </select>
                      <span v-else class="setting-value-text">
                        {{ client.hdrProfile || $t('pin.system_color_profile') }}
                      </span>
                    </td>
                    <td class="ps-3" :data-label="$t('pin.device_size')">
                      <select
                        v-if="editingStates[client.uuid]"
                        class="form-select form-select-sm"
                        v-model="client.deviceSize"
                      >
                        <option value="small">{{ $t('pin.device_size_small') }}</option>
                        <option value="medium">{{ $t('pin.device_size_medium') }}</option>
                        <option value="large">{{ $t('pin.device_size_large') }}</option>
                      </select>
                      <span v-else class="setting-value-text">
                        {{ $t(`pin.device_size_${client.deviceSize}`) }}
                      </span>
                    </td>
                    <td class="text-center client-actions-cell" :data-label="$t('pin.actions')">
                      <div class="btn-toolbar justify-content-center" role="toolbar">
                        <!-- 编辑模式按钮 -->
                        <template v-if="!editingStates[client.uuid]">
                          <button
                            class="btn btn-sm btn-outline-primary me-1"
                            @click="startEdit(client.uuid)"
                            :disabled="saving || deleting.has(client.uuid)"
                            :title="$t('pin.edit_client_settings')"
                          >
                            <i class="fas fa-edit me-1"></i> {{ $t('_common.edit') }}
                          </button>
                        </template>
                        <!-- 保存/取消按钮 -->
                        <template v-else>
                          <button
                            class="btn btn-sm btn-primary me-1"
                            @click="handleSave(client.uuid)"
                            :disabled="saving || deleting.has(client.uuid)"
                            :title="$t('pin.save_changes')"
                          >
                            <span v-if="saving" class="spinner-border spinner-border-sm me-1"></span>
                            <i v-else class="fas fa-check me-1"></i> {{ $t('_common.save') }}
                          </button>
                          <button
                            class="btn btn-sm btn-secondary me-1"
                            @click="handleCancelEdit(client.uuid)"
                            :disabled="saving || deleting.has(client.uuid)"
                            :title="$t('pin.cancel_editing')"
                          >
                            <i class="fas fa-times me-1"></i> {{ $t('_common.cancel') }}
                          </button>
                        </template>
                        <!-- 删除按钮 -->
                        <button
                          class="btn btn-sm btn-outline-danger"
                          @click="handleDelete(client)"
                          :disabled="saving || deleting.has(client.uuid) || editingStates[client.uuid]"
                          :title="editingStates[client.uuid] ? $t('pin.save_or_cancel_first') : $t('pin.delete_client')"
                        >
                          <span v-if="deleting.has(client.uuid)" class="spinner-border spinner-border-sm me-1"></span>
                          <i v-else class="fas fa-trash me-1"></i> {{ $t('_common.delete') }}
                        </button>
                      </div>
                      <!-- 未保存更改提示 -->
                      <div
                        v-if="editingStates[client.uuid] && hasUnsavedChanges(client.uuid)"
                        class="text-warning small mt-2"
                      >
                        <i class="fas fa-exclamation-triangle me-1"></i> {{ $t('pin.unsaved_changes') }}
                      </div>
                    </td>
                  </tr>
                </tbody>
              </table>
            </div>
        </div>
        <!-- 空状态 -->
        <div v-else-if="!loading" class="paired-clients-empty">
          {{ $t('troubleshooting.unpair_single_no_devices') }}
        </div>
      </section>

      <!-- 删除确认对话框 -->
      <Transition name="fade">
        <div v-if="clientToDelete" class="delete-client-overlay" @click.self="clientToDelete = null">
          <div class="delete-client-modal">
            <div class="delete-client-header">
              <h5>
                <i class="fas fa-exclamation-triangle me-2"></i>{{ $t('pin.confirm_delete') }}
              </h5>
              <button class="btn-close" @click="clientToDelete = null"></button>
            </div>
            <div class="delete-client-body">
              <i18n-t keypath="pin.delete_confirm_message" tag="p">
                <template #name>
                  <strong>{{ clientToDelete.name || $t('pin.unknown_client') }}</strong>
                </template>
              </i18n-t>
              <p class="text-muted small mb-0">{{ $t('pin.delete_warning') }}</p>
            </div>
            <div class="delete-client-footer">
              <button type="button" class="btn btn-secondary" @click="clientToDelete = null">{{ $t('_common.cancel') }}</button>
              <button type="button" class="btn btn-danger" @click="confirmDelete">
                <span v-if="deleting.has(clientToDelete.uuid)" class="spinner-border spinner-border-sm me-2"></span>
                {{ $t('_common.delete') }}
              </button>
            </div>
          </div>
        </div>
      </Transition>
    </main>
  </div>
</template>

<script setup>
import { onMounted, onUnmounted, ref, nextTick, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { Tooltip } from 'bootstrap'
import Navbar from '../components/layout/Navbar.vue'
import { formatHdrNits, usePin } from '../composables/usePin.js'
import { useQrPair } from '../composables/useQrPair.js'

const { t } = useI18n()

const {
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
  clients,
  hdrProfileList,
  hasIccFileList,
  loading,
  saving,
  deleting,
  editingStates,
  refreshClients,
  unpairAll,
  unpairSingle,
  saveClient,
  startEdit,
  cancelEdit,
  hasUnsavedChanges,
  selectPairing,
  refreshPendingPairings,
  approvePairing,
  cancelPairingRequest,
  clickedApplyBanner,
  loadConfig,
  loadColorProfiles,
} = usePin()

const {
  qrDataUrl,
  qrPin,
  qrRemaining,
  qrLoading,
  qrError,
  qrPaired,
  qrActive,
  generateQrCode,
  cancelQrCode,
} = useQrPair()

const clientToDelete = ref(null)
let pairingRefreshTimer

const handleDelete = (client) => {
  if (editingStates[client.uuid]) return
  clientToDelete.value = client
}

const confirmDelete = async () => {
  if (!clientToDelete.value) return
  const success = await unpairSingle(clientToDelete.value.uuid)
  if (success) clientToDelete.value = null
}

const handleSave = async (uuid) => {
  const success = await saveClient(uuid)
  if (!success) alert(t('pin.save_failed'))
}

const handleCancelEdit = (uuid) => cancelEdit(uuid)

const handleUnpairAll = async () => {
  if (confirm(t('pin.unpair_all_confirm'))) await unpairAll()
}

const initTooltips = () => {
  nextTick(() => {
    const tooltipConfigs = [
      { selector: '[data-tooltip="hdr-profile"]', title: t('pin.hdr_profile_info') },
      { selector: '[data-tooltip="device-size"]', title: t('pin.device_size_info') }
    ]
    
    tooltipConfigs.forEach(({ selector, title }) => {
      const el = document.querySelector(selector)
      if (!el) return
      
      Tooltip.getInstance(el)?.dispose()
      new Tooltip(el, { html: true, placement: 'top', title })
    })
  })
}

onMounted(async () => {
  await loadConfig()
  await refreshPendingPairings()
  await refreshClients()
  await loadColorProfiles()

  pairingRefreshTimer = window.setInterval(() => {
    if (!pairingBusy.value) refreshPendingPairings({ silent: true })
  }, 2000)

  initTooltips()
})

onUnmounted(() => {
  if (pairingRefreshTimer) window.clearInterval(pairingRefreshTimer)
})

watch(clients, initTooltips, { deep: true })
</script>

<style>
@import '../styles/global.less';
</style>

<style scoped lang="less">
.pin-page {
  min-height: 100vh;
  padding-bottom: var(--spacing-xl);
  color: var(--ui-text-primary);
  background: var(--ui-page-bg);

  .page-title {
    color: var(--ui-text-primary) !important;
    font-weight: var(--ui-label-weight);
  }

  .table {
    --bs-table-bg: transparent;
    --bs-table-color: var(--ui-text-primary);
    --bs-table-border-color: var(--ui-border);
    --bs-table-hover-bg: var(--ui-surface-hover);
    --bs-table-hover-color: var(--ui-text-primary);
  }

  .table-dark {
    --bs-table-bg: var(--ui-surface-strong);
    --bs-table-color: var(--ui-text-secondary);
    --bs-table-border-color: var(--ui-border);
  }
}

.pin-content {
  width: 100%;
  max-width: var(--ui-content-max);
  margin-inline: auto;
  padding-inline: var(--ui-space-4);
}

.pin-page-title {
  margin: var(--ui-space-4) 0 var(--ui-space-3);
}

.pairing-methods {
  display: grid;
  grid-template-columns: minmax(280px, 0.72fr) minmax(520px, 1.28fr);
  align-items: start;
  gap: var(--ui-space-3);
}

.pairing-section,
.paired-clients-section {
  background: var(--ui-surface);
  border: 1px solid var(--ui-border);
}

.paired-clients-section {
  margin-top: var(--ui-space-3);
}

.pairing-section-heading,
.paired-clients-heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  min-height: 52px;
  gap: var(--ui-space-3);
  padding: var(--ui-space-2) var(--ui-space-3);
  background: var(--ui-panel-soft);
  border-bottom: 1px solid var(--ui-border);

  h2 {
    display: flex;
    align-items: center;
    gap: var(--ui-space-2);
    margin: 0;
    color: var(--ui-text-primary);
    font-size: var(--ui-font-size-base);
    font-weight: var(--ui-label-weight);
  }

  p {
    margin: 2px 0 0;
    color: var(--ui-text-secondary);
    font-size: var(--font-size-xs);
    line-height: 1.3;
  }
}

.pairing-empty,
.paired-clients-empty,
.paired-clients-loading {
  display: flex;
  align-items: center;
  min-height: 52px;
  gap: var(--ui-space-2);
  padding: var(--ui-space-2) var(--ui-space-3);
  color: var(--ui-text-secondary);
  border-bottom: 1px solid var(--ui-border);
}

.paired-clients-empty {
  border-bottom: 0;
}

.pairing-request-list {
  border-bottom: 1px solid var(--ui-border);
}

.pairing-request {
  display: flex;
  align-items: center;
  width: 100%;
  min-height: 52px;
  gap: var(--ui-space-3);
  padding: var(--ui-space-2) var(--ui-space-3);
  color: var(--ui-text-primary);
  text-align: left;
  background: transparent;
  border: 0;
  border-left: 3px solid transparent;
  border-bottom: 1px solid var(--ui-border);
  transition: background-color 100ms, color 100ms, border-color 100ms;

  &:last-child {
    border-bottom: 0;
  }

  &:hover,
  &.is-selected {
    background: var(--ui-surface-strong);
  }

  &.is-selected {
    border-left-color: var(--ui-border-strong);
  }

  &:focus-visible {
    outline: 2px solid var(--ui-border-strong);
    outline-offset: -2px;
  }
}

.pairing-request-token {
  flex: 0 0 4rem;
  color: var(--ui-text-secondary);
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  line-height: 1;
}

.pairing-request-details {
  display: grid;
  min-width: 0;

  span {
    overflow: hidden;
    color: var(--ui-text-secondary);
    font-family: var(--font-mono);
    font-size: var(--font-size-xs);
    text-overflow: ellipsis;
    white-space: nowrap;
  }
}

.pairing-approval-form {
  display: grid;
  grid-template-columns: auto minmax(72px, 96px) auto minmax(140px, 1fr) auto;
  align-items: center;
  gap: var(--ui-space-2) var(--ui-space-3);
  padding: var(--ui-space-2) var(--ui-space-3);
  border-bottom: 1px solid var(--ui-border);

  .form-label {
    margin: 0;
  }
}

.pairing-actions {
  display: flex;
  flex-wrap: wrap;
  gap: var(--ui-space-2);
}

.pairing-notice,
.pairing-status-row {
  display: flex;
  align-items: flex-start;
  gap: var(--ui-space-2);
  padding: var(--ui-space-2) var(--ui-space-3);
  color: var(--ui-text-secondary);
  border-bottom: 1px solid var(--ui-border);
  font-size: var(--font-size-xs);
  line-height: 1.4;

  > i {
    flex: 0 0 auto;
    margin-top: 2px;
  }
}

.pairing-notice--warning {
  color: var(--ui-warning-text);
  background: var(--ui-warning-soft);
}

.pairing-notice--danger,
.pairing-status-row--danger {
  color: var(--ui-danger-text);
  background: var(--ui-danger-soft);
}

.pairing-status-row--success {
  color: var(--ui-success-text);
  background: var(--ui-success-soft);
}

.paired-clients-heading {
  h2 {
    font-size: var(--ui-font-size-base);
  }
}

.client-list-container {
  margin-top: 0;

  .brightness-guidance {
    display: flex;
    align-items: flex-start;
    gap: var(--ui-space-2);
    padding: var(--ui-space-2) var(--ui-space-3);
    color: var(--ui-text-secondary);
    background: var(--ui-surface-strong);
    border-bottom: 1px solid var(--ui-border);
    border-radius: 0;
    font-size: var(--font-size-xs);
    line-height: 1.4;

    i {
      margin-top: 0.2rem;
      color: var(--ui-text-muted);
    }
  }

  .table-responsive {
    border-radius: 0;
    overflow-x: auto;
    overflow-y: hidden;
    -webkit-overflow-scrolling: touch;
  }

  .table {
    border-radius: 0;
    overflow: hidden;
    margin-bottom: 0;
  }

  .cell-control {
    display: grid;
    gap: 0.55rem;
    min-width: 0;
  }

  // A framed label, square like everything else - it was a rounded pill.
  .setting-pill {
    display: inline-flex;
    align-items: center;
    justify-self: start;
    gap: 0.45rem;
    padding: 0.3rem 0.55rem;
    color: var(--ui-text-primary);
    background: var(--ui-surface-strong);
    border: 1px solid var(--ui-border-strong);
    border-radius: 0;
    font-size: 0.82rem;
    font-weight: 600;

    i {
      color: var(--ui-text-muted);
      font-size: 0.75rem;
    }
  }

  .setting-value-text {
    color: var(--ui-text-secondary);
    font-size: 0.875rem;
    line-height: 1.35;
  }

  .manual-brightness-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(7rem, 1fr));
    gap: 0.5rem;
    padding: 0.65rem;
    background: var(--ui-surface);
    border: 1px solid var(--ui-border);
    border-radius: 0;
  }

  .brightness-field {
    display: grid;
    gap: 0.25rem;
    min-width: 0;
    margin: 0;

    span {
      overflow: hidden;
      color: var(--ui-text-secondary);
      font-size: 0.72rem;
      font-weight: 600;
      line-height: 1.25;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
  }

  .brightness-runtime {
    display: grid;
    gap: 0.2rem;
    padding-left: 0.1rem;
    line-height: 1.35;
  }

  .runtime-source {
    color: var(--ui-success-text);
    font-size: 0.78rem;
    font-weight: 600;

    i {
      margin-right: 0.25rem;
    }
  }

  .runtime-values {
    color: var(--ui-text-muted);
    font-family: var(--font-family-mono);
    font-size: 0.72rem;
  }
}

.table-warning {
  background-color: var(--ui-warning-soft) !important;
}

/* Delete client modal: an opaque panel over a plain scrim. */
.delete-client-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  width: 100vw;
  height: 100vh;
  margin: 0;
  background: var(--ui-overlay);
  z-index: 9999;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: var(--spacing-lg, 20px);
  overflow: hidden;
}

.delete-client-modal {
  background: var(--ui-surface-strong);
  border: 1px solid var(--ui-border-strong);
  border-radius: 0;
  width: 100%;
  max-width: 500px;
  max-height: 80vh;
  display: flex;
  flex-direction: column;
}

.delete-client-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: var(--spacing-md, 20px) var(--spacing-lg, 24px);
  border-bottom: 1px solid var(--ui-border);

  h5 {
    margin: 0;
    color: var(--ui-text-primary);
    font-size: var(--font-size-lg, 1.1rem);
    font-weight: var(--ui-label-weight);
    text-transform: var(--ui-label-transform);
    letter-spacing: var(--ui-label-tracking);
    display: flex;
    align-items: center;
    gap: var(--spacing-sm, 8px);
  }
}

.delete-client-body {
  padding: var(--spacing-lg, 24px);
  font-size: var(--font-size-md, 0.95rem);
  line-height: 1.5;
  overflow-y: auto;
  flex: 1;
  color: var(--ui-text-primary);
}

.delete-client-footer {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  padding: var(--spacing-md, 20px) var(--spacing-lg, 24px);
  border-top: 1px solid var(--ui-border);
  background: var(--ui-surface);

  button {
    padding: 8px 16px;
    font-size: 0.9rem;
  }
}

/* Responsive */
@media (max-width: 768px) {
  .client-list-container {
    .brightness-guidance {
      margin-left: 0 !important;
      margin-right: 0 !important;
    }

    .table-responsive {
      overflow: visible;
    }

    .table {
      display: block;
      border: 0;
      background: transparent;

      thead {
        display: none;
      }

      tbody {
        display: grid;
        gap: 0.75rem;
      }

      tr {
        display: block;
        border: 1px solid var(--ui-border);
        border-radius: 0;
        background: var(--ui-surface);
        overflow: hidden;
      }

      td {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 1rem;
        width: 100%;
        padding: 0.75rem 1rem !important;
        border: 0;
        border-bottom: 1px solid var(--ui-border);
        text-align: right;

        &::before {
          content: attr(data-label);
          min-width: 6.5rem;
          color: var(--ui-text-secondary);
          font-weight: 600;
          text-align: left;
        }

        &:last-child {
          border-bottom: 0;
        }
      }

      .hdr-brightness-cell {
        display: block;
        text-align: left;

        &::before {
          display: block;
          margin-bottom: 0.65rem;
        }

        .cell-control {
          width: 100%;
        }
      }

      .manual-brightness-grid {
        grid-template-columns: 1fr;
      }

      .brightness-field span {
        white-space: normal;
      }

      .runtime-values {
        display: block;
        line-height: 1.5;
      }

      .form-select {
        width: min(100%, 13rem);
        min-height: 44px;
      }

      .client-actions-cell {
        display: block;
        text-align: left;

        &::before {
          display: block;
          margin-bottom: 0.5rem;
        }
      }
    }
  }

  .btn-toolbar {
    flex-direction: column;
    gap: 0.5rem;

    .btn {
      width: 100%;
      min-height: 44px;
      margin: 0;
    }
  }

  .table-responsive {
    font-size: 0.875rem;
  }
}

.qr-image-wrapper {
  display: block;
  padding: var(--ui-space-1);
  background: var(--ui-functional-image-bg);
  border: 1px solid var(--ui-border-strong);
  border-radius: 0;
}

.qr-image {
  display: block;
  width: 176px;
  height: 176px;
  border-radius: 0;
}

.qr-display {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr);
  align-items: center;
  gap: var(--ui-space-3);
  padding: var(--ui-space-3);
  border-bottom: 1px solid var(--ui-border);
}

.qr-info {
  display: grid;
  gap: var(--ui-space-2);
  align-content: center;
}

.qr-pin-row {
  display: flex;
  align-items: baseline;
  gap: var(--ui-space-2);

  span {
    color: var(--ui-text-secondary);
  }

  strong {
    color: var(--ui-text-primary);
    font-family: var(--font-family-mono);
    font-size: var(--font-size-lg);
    letter-spacing: 0;
  }
}

.qr-expiry {
  display: flex;
  align-items: center;
  gap: var(--ui-space-1);
  color: var(--ui-text-muted);
  font-family: var(--font-family-mono);
  font-size: var(--font-size-xs);

  &.is-expiring {
    color: var(--ui-warning-text);
  }
}

.qr-generate-row {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: var(--ui-space-2);
  padding: var(--ui-space-2) var(--ui-space-3);

  .pairing-status-row {
    flex: 1;
    padding: 0;
    border: 0;
    background: transparent;
  }
}

@media (max-width: 960px) {
  .pairing-methods {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 720px) {
  .pairing-approval-form {
    grid-template-columns: auto minmax(0, 1fr);

    .pairing-actions {
      grid-column: 1 / -1;
    }
  }

  .qr-display {
    grid-template-columns: 1fr;
    justify-items: start;
  }

  .paired-clients-heading,
  .pairing-section-heading {
    align-items: flex-start;
    flex-direction: column;
  }
}
</style>
