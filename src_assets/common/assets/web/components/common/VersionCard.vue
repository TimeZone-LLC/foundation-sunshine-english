<template>
  <div id="version-details" class="card shadow-sm mb-4" v-if="version">
    <div class="card-header version-card-header">
      <h5 class="card-title mb-0">
        <i class="fas fa-code-branch me-2"></i>
        Version {{ version.version }}
      </h5>
    </div>
    <div class="card-body">
      <!-- 加载状态 -->
      <div v-if="loading" class="version-loading">
        <i class="fas fa-spinner fa-spin me-2"></i>
        {{ $t('index.loading_latest') }}
      </div>

      <!-- 稳定版本可用 -->
      <div v-if="stableBuildAvailable" class="version-update">
        <div class="version-update-header">
          <div class="version-update-title">
            <i class="fas fa-star text-warning me-2"></i>
            <span>{{ $t('index.new_stable') }}</span>
          </div>
          <button
            type="button"
            class="btn btn-primary btn-download"
            :disabled="pendingNativeChannel !== ''"
            :aria-busy="pendingNativeChannel === 'stable'"
            @click="handleDownloadClick(githubVersion.release.html_url, 'stable')"
          >
            <i :class="pendingNativeChannel === 'stable' ? 'fas fa-spinner fa-spin me-2' : 'fas fa-download me-2'"></i>
            {{ $t('index.download') }}
            <span v-if="nativeUpdaterAvailable" class="native-updater-badge">Control Panel</span>
          </button>
        </div>
        <h3 class="version-release-name">{{ githubVersion.release.name }}</h3>
        <div class="markdown-content" v-html="parsedStableBody"></div>
      </div>
    </div>

    <!-- 下载确认弹窗（与配置页虚拟麦克风下载相同方式，确认后打开下载页） -->
    <ConfirmDialog
      :show="showDownloadConfirm"
      dialog-id="version-download-confirm"
      :title="$t('_common.download')"
      title-icon="fas fa-external-link-alt"
      :close-label="$t('_common.close')"
      @close="cancelDownload"
    >
      <p>{{ $t('index.update_download_confirm') }}</p>
      <template #actions>
        <button type="button" class="btn btn-secondary" @click="cancelDownload">{{ $t('_common.cancel') }}</button>
        <button type="button" class="btn btn-primary" @click="confirmDownload">
          <i class="fas fa-download me-1"></i>{{ $t('_common.download') }}
        </button>
      </template>
    </ConfirmDialog>
  </div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'
import ConfirmDialog from './ConfirmDialog.vue'
import { openExternalUrl } from '../../utils/helpers.js'

defineProps({
  version: Object,
  githubVersion: Object,
  preReleaseVersion: Object,
  notifyPreReleases: Boolean,
  loading: Boolean,
  stableBuildAvailable: Boolean,
  preReleaseBuildAvailable: Boolean,
  parsedStableBody: String,
  parsedPreReleaseBody: String,
})

const showDownloadConfirm = ref(false)
const pendingDownloadUrl = ref('')
const nativeUpdaterAvailable = ref(false)
const pendingNativeChannel = ref('')
let pendingNativeRequestId = ''
let nativeRequestTimer = null
const contextRequestTimers = []

const CONTROL_PANEL_ORIGINS = new Set([
  'http://tauri.localhost',
  'https://tauri.localhost',
  'tauri://localhost',
  'http://localhost:8080',
  'https://localhost:8080',
])

const normalizeOrigin = (url) => {
  try {
    const parsed = new URL(url)
    return parsed.origin === 'null' ? `${parsed.protocol}//${parsed.host}` : parsed.origin
  } catch {
    return ''
  }
}

const controlPanelOrigin = [
  document.referrer,
  window.location.ancestorOrigins?.[0],
].map(normalizeOrigin).find((origin) => CONTROL_PANEL_ORIGINS.has(origin)) || ''

