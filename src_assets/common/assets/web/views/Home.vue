<template>
  <div class="home-page">
    <Navbar v-if="!showSetupWizard" />

    <!-- 首次设置向导 -->
    <SetupWizard
      v-if="showSetupWizard"
      :adapters="adapters"
      :display-devices="displayDevices"
      :has-locale="hasLocale"
      @setup-complete="onSetupComplete"
    />

    <!-- 正常首页内容 -->
    <main v-if="!showSetupWizard" id="content" class="container home-content">
      <section class="home-hero" aria-labelledby="home-title">
        <div class="page-header home-intro">
          <div class="home-intro-copy">
            <h1 id="home-title" class="page-title">{{ $t('index.welcome') }}</h1>
          </div>
        </div>

        <div class="host-overview">
          <div class="host-overview-main">
            <span class="host-mark" aria-hidden="true">
              <i class="fas fa-server"></i>
            </span>
            <div class="host-summary">
              <h2 class="host-name">{{ hostConfig?.sunshine_name || 'Sunshine' }}</h2>
              <p class="host-meta">
                <span v-if="hostConfig?.platform">{{ hostConfig.platform }}</span>
                <span v-if="hostConfig?.platform" aria-hidden="true"> · </span>
                <span>{{ versionLabel }}</span>
              </p>
            </div>
          </div>

          <div class="host-metrics" aria-label="Host summary">
            <div class="host-metric">
              <span class="metric-icon" aria-hidden="true"><i class="fas fa-display"></i></span>
              <span class="metric-value">{{ clientsCount ?? '—' }}</span>
              <span class="metric-label">{{ $t('navbar.clients') }}</span>
            </div>
            <div class="host-metric">
              <span class="metric-icon" aria-hidden="true"><i class="fas fa-gamepad"></i></span>
              <span class="metric-value">{{ appsCount ?? '—' }}</span>
              <span class="metric-label">{{ $t('navbar.applications') }}</span>
            </div>
          </div>

        </div>
      </section>

      <div v-if="hostStatus === 'error'" class="home-alert" role="alert">
        <i class="fas fa-triangle-exclamation" aria-hidden="true"></i>
        <div>
          <strong>{{ $t('_common.error') }}</strong>
          <p>{{ $t('index.startup_errors').replace(/<[^>]+>/g, '') }}</p>
        </div>
      </div>

      <!-- 错误日志 -->
      <ErrorLogs :fatal-logs="fatalLogs" />

      <!-- 版本信息 -->
      <VersionCard
        v-if="showVersionDetails"
        id="version-details"
        :version="version"
        :github-version="githubVersion"
        :pre-release-version="preReleaseVersion"
        :notify-pre-releases="notifyPreReleases"
        :loading="loading"
        :stable-build-available="stableBuildAvailable"
        :pre-release-build-available="preReleaseBuildAvailable"
        :parsed-stable-body="parsedStableBody"
        :parsed-pre-release-body="parsedPreReleaseBody"
      />
    </main>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import Navbar from '../components/layout/Navbar.vue'
import SetupWizard from '../components/SetupWizard.vue'
import ErrorLogs from '../components/common/ErrorLogs.vue'
import VersionCard from '../components/common/VersionCard.vue'
import { VERSION_CHECK_STATUS, useVersion } from '../composables/useVersion.js'
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

const hasStableUpdate = computed(() => stableBuildAvailable.value)

const versionLabel = computed(() => {
  const currentVersion = version.value?.version || hostConfig.value?.version
  return currentVersion ? `Ver ${currentVersion}` : t('index.loading_latest')
})

// The card carries update notices only, so it appears only when there is one.
// Pre-release notifications were removed, so only a stable update opens it.
const showVersionDetails = computed(() => hasStableUpdate.value)

const countCollection = (payload, key) => {
  if (Array.isArray(payload)) return payload.length
  if (Array.isArray(payload?.[key])) return payload[key].length
  if (key === 'clients' && Array.isArray(payload?.named_certs)) return payload.named_certs.length
  return null
}

