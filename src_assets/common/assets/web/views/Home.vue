<template>
  <div class="home-page">
    <Navbar v-if="!showSetupWizard" />

    <!-- 首次设置向导 -->
    <SetupWizard
      v-if="showSetupWizard"
      :adapters="adapters"
      :display-devices="displayDevices"
      @setup-complete="onSetupComplete"
    />

    <!-- 正常首页内容 -->
    <main v-if="!showSetupWizard" id="content" class="home-content">
      <section class="home-summary" aria-labelledby="home-title">
        <header class="home-summary-header">
          <h1 id="home-title">Host overview</h1>
          <p>Identity, published resources, and input-only streaming.</p>
        </header>

        <dl class="home-data-table">
          <div class="home-data-row">
            <dt>Host</dt>
            <dd class="home-data-value">{{ hostConfig?.sunshine_name || 'Sunshine' }}</dd>
            <dd class="home-data-detail">{{ hostConfig?.platform || 'Local streaming host' }}</dd>
          </div>

          <div class="home-data-row">
            <dt>{{ $t('navbar.clients') }}</dt>
            <dd class="home-data-value home-data-number">{{ clientsCount ?? '—' }}</dd>
            <dd class="home-data-detail">Paired client devices</dd>
          </div>

          <div class="home-data-row">
            <dt>{{ $t('navbar.applications') }}</dt>
            <dd class="home-data-value home-data-number">{{ appsCount ?? '—' }}</dd>
            <dd class="home-data-detail">Published applications</dd>
          </div>

          <div class="home-data-row home-mode-row">
            <dt>Stream input</dt>
            <dd class="home-data-value">
              {{ inputOnlyEnabled ? $t('index.input_only_on') : $t('index.input_only_off') }}
            </dd>
            <dd class="home-data-detail">
              <span>{{ $t('index.input_only_summary') }}</span>
              <span v-if="inputOnlyMessage" class="input-only-message" role="status">
                {{ inputOnlyMessage }}
              </span>
            </dd>
            <dd class="home-data-action">
              <button
                type="button"
                class="input-only-toggle"
                :aria-pressed="inputOnlyEnabled === true"
                :disabled="inputOnlyEnabled === null || inputOnlySaving"
                @click="toggleInputOnlyMode"
              >
                <i v-if="inputOnlySaving" class="fas fa-spinner fa-spin" aria-hidden="true"></i>
                <span>{{ inputOnlyEnabled ? $t('index.input_only_disable') : $t('index.input_only_enable') }}</span>
              </button>
            </dd>
          </div>
        </dl>
      </section>

      <div v-if="hostStatus === 'error'" class="home-alert" role="alert">
        <div>
          <strong>{{ $t('_common.error') }}</strong>
          <p>{{ $t('index.startup_errors').replace(/<[^>]+>/g, '') }}</p>
        </div>
      </div>

      <!-- 错误日志 -->
      <ErrorLogs :fatal-logs="fatalLogs" />
    </main>
  </div>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import Navbar from '../components/layout/Navbar.vue'
import SetupWizard from '../components/SetupWizard.vue'
import ErrorLogs from '../components/common/ErrorLogs.vue'
import { useLogs } from '../composables/useLogs.js'
import { useSetupWizard } from '../composables/useSetupWizard.js'
import { trackEvents } from '../config/firebase.js'
import { getBootstrapConfig } from '../config/bootstrapData.js'
import { apiJson } from '../utils/apiFetch.js'

const { t } = useI18n()
const hostConfig = ref(null)
const hostStatus = ref('loading')
const appsCount = ref(null)
const clientsCount = ref(null)
const inputOnlyEnabled = ref(null)
const inputOnlySaving = ref(false)
const inputOnlyMessage = ref('')

const countCollection = (payload, key) => {
  if (Array.isArray(payload)) return payload.length
  if (Array.isArray(payload?.[key])) return payload[key].length
  if (key === 'clients' && Array.isArray(payload?.named_certs)) return payload.named_certs.length
  return null
}

const loadInputOnlyMode = async () => {
  const result = await apiJson('/api/input-only-mode')
  inputOnlyEnabled.value = result.enabled === true
}

const toggleInputOnlyMode = async () => {
  if (inputOnlyEnabled.value === null || inputOnlySaving.value) return

  inputOnlySaving.value = true
  inputOnlyMessage.value = ''
  const nextEnabled = !inputOnlyEnabled.value

  try {
    const result = await apiJson('/api/input-only-mode', {
      method: 'POST',
      body: { enabled: nextEnabled },
    })
    inputOnlyEnabled.value = result.enabled === true
    inputOnlyMessage.value = result.activeSessions > 0
      ? t('index.input_only_existing_sessions')
      : t('index.input_only_saved')
  } catch (error) {
    inputOnlyMessage.value = error.message || t('index.input_only_failed')
  } finally {
    inputOnlySaving.value = false
  }
}

const { fatalLogs, fetchLogs } = useLogs()