const requestNativeUpdaterContext = () => {
  if (window.parent === window || !CONTROL_PANEL_ORIGINS.has(controlPanelOrigin)) return
  window.parent.postMessage(
    {
      type: 'native-updater-context-request',
      source: 'sunshine-webui',
    },
    controlPanelOrigin
  )
}

const clearNativeRequest = () => {
  pendingNativeChannel.value = ''
  pendingNativeRequestId = ''
  if (nativeRequestTimer) {
    clearTimeout(nativeRequestTimer)
    nativeRequestTimer = null
  }
}

const handleNativeUpdaterMessage = (event) => {
  if (
    event.source !== window.parent
    || event.origin !== controlPanelOrigin
    || !CONTROL_PANEL_ORIGINS.has(event.origin)
    || event.data?.source !== 'sunshine-control-panel'
  ) return

  if (event.data.type === 'native-updater-context') {
    nativeUpdaterAvailable.value = event.data.available === true
    return
  }

  if (
    event.data.type === 'native-update-result'
    && event.data.requestId === pendingNativeRequestId
  ) {
    clearNativeRequest()
  }
}

const requestNativeUpdate = (channel) => {
  if (!CONTROL_PANEL_ORIGINS.has(controlPanelOrigin)) return
  pendingNativeChannel.value = channel
  pendingNativeRequestId = `${Date.now()}-${Math.random().toString(36).slice(2)}`
  window.parent.postMessage(
    {
      type: 'native-update-request',
      source: 'sunshine-webui',
      requestId: pendingNativeRequestId,
      channel,
    },
    controlPanelOrigin
  )

  nativeRequestTimer = setTimeout(clearNativeRequest, 30000)
}

const handleDownloadClick = (url, channel) => {
  if (nativeUpdaterAvailable.value) {
    requestNativeUpdate(channel)
    return
  }

  pendingDownloadUrl.value = url
  showDownloadConfirm.value = true
}

const confirmDownload = async () => {
  const url = pendingDownloadUrl.value
  showDownloadConfirm.value = false
  pendingDownloadUrl.value = ''
  if (!url) return
  try {
    await openExternalUrl(url)
  } catch (error) {
    console.error('Failed to open download URL:', error)
  }
}

const cancelDownload = () => {
  showDownloadConfirm.value = false
  pendingDownloadUrl.value = ''
}

onMounted(() => {
  window.addEventListener('message', handleNativeUpdaterMessage)
  requestNativeUpdaterContext()
  contextRequestTimers.push(setTimeout(requestNativeUpdaterContext, 500))
  contextRequestTimers.push(setTimeout(requestNativeUpdaterContext, 1500))
})

onBeforeUnmount(() => {
  window.removeEventListener('message', handleNativeUpdaterMessage)
  contextRequestTimers.forEach(clearTimeout)
  clearNativeRequest()
})
</script>

<style scoped>
/*
 * The card is now purely an update notice: the header states the installed
 * version, the body states what is available. There is no "you are up to date"
 * banner to style, because the card does not render when that is the case.
 */
.version-loading {
  display: flex;
  align-items: center;
  padding: var(--ui-space-4);
  color: var(--ui-text-muted);
  font-size: var(--font-size-sm);
}

.version-card-header .card-title i {
  color: var(--ui-text-muted);
}

/*
 * The update notice is a framed block one tonal step off the card. The signal
 * is carried by the hairline border, not by a coloured fill.
 */
.version-update {
  margin-top: var(--ui-space-4);
  padding: var(--ui-space-4);
  border: 1px solid var(--ui-warning-border);
  border-radius: var(--ui-radius);
  background: var(--ui-surface);
}

.version-update-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: var(--ui-space-3);
  margin-bottom: var(--ui-space-3);
}

.version-update-title {
  display: flex;
  align-items: center;
  flex: 1;
  min-width: 200px;
  color: var(--ui-text-primary);
  font-size: var(--font-size-sm);
  font-weight: var(--ui-label-weight);
}