// 使用组合式函数
const {
  version,
  githubVersion,
  preReleaseVersion,
  notifyPreReleases,
  versionCheckStatus,
  loading,
  stableBuildAvailable,
  preReleaseBuildAvailable,
  parsedStableBody,
  parsedPreReleaseBody,
  fetchVersions,
} = useVersion()

const { fatalLogs, fetchLogs } = useLogs()

const { showSetupWizard, adapters, displayDevices, hasLocale, checkSetupWizard, onSetupComplete } = useSetupWizard()

// 上报显卡信息
const reportGPUInfo = (config) => {
  try {
    const adapters = config.adapters || []
    const adapterNames = adapters.map((a) => (typeof a === 'string' ? a : a?.name || String(a))).join(', ')

    const gpuInfo = {
      platform: config.platform || 'unknown',
      adapter_count: adapters.length,
      adapters: adapterNames,
      selected_adapter: config.adapter_name || (adapters.length ? 'auto' : 'none'),
      has_selected_adapter: !!config.adapter_name,
    }

    trackEvents.gpuReported(gpuInfo)
  } catch (error) {
    console.error('Failed to report GPU information:', error)
  }
}

// 初始化
onMounted(async () => {
  // 记录页面访问
  trackEvents.pageView('home')

  try {
    const config = await getBootstrapConfig()
    hostConfig.value = config
    hostStatus.value = 'ready'

    setTimeout(() => {
      reportGPUInfo(config)
    }, 1000)

    // 检查是否需要显示设置向导
    if (checkSetupWizard(config)) {
      return
    }

    const [appsResult, clientsResult] = await Promise.allSettled([
      apiJson('/api/apps'),
      apiJson('/api/clients/list'),
    ])
    if (appsResult.status === 'fulfilled') appsCount.value = countCollection(appsResult.value, 'apps')
    if (clientsResult.status === 'fulfilled') clientsCount.value = countCollection(clientsResult.value, 'clients')

    // 版本和日志互不阻塞，避免外部版本检查拖延本机状态提示
    await Promise.allSettled([fetchVersions(config), fetchLogs()])

    // 更新页面标题
    if (version.value) {
      document.title += ` Ver ${version.value.version}`
    }
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
  display: flex;
  /*
   * Was calc(100vh - 77px) with the old mascot-height bar hardcoded, plus a
   * 1180px max-width that stranded a gutter either side on a wide window.
   */
  min-height: calc(100vh - var(--ui-navbar-height));
  box-sizing: border-box;
  flex-direction: column;
  padding-top: 0.45rem;
  padding-bottom: 0.35rem;
}

/*
 * The hero is a framed panel, not a decorated one: the gradient fill, the
 * blur and the two floating blobs that used to be drawn with ::before and
 * ::after are gone.
 */
.home-hero {
  position: relative;
  margin: 0.45rem 0 0.7rem;
  overflow: hidden;
  border: 1px solid var(--ui-border);
  border-radius: 0;
  background: var(--ui-surface);
}

.home-intro {
  position: relative;
  z-index: 1;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  margin: 0;
  padding: 0.72rem 1rem 0.65rem 1.1rem;
}

.home-intro .page-title {
  margin-bottom: 0;
  font-size: 1.05rem;
  line-height: 1.3;
}

.home-status {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  flex-shrink: 0;
  padding: 0.45rem 0.7rem;
  border: 1px solid var(--ui-border);
  border-radius: 0;
  background: var(--ui-surface-strong);
  color: var(--ui-text-primary);
  font-size: var(--font-size-sm);
  font-weight: 600;
  font-family: inherit;
  text-align: left;
  transition: var(--transition-default);
}

.home-status-action {
  appearance: none;
  cursor: pointer;
}

.home-status-action:hover {
  border-color: var(--ui-border-strong);
}

.home-status-action:focus-visible {
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 0;
}

/* Status marker: a square swatch in the state color, no halo. */
.home-status-dot {
  width: 0.48rem;
  height: 0.48rem;
  border-radius: 0;
  background: currentColor;
}

/*
 * "Nothing is wrong" does not need a colour. Colour is reserved for states the
 * user has to act on - loading, error, update available - so a healthy host
 * reads as quiet chrome rather than a green badge.
 */
.home-status-ready {
  border-color: var(--ui-border);
  background: var(--ui-surface-raised);
  color: var(--ui-text-secondary);
}

.home-status-loading {
  border-color: var(--ui-warning-border);
  background: var(--ui-warning-soft);
  color: var(--ui-warning-text);
}

.home-status-error {
  border-color: var(--ui-danger-border);
  background: var(--ui-danger-soft);
  color: var(--ui-danger-text);
}

.home-status-update {
  border-color: var(--ui-info-border);
  background: var(--ui-info-soft);
  color: var(--ui-info-text);
}

.host-overview {
  position: relative;
  z-index: 1;
  display: grid;
  grid-template-columns: minmax(13rem, 1.1fr) auto minmax(29rem, 2fr);
  align-items: center;
  gap: 0.75rem 1rem;
  padding: 0.68rem 0.78rem;
  border-top: 1px solid var(--ui-border);
  background: var(--ui-surface);
}

.host-overview-main {
  display: flex;
  align-items: center;
  gap: 0.7rem;
  min-width: 0;
}

.host-mark {
  display: inline-flex;
  width: 2.55rem;
  height: 2.55rem;
  flex: 0 0 auto;
  align-items: center;
  justify-content: center;
  border: 1px solid var(--ui-border-strong);
  border-radius: 0;
  background: var(--ui-surface-strong);
  color: var(--ui-text-primary);
  font-size: 1rem;
}

.host-summary {
  min-width: 0;
}

.host-name {
  margin: 0;
  overflow: hidden;
  color: var(--ui-text-primary);
  font-size: clamp(1.2rem, 2.3vw, 1.55rem);
  font-weight: 600;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.host-meta {
  margin: 0.22rem 0 0;
  color: var(--ui-text-secondary);
  font-size: var(--font-size-sm);
}

.host-metrics {
  display: flex;
  align-items: stretch;
  gap: 0.38rem;
}

.host-metric {
  display: grid;
  min-width: 5.3rem;
  grid-template-columns: 1.6rem auto;
  grid-template-rows: auto auto;
  column-gap: 0.45rem;
  align-items: center;
  padding: 0.45rem 0.55rem;
  border: 1px solid var(--ui-border);
  border-radius: 0;
  background: var(--ui-surface-strong);
}

.metric-icon {
  display: inline-flex;
  width: 1.6rem;
  height: 1.6rem;
  grid-row: 1 / 3;
  align-items: center;
  justify-content: center;
  border-radius: 0;
  background: var(--ui-surface);
  color: var(--ui-text-muted);
  font-size: 0.7rem;
}

.metric-value {
  color: var(--ui-text-primary);
  font-family: var(--font-family-mono);
  font-size: 1.05rem;
  font-weight: 600;
  line-height: 1;
}

.metric-label {
  color: var(--ui-text-secondary);
  font-size: var(--font-size-xs);
  line-height: 1.1;
}

.quick-actions {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 0.38rem;
}

.quick-action {
  display: grid;
  min-width: 0;
  grid-template-columns: 1.8rem minmax(0, 1fr) auto;
  column-gap: 0.45rem;
  align-items: center;
  padding: 0.42rem 0.5rem;
  border: 1px solid var(--ui-border);
  border-radius: 0;
  background-color: var(--ui-surface-strong);
  color: var(--ui-text-primary);
  text-decoration: none;
  transition: var(--transition-default);
}

.quick-action:hover,
.quick-action:focus-visible {
  border-color: var(--ui-border-strong);
  background-color: var(--ui-surface-hover);
  color: var(--ui-text-primary);
}

.quick-action:focus-visible {
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 0;
}

.quick-action-primary {
  border-color: var(--ui-border-strong);
  background-color: var(--ui-surface-hover);
}

.quick-action-icon {
  display: inline-flex;
  width: 1.8rem;
  height: 1.8rem;
  align-items: center;
  justify-content: center;
  border-radius: 0;
  background: var(--ui-surface);
  color: var(--ui-text-muted);
  font-size: 0.75rem;
}

.quick-action-copy {
  display: block;
  min-width: 0;
}

.quick-action-copy > span,
.quick-action-copy > small {
  display: block;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.quick-action-copy > span {
  font-size: var(--font-size-sm);
  font-weight: 600;
}

.quick-action-copy > small {
  color: var(--ui-text-secondary);
  font-size: var(--font-size-xs);
}

.quick-action-arrow {
  color: var(--ui-text-muted);
  font-size: 0.58rem;
}

#version-details {
  scroll-margin-top: 1rem;
}

@media (min-width: 1440px) {
  .home-content {
    width: calc(100% - 2rem);
    max-width: 1760px;
  }
}

@media (min-width: 1440px) and (min-height: 820px) {
  .home-content {
    padding-top: 0.75rem;
  }

  .home-hero {
    margin-bottom: 0.9rem;
  }

  .home-intro {
    gap: 1.5rem;
    padding: 1rem 1.25rem 0.9rem 1.35rem;
  }

  .home-status {
    padding: 0.55rem 0.9rem;
  }

  .host-overview {
    grid-template-columns: minmax(16rem, 1.2fr) auto minmax(38rem, 2.3fr);
    gap: 1rem 1.25rem;
    padding: 0.9rem 1rem;
  }

  .host-mark {
    width: 3rem;
    height: 3rem;
    font-size: 1.1rem;
  }

  .host-name {
    font-size: 1.7rem;
  }

  .host-metrics,
  .quick-actions {
    gap: 0.55rem;
  }

  .host-metric {
    min-width: 6.25rem;
    grid-template-columns: 1.9rem auto;
    column-gap: 0.55rem;
    padding: 0.58rem 0.7rem;
  }

  .metric-icon {
    width: 1.9rem;
    height: 1.9rem;
    font-size: 0.78rem;
  }

  .metric-value {
    font-size: 1.2rem;
  }

  .quick-action {
    grid-template-columns: 2.1rem minmax(0, 1fr) auto;
    column-gap: 0.58rem;
    padding: 0.58rem 0.68rem;
  }

  .quick-action-icon {
    width: 2.1rem;
    height: 2.1rem;
    font-size: 0.82rem;
  }
}

@media (max-width: 1199.98px) {
  .host-overview {
    grid-template-columns: minmax(0, 1fr) auto;
  }

  .quick-actions {
    grid-column: 1 / -1;
  }
}

.home-alert {
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
  margin: 1rem 0;
  padding: 0.9rem 1rem;
  border: 1px solid var(--ui-danger-border);
  border-radius: 0;
  background: var(--ui-danger-soft);
  color: var(--ui-danger-text);
}

.home-alert > i {
  margin-top: 0.15rem;
  color: var(--ui-danger-text);
}

.home-alert p {
  margin: 0.25rem 0 0;
  font-size: 0.85rem;
}

@media (max-width: 767.98px) {
  .home-intro {
    align-items: flex-start;
    flex-direction: column;
    gap: 0.65rem;
    padding: 0.85rem;
  }

  .home-intro .page-title {
    font-size: 1.7rem;
  }

  .home-status {
    align-self: stretch;
    justify-content: center;
  }

  .host-overview {
    grid-template-columns: 1fr;
    gap: 0.65rem;
    padding: 0.75rem;
  }

  .host-metrics {
    gap: 0.4rem;
  }

  .host-metric {
    flex: 1;
    min-width: 0;
    padding: 0.45rem 0.55rem;
  }

  .quick-actions {
    grid-template-columns: 1fr;
  }
}
</style>