const { showSetupWizard, adapters, displayDevices, checkSetupWizard, onSetupComplete } = useSetupWizard()

// 初始化
onMounted(async () => {
  // 记录页面访问
  trackEvents.pageView('home')

  try {
    const config = await getBootstrapConfig()
    hostConfig.value = config
    hostStatus.value = 'ready'

    // 检查是否需要显示设置向导
    if (checkSetupWizard(config)) {
      return
    }

    const [appsResult, clientsResult, inputOnlyResult] = await Promise.allSettled([
      apiJson('/api/apps'),
      apiJson('/api/clients/list'),
      loadInputOnlyMode(),
    ])
    if (appsResult.status === 'fulfilled') appsCount.value = countCollection(appsResult.value, 'apps')
    if (clientsResult.status === 'fulfilled') clientsCount.value = countCollection(clientsResult.value, 'clients')
    if (inputOnlyResult.status === 'rejected') {
      inputOnlyEnabled.value = false
      inputOnlyMessage.value = t('index.input_only_failed')
    }

    await fetchLogs()
  } catch (e) {
    hostStatus.value = 'error'
    // 在预览模式下，API 不可用是正常的，只记录警告
    if (e?.message?.includes('JSON') || e?.message?.includes('<!DOCTYPE')) {
      console.warn('API not available in preview mode:', e.message)
    } else {
      console.error('Failed to initialize:', e)
      trackEvents.errorOccurred('home_initialization', e.message)
    }
  }
})
</script>

<style>
@import '../styles/global.less';

.home-content {
  min-height: calc(100vh - var(--ui-navbar-height));
  padding: 16px;
}

.home-summary {
  width: 100%;
  border: 1px solid var(--ui-border);
  background: var(--ui-surface);
}

.home-summary-header {
  min-height: 58px;
  padding: 10px 12px;
  border-bottom: 1px solid var(--ui-border);
}

.home-summary-header h1 {
  margin: 0;
  color: var(--ui-text-primary);
  font-size: 15px;
  font-weight: 600;
  line-height: 20px;
}

.home-summary-header p {
  margin: 2px 0 0;
  color: var(--ui-text-secondary);
  font-size: 12px;
  line-height: 18px;
}

.home-data-table {
  margin: 0;
}

.home-data-row {
  display: grid;
  min-height: 48px;
  grid-template-columns: 150px minmax(160px, 0.7fr) minmax(240px, 1.3fr) auto;
  align-items: center;
  column-gap: 16px;
  margin: 0;
  padding: 8px 12px;
  border-bottom: 1px solid var(--ui-border-soft);
}

.home-data-row:last-child {
  border-bottom: 0;
}

.home-data-row dt,
.home-data-row dd {
  min-width: 0;
  margin: 0;
}

.home-data-row dt {
  color: var(--ui-text-secondary);
  font-size: 12px;
}

.home-data-value {
  overflow: hidden;
  color: var(--ui-text-primary);
  font-size: 13px;
  font-weight: 600;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.home-data-number {
  font-family: var(--font-family-mono);
  font-weight: 400;
}

.home-data-detail {
  overflow: hidden;
  color: var(--ui-text-secondary);
  font-size: 12px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.home-data-detail > span {
  display: block;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.home-data-detail .input-only-message {
  color: var(--ui-text-primary);
  font-size: 11px;
}

.home-data-action {
  justify-self: end;
}

.input-only-toggle {
  display: inline-flex;
  min-width: 116px;
  min-height: 32px;
  align-items: center;
  justify-content: center;
  gap: 6px;
  padding: 0 10px;
  border: 1px solid var(--ui-border-strong);
  background: var(--ui-surface-strong);
  color: var(--ui-text-primary);
  font-size: 12px;
  font-weight: 600;
  transition: background-color 100ms, color 100ms, border-color 100ms;
}

.input-only-toggle:hover:not(:disabled) {
  background: var(--ui-surface-hover);
}

.input-only-toggle:focus-visible {
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 1px;
}

.input-only-toggle:disabled {
  cursor: not-allowed;
  color: var(--ui-text-muted);
}

.home-alert {
  margin-top: 12px;
  padding: 10px 12px;
  border: 1px solid var(--ui-danger-border);
  background: var(--ui-danger-soft);
  color: var(--ui-danger-text);
}

.home-alert p {
  margin: 2px 0 0;
  font-size: 12px;
}

@media (max-width: 760px) {
  .home-content {
    padding: 12px;
  }

  .home-data-row {
    grid-template-columns: 112px minmax(0, 1fr) auto;
    column-gap: 10px;
  }

  .home-data-detail {
    display: none;
  }

  .home-data-action {
    grid-column: 2 / -1;
    justify-self: stretch;
    margin-top: 8px;
  }

  .home-mode-row {
    padding-top: 10px;
    padding-bottom: 10px;
  }

  .input-only-toggle {
    width: 100%;
  }
}
</style>