.btn-download {
  display: inline-flex;
  align-items: center;
  height: var(--ui-control-height);
  padding: 0 var(--ui-space-3);
  border-radius: var(--ui-radius);
  font-size: var(--font-size-sm);
  font-weight: var(--ui-label-weight);
  white-space: nowrap;
  transition: var(--transition-default);
}

.native-updater-badge {
  display: inline-flex;
  align-items: center;
  margin-left: var(--ui-space-2);
  padding: 1px var(--ui-space-1);
  border: 1px solid currentColor;
  border-radius: var(--ui-radius);
  font-size: var(--font-size-xs);
  font-weight: var(--ui-label-weight);
  line-height: 1.2;
}

.version-release-name {
  margin: var(--ui-space-3) 0 var(--ui-space-2);
  color: var(--ui-text-primary);
  font-size: var(--font-size-md);
  font-weight: var(--ui-label-weight);
}

/* Release notes, rendered from the GitHub release body. */
.markdown-content {
  margin-top: var(--ui-space-3);
  padding: var(--ui-space-4);
  border: 1px solid var(--ui-border);
  border-radius: var(--ui-radius);
  background: var(--ui-panel);
  font-size: var(--font-size-sm);
  line-height: 1.6;
}

.markdown-content h1,
.markdown-content h2,
.markdown-content h3,
.markdown-content h4,
.markdown-content h5,
.markdown-content h6 {
  margin-top: var(--ui-space-4);
  margin-bottom: var(--ui-space-2);
  color: var(--ui-text-primary);
  font-weight: var(--ui-label-weight);
  line-height: 1.25;
}

.markdown-content h1:first-child,
.markdown-content h2:first-child,
.markdown-content h3:first-child {
  margin-top: 0;
}

.markdown-content h1 {
  font-size: 1.25em;
}

.markdown-content h2 {
  font-size: 1.15em;
}

.markdown-content h3 {
  font-size: 1.05em;
}

.markdown-content p {
  margin-bottom: var(--ui-space-2);
  color: var(--ui-text-secondary);
  white-space: pre-line;
}

.markdown-content ul,
.markdown-content ol {
  margin-bottom: var(--ui-space-2);
  padding-left: var(--ui-space-5);
}

.markdown-content li {
  margin-bottom: var(--ui-space-1);
  color: var(--ui-text-secondary);
}

.markdown-content code {
  padding: 0.1em 0.35em;
  border: 1px solid var(--ui-border-soft);
  border-radius: var(--ui-radius);
  background: var(--ui-surface);
  color: var(--ui-text-primary);
  font-family: var(--font-family-mono);
  font-size: 0.92em;
}

.markdown-content pre {
  overflow-x: auto;
  margin: var(--ui-space-3) 0;
  padding: var(--ui-space-3);
  border: 1px solid var(--ui-border);
  border-radius: var(--ui-radius);
  background: var(--ui-surface);
}

.markdown-content pre code {
  padding: 0;
  border: 0;
  background: none;
  color: inherit;
}

.markdown-content blockquote {
  margin: var(--ui-space-3) 0;
  padding-left: var(--ui-space-3);
  border-left: 1px solid var(--ui-border-strong);
  color: var(--ui-text-muted);
}

.markdown-content a {
  color: var(--ui-text-primary);
  font-weight: 500;
  text-decoration: underline;
  text-underline-offset: 2px;
  transition: var(--transition-default);
}

.markdown-content a:hover {
  color: var(--ui-text-muted);
}

.markdown-content table {
  width: 100%;
  margin: var(--ui-space-3) 0;
  border-collapse: collapse;
}

.markdown-content th,
.markdown-content td {
  padding: var(--ui-space-2) var(--ui-space-3);
  border: 1px solid var(--ui-border);
  text-align: left;
}

.markdown-content th {
  background: var(--ui-surface);
  font-weight: var(--ui-label-weight);
}
</style>
