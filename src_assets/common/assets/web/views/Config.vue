<template>
  <div class="page-config">
    <Navbar />
    <div class="config-floating-buttons">
      <button
        type="button"
        class="tool-btn tool-btn-primary"
        :class="{ 'has-unsaved': hasUnsaved }"
        @click="requestConfigAction('save')"
        :disabled="riskActionRunning"
        :aria-label="$t('_common.save')"
        :title="hasUnsaved ? $t('config.unsaved_changes_tooltip') : $t('_common.save')"
      >
        <i class="fas fa-save"></i>
      </button>
      <button
        v-if="saved && !restarted"
        type="button"
        class="tool-btn tool-btn-primary"
        @click="requestConfigAction('apply')"
        :disabled="riskActionRunning"
        :aria-label="$t('_common.apply')"
        :title="$t('_common.apply')"
      >
        <i class="fas fa-check"></i>
      </button>
      <div class="floating-toast-container">
        <Transition name="toast">
          <div
            v-if="showSaveToast"
            class="toast align-items-center text-bg-success border-0 show"
            role="alert"
            aria-live="assertive"
            aria-atomic="true"
          >
            <div class="d-flex">
              <div class="toast-body">
                <i class="fas fa-check-circle me-2"></i>
                <b>{{ $t('_common.success') }}</b> {{ $t('config.apply_note') }}
              </div>
              <button
                type="button"
                class="btn-close btn-close-white me-2 m-auto"
                @click="showSaveToast = false"
                aria-label="Close"
              ></button>
            </div>
          </div>
        </Transition>
        <Transition name="toast">
          <div
            v-if="showRestartToast"
            class="toast align-items-center text-bg-success border-0 mt-2 show"
            role="alert"
            aria-live="assertive"
            aria-atomic="true"
          >
            <div class="d-flex">
              <div class="toast-body">
                <i class="fas fa-check-circle me-2"></i>
                <b>{{ $t('_common.success') }}</b> {{ $t('config.restart_note') }}
              </div>
              <button
                type="button"
                class="btn-close btn-close-white me-2 m-auto"
                @click="showRestartToast = false"
                aria-label="Close"
              ></button>
            </div>
          </div>
        </Transition>
      </div>
    </div>

    <ConfirmDialog
      :show="showRiskConfirm"
      dialog-id="risk-confirm"
      :title="$t(riskAction === 'apply' ? 'config.risk_confirm.title_apply' : 'config.risk_confirm.title_save')"
      title-icon="fas fa-exclamation-triangle"
      tone="danger"
      max-width="720px"
      :close-label="$t('_common.close')"
      @close="cancelRiskConfirm"
    >
      <p class="risk-confirm-intro">
        {{ $t(riskAction === 'apply' ? 'config.risk_confirm.intro_apply' : 'config.risk_confirm.intro_save') }}
      </p>
      <div class="risk-confirm-list">
        <div
          v-for="risk in riskItems"
          :key="risk.id"
          class="risk-item"
          :class="risk.severity"
        >
          <div class="risk-item-header">
            <span class="risk-badge" :class="risk.severity">
              {{ $t(`config.risk_confirm.severity_${risk.severity}`) }}
            </span>
            <strong>{{ $t(risk.titleKey) }}</strong>
          </div>
          <p>{{ $t(risk.descriptionKey) }}</p>
          <div v-if="risk.currentValue" class="risk-detail">
            <span>{{ $t('config.risk_confirm.value_label') }}</span>
            <code>{{ risk.currentValue }}</code>
          </div>
          <div v-if="risk.recoveryKey" class="risk-recovery">
            <span>{{ $t('config.risk_confirm.recovery_label') }}</span>
            <p>{{ $t(risk.recoveryKey) }}</p>
          </div>
        </div>
      </div>

      <template #actions>
        <button type="button" class="btn btn-secondary" @click="cancelRiskConfirm" :disabled="riskActionRunning">
          {{ $t('_common.cancel') }}
        </button>
        <button
          type="button"
          class="btn btn-danger"
          @click="confirmRiskAction"
          :disabled="riskActionRunning"
        >
          <i v-if="riskActionRunning" class="fas fa-spinner fa-spin me-1"></i>
          {{ $t(riskAction === 'apply' ? 'config.risk_confirm.confirm_apply' : 'config.risk_confirm.confirm_save') }}
        </button>
      </template>
    </ConfirmDialog>

    <div class="container">
      <h1 class="mt-2 mb-4 page-title">{{ $t('config.configuration') }}</h1>

      <div v-if="!config" class="form card config-skeleton">
        <div class="card-header skeleton-header">
          <div class="skeleton-tabs">
            <div v-for="n in 6" :key="n" class="skeleton-tab"></div>
          </div>
        </div>
        <div class="config-page skeleton-body">
          <div class="skeleton-section">
            <div class="skeleton-title"></div>
            <div v-for="n in 4" :key="n" class="skeleton-row">
              <div class="skeleton-label"></div>
              <div class="skeleton-input"></div>
            </div>
          </div>
          <div class="skeleton-section">
            <div class="skeleton-title"></div>
            <div v-for="n in 3" :key="n" class="skeleton-row">
              <div class="skeleton-label"></div>
              <div class="skeleton-input"></div>
            </div>
          </div>
        </div>
      </div>

      <div v-else class="form card">
        <div class="config-tabs-shell">
          <ul ref="configTabsRef" class="nav nav-tabs config-tabs">
            <template v-for="tab in tabs" :key="tab.id">
              <li
                v-if="tab.type === 'group' && tab.children"
                class="nav-item dropdown"
                :class="{ active: isEncoderTabActive(tab), show: expandedDropdown === tab.id }"
              >
                <a
                  class="nav-link dropdown-toggle"
                  :class="{ active: isEncoderTabActive(tab) }"
                  href="#"
                  role="button"
                  :aria-expanded="expandedDropdown === tab.id"
                  @click.prevent="toggleEncoderDropdown(tab.id, $event)"
                >
                  {{ $t(`tabs.${tab.id}`) || tab.name }}
                </a>
                <ul class="dropdown-menu" :class="{ show: expandedDropdown === tab.id }">
                  <li v-for="childTab in tab.children" :key="childTab.id">
                    <a
                      class="dropdown-item"
                      :class="[{ active: currentTab === childTab.id }, `encoder-item-${childTab.id}`]"
                      href="#"
                      @click.prevent="selectEncoderTab(childTab.id, $event)"
                    >
                      {{ $t(`tabs.${childTab.id}`) || childTab.name }}
                    </a>
                  </li>
                </ul>
              </li>
              <li v-else class="nav-item">
                <a
                  class="nav-link"
                  :class="{ active: tab.id === currentTab }"
                  href="#"
                  @click.prevent="currentTab = tab.id"
                >
                  {{ $t(`tabs.${tab.id}`) || tab.name }}
                </a>
              </li>
            </template>
          </ul>
        </div>

        <General
          v-if="currentTab === 'general'"
          :config="config"
          :global-prep-cmd="global_prep_cmd"
          :platform="platform"
        />
        <Inputs v-if="currentTab === 'input'" :config="config" :platform="platform" />
        <AudioVideo
          v-if="currentTab === 'av'"
          :config="config"
          :platform="platform"
          :resolutions="resolutions"
          :fps="fps"
          :display-mode-remapping="display_mode_remapping"
        />
        <Network v-if="currentTab === 'network'" :config="config" :platform="platform" />
        <Files v-if="currentTab === 'files'" :config="config" :platform="platform" />
        <Advanced v-if="currentTab === 'advanced'" :config="config" :platform="platform" />
        <ContainerEncoders
          v-if="isEncoderCurrentTab"
          :current-tab="currentTab"
          :config="config"
          :platform="platform"
        />
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, provide, computed, onUnmounted, nextTick, defineAsyncComponent } from 'vue'
import Navbar from '../components/layout/Navbar.vue'
import ConfirmDialog from '../components/common/ConfirmDialog.vue'
import { getPreferredEncoderTab } from '../config/encoderTabs.js'
import { useConfig } from '../composables/useConfig.js'
import { trackEvents } from '../config/firebase.js'

const General = defineAsyncComponent(() => import('../configs/tabs/General.vue'))
const Inputs = defineAsyncComponent(() => import('../configs/tabs/Inputs.vue'))
const Network = defineAsyncComponent(() => import('../configs/tabs/Network.vue'))
const Files = defineAsyncComponent(() => import('../configs/tabs/Files.vue'))
const Advanced = defineAsyncComponent(() => import('../configs/tabs/Advanced.vue'))
const AudioVideo = defineAsyncComponent(() => import('../configs/tabs/AudioVideo.vue'))
const ContainerEncoders = defineAsyncComponent(() => import('../configs/tabs/ContainerEncoders.vue'))

const ENCODER_TAB_IDS = new Set(['nv', 'qsv', 'amd', 'vt', 'sw'])

const {
  platform,
  saved,
  restarted,
  config,
  fps,
  resolutions,
  currentTab,
  global_prep_cmd,
  display_mode_remapping,
  tabs,
  initTabs,
  loadConfig,
  save: saveConfig,
  apply: applyConfig,
  getRiskyChanges,
  handleHash,
  hasUnsavedChanges,
} = useConfig()

const showSaveToast = ref(false)
const showRestartToast = ref(false)
const expandedDropdown = ref(null)
const showRiskConfirm = ref(false)
const riskAction = ref('save')
const riskItems = ref([])
const riskActionRunning = ref(false)
const configTabsRef = ref(null)

const hasUnsaved = computed(() => {
  if (!config.value) return false
  void config.value
  void fps.value
  void resolutions.value
  void global_prep_cmd.value
  void display_mode_remapping.value
  return hasUnsavedChanges()
})

const isEncoderCurrentTab = computed(() => ENCODER_TAB_IDS.has(currentTab.value))

const isEncoderTabActive = (tab) => tab.type === 'group' && tab.children?.some((child) => child.id === currentTab.value)

const preferredEncoderTab = (children = []) =>
  getPreferredEncoderTab({
    activeEncoder: config.value?.active_encoder,
    configuredEncoder: config.value?.encoder,
    selectedAdapter: config.value?.adapter_name,
    adapters: config.value?.adapters,
    platform: platform.value,
    availableTabIds: children.map((child) => child.id),
  })

const scrollActiveTabIntoView = async () => {
  await nextTick()
  const activeTab = configTabsRef.value?.querySelector('.nav-link.active')
  const behavior = window.matchMedia?.('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth'
  activeTab?.scrollIntoView?.({ behavior, block: 'nearest', inline: 'center' })
}

const toggleEncoderDropdown = (tabId, event) => {
  event.stopPropagation()

  if (expandedDropdown.value === tabId) {
    expandedDropdown.value = null
    return
  }

  expandedDropdown.value = tabId

  const encoderGroup = tabs.value.find((t) => t.id === tabId && t.type === 'group')
  const children = encoderGroup?.children

  if (children?.length && !children.some((child) => child.id === currentTab.value)) {
    currentTab.value = preferredEncoderTab(children)
  }
}

const selectEncoderTab = (childTabId, event) => {
  event.stopPropagation()
  currentTab.value = childTabId
  expandedDropdown.value = null
}

const showToast = (toastRef, duration = 5000) => {
  toastRef.value = true
  setTimeout(() => {
    toastRef.value = false
  }, duration)
}

const runConfigAction = async (action) => {
  if (riskActionRunning.value) return

  riskActionRunning.value = true
  try {
    if (action === 'apply') {
      await applyConfig()
    } else {
      await saveConfig()
    }
  } finally {
    riskActionRunning.value = false
  }
}

const requestConfigAction = async (action) => {
  if (riskActionRunning.value || showRiskConfirm.value) return

  const risks = getRiskyChanges(action)
  if (risks.length > 0) {
    riskAction.value = action
    riskItems.value = risks
    showRiskConfirm.value = true
    return
  }

  await runConfigAction(action)
}

const cancelRiskConfirm = () => {
  if (riskActionRunning.value) return
  showRiskConfirm.value = false
  riskItems.value = []
}

const confirmRiskAction = async () => {
  if (riskActionRunning.value) return

  const action = riskAction.value
  showRiskConfirm.value = false
  await runConfigAction(action)
  riskItems.value = []
}

watch(currentTab, scrollActiveTabIntoView)

watch(saved, (newVal) => {
  if (newVal && !restarted.value) {
    showToast(showSaveToast)
  }
})

watch(restarted, (newVal) => {
  if (newVal) {
    showSaveToast.value = false
    showToast(showRestartToast)
  }
})

provide(
  'platform',
  computed(() => platform.value)
)

const handleOutsideClick = (event) => {
  if (expandedDropdown.value && !event.target.closest('.dropdown')) {
    expandedDropdown.value = null
  }
}

const handleConfigHash = () => {
  handleHash()

  if (currentTab.value === 'encoders') {
    const encoderGroup = tabs.value.find((tab) => tab.id === 'encoders')
    currentTab.value = preferredEncoderTab(encoderGroup?.children)
  }
}

onMounted(async () => {
  trackEvents.pageView('configuration')
  initTabs()
  await loadConfig()
  handleConfigHash()

  await scrollActiveTabIntoView()

  window.addEventListener('hashchange', handleConfigHash)
  document.addEventListener('click', handleOutsideClick)
})

onUnmounted(() => {
  window.removeEventListener('hashchange', handleConfigHash)
  document.removeEventListener('click', handleOutsideClick)
})
</script>

<style lang="less">
@import '../styles/global.less';

// Layout constants. Radii, easings and shadows are no longer declared here:
// corners are square everywhere and only color transitions are permitted.
@btn-size: 52px;
@btn-size-mobile: 48px;

// Mixins
.flex-center() {
  display: flex;
  justify-content: center;
  align-items: center;
}

.config-page {
  padding: 1em;
  border: 1px solid var(--ui-border);
  border-top: none;
  border-radius: 0;
  background: var(--ui-surface-strong);
  color: var(--ui-text-primary);
}

// Loading placeholders are static blocks. The shimmer gradient that used to
// sweep across them is gone; the skeleton is a plain outline of the layout.
.config-skeleton {
  .skeleton-block() {
    background: var(--ui-skeleton-base);
    border: 1px solid var(--ui-border);
    border-radius: 0;
  }

  .skeleton-header {
    background: var(--ui-surface-strong);
    padding: 0.5rem 1rem;
  }

  .skeleton-tabs {
    display: flex;
    gap: 0.5rem;
    padding: 0.5rem 0;
  }

  .skeleton-tab {
    width: 80px;
    height: 38px;
    .skeleton-block();
  }

  .skeleton-body {
    padding: 1.5rem;
  }

  .skeleton-section {
    margin-bottom: 2rem;
    &:last-child {
      margin-bottom: 0;
    }
  }

  .skeleton-title {
    width: 150px;
    height: 24px;
    .skeleton-block();
    margin-bottom: 1rem;
  }

  .skeleton-row {
    display: flex;
    align-items: center;
    gap: 1rem;
    margin-bottom: 1rem;
    &:last-child {
      margin-bottom: 0;
    }
  }

  .skeleton-label {
    width: 120px;
    height: 16px;
    .skeleton-block();
    flex-shrink: 0;
  }

  .skeleton-input {
    flex: 1;
    height: 38px;
    .skeleton-block();
    max-width: 300px;
  }
}

.page-config {
  min-height: 100vh;
  padding-bottom: var(--spacing-xl);
  background: var(--ui-page-bg);

  .page-title {
    color: var(--ui-text-primary) !important;
    font-weight: var(--ui-label-weight);
  }

  .form.card {
    overflow: visible;
    border: 1px solid var(--ui-border);
    border-radius: 0;
    background: var(--ui-surface);
  }

  .config-page {
    .accordion-item {
      overflow: hidden;
      border: 1px solid var(--ui-border);
      border-radius: 0;
      background: var(--ui-surface);
    }

    .accordion-button {
      border: 0;
      border-radius: 0;
      background: var(--ui-surface-strong);
      color: var(--ui-text-primary);
      font-weight: 600;
      transition: var(--transition-default);

      &:hover,
      &:not(.collapsed) {
        background: var(--ui-surface-hover);
        color: var(--ui-text-primary);
      }

      &:focus-visible {
        outline: 2px solid var(--ui-text-primary);
        outline-offset: 0;
      }
    }

    .accordion-body {
      background: transparent;
    }

    pre {
      max-width: 100%;
      margin: 0.5rem 0;
      padding: 0.75rem 1rem;
      overflow: auto;
      border: 1px solid var(--ui-border);
      border-radius: 0;
      background: var(--ui-surface);
      color: var(--ui-text-secondary);
      font-family: var(--font-family-mono);
      font-size: 0.82rem;
    }

    .pre-line {
      white-space: pre-line;
    }

    .settings-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 0.85rem;
    }

    .settings-field,
    .settings-panel {
      min-width: 0;
      padding: 0.9rem;
      border: 1px solid var(--ui-border);
      border-radius: 0;
      background: var(--ui-surface);
    }

    .settings-panel--accent {
      border-left: 3px solid var(--ui-border-strong);
      background: var(--ui-surface-strong);
    }

    .settings-subpanel {
      padding: 0.8rem;
      border: 1px solid var(--ui-border);
      border-left: 3px solid var(--ui-border-strong);
      border-radius: 0;
      background: var(--ui-surface-strong);
    }

    .settings-toggle-field {
      display: flex;
      flex-direction: column;
      justify-content: center;
    }

    .settings-field > .form-label:first-child,
    .settings-panel > .form-label:first-child {
      color: var(--ui-text-primary);
      font-weight: 600;
    }

    @media (max-width: 767.98px) {
      .settings-grid {
        grid-template-columns: minmax(0, 1fr);
      }

      .settings-field,
      .settings-panel {
        padding: 0.75rem;
      }
    }
  }

  .nav-tabs {
    border: none;
  }

  .ms-item {
    border: 1px solid;
    border-radius: 0;
    font-size: 12px;
    font-weight: 600;
  }

  .config-tabs-shell {
    position: relative;
    z-index: 10;
    background: var(--ui-surface-strong);
  }

  .config-tabs {
    background: var(--ui-surface-strong);
    padding: 0.5rem 1rem 0;
    gap: 0.5rem;
    border-bottom: 1px solid var(--ui-border);
    position: relative;
    z-index: 10;
    overflow: visible;

    .nav-item {
      margin-bottom: -1px;

      &.dropdown {
        position: relative;
        &.show .dropdown-menu {
          display: block;
        }
      }
    }

    .nav-link {
      border: 1px solid transparent;
      border-radius: 0;
      padding: 0.75rem 1.5rem;
      font-weight: var(--ui-label-weight);
      text-transform: var(--ui-label-transform);
      letter-spacing: var(--ui-label-tracking);
      color: var(--ui-text-secondary);
      background: transparent;
      position: relative;
      transition: var(--transition-default);

      &:hover {
        color: var(--ui-text-primary);
        background: var(--ui-surface-hover);
      }

      // The active tab is marked by a solid bar, not by a growing underline.
      &.active {
        color: var(--ui-text-primary);
        background: var(--ui-surface);
        border-color: var(--ui-border);
        border-bottom-color: transparent;
        font-weight: var(--ui-label-weight);

        &::before {
          content: '';
          position: absolute;
          top: 0;
          left: 0;
          right: 0;
          height: 3px;
          background: var(--ui-accent);
        }
      }
    }

    .dropdown-menu {
      display: none;
      position: absolute;
      top: 100%;
      left: 0;
      z-index: 1050;
      min-width: 200px;
      margin-top: 0;
      padding: 0.5rem 0;
      border-radius: 0;
      border: 1px solid var(--ui-border-strong);
      background: var(--ui-surface-strong);

      &.show {
        display: block;
      }

      .dropdown-item {
        display: flex;
        align-items: center;
        padding: 0.5rem 1.5rem;
        font-weight: 500;
        color: var(--ui-text-primary);
        text-decoration: none;
        transition: var(--transition-default);

        &:hover {
          background: var(--ui-surface-hover);
        }

        &.active {
          background: var(--ui-accent);
          color: var(--ui-accent-contrast);
          font-weight: 600;
        }
      }
    }
  }
}

.toast.show {
  opacity: 1;
}

.risk-confirm-intro {
  margin: 0 0 1rem;
  color: var(--ui-text-secondary);
}

.risk-confirm-list {
  display: grid;
  gap: 0.75rem;
}

.risk-item {
  padding: 1rem;
  border: 1px solid var(--ui-border);
  border-radius: 0;
  background: var(--ui-surface);

  &.critical {
    border-color: var(--ui-danger-border);
    background: var(--ui-danger-soft);
  }

  &.high {
    border-color: var(--ui-warning-border);
    background: var(--ui-warning-soft);
  }

  &.medium {
    border-color: var(--ui-border-strong);
    background: var(--ui-surface-strong);
  }

  p {
    margin: 0.5rem 0 0;
    color: var(--ui-text-primary);
    line-height: 1.5;
  }
}

.risk-item-header {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  flex-wrap: wrap;

  strong {
    font-size: 0.98rem;
  }
}

.risk-badge {
  display: inline-flex;
  align-items: center;
  min-height: 24px;
  padding: 0.15rem 0.55rem;
  border-radius: 0;
  font-size: 0.75rem;
  font-weight: var(--ui-label-weight);
  text-transform: var(--ui-label-transform);
  letter-spacing: var(--ui-label-tracking);

  &.critical {
    color: var(--ui-danger-contrast);
    background: var(--ui-danger);
  }

  &.high {
    color: var(--ui-warning-contrast);
    background: var(--ui-warning);
  }

  &.medium {
    color: var(--ui-accent-contrast);
    background: var(--ui-accent);
  }
}

.risk-detail,
.risk-recovery {
  margin-top: 0.7rem;
  padding-top: 0.7rem;
  border-top: 1px solid var(--ui-border);

  span {
    display: block;
    margin-bottom: 0.25rem;
    color: var(--ui-text-secondary);
    font-size: 0.8rem;
    font-weight: 600;
  }

  code {
    display: inline-block;
    max-width: 100%;
    overflow-wrap: anywhere;
    padding: 0.2rem 0.4rem;
    border: 1px solid var(--ui-border);
    border-radius: 0;
    color: var(--ui-text-primary);
    background: var(--ui-surface-strong);
    font-family: var(--font-family-mono);
  }
}

.risk-recovery p {
  margin: 0;
  color: var(--ui-text-secondary);
}

// Sticky save / apply controls. Formerly circular buttons that lifted on
// hover, scaled on press and pulsed while changes were pending.
.config-floating-buttons {
  position: sticky;
  top: 80%;
  right: 2rem;
  float: right;
  clear: right;
  margin: 2rem 0;
  display: flex;
  flex-direction: column;
  gap: 1rem;
  z-index: 1000;

  .floating-toast-container {
    position: absolute;
    right: calc(100% + 1rem);
    top: 0;
    width: max-content;
    max-width: 300px;

    .toast {
      margin-bottom: 0.5rem;
    }
  }

  .tool-btn {
    width: @btn-size;
    height: @btn-size;
    border-radius: 0;
    border: 1px solid var(--ui-border-strong);
    font-size: 1.25rem;
    cursor: pointer;
    position: relative;
    transition: var(--transition-default);
    .flex-center();

    &:focus-visible {
      outline: 2px solid var(--ui-text-primary);
      outline-offset: 0;
    }

    &-primary {
      background: var(--ui-accent);
      color: var(--ui-accent-contrast);

      &:hover {
        background: var(--ui-accent);
      }

      // Pending changes are flagged by the frame color alone.
      &.has-unsaved {
        border-color: var(--ui-warning);
      }
    }

    &-success {
      background: var(--ui-success);
      color: var(--ui-success-contrast);

      &:hover {
        background: var(--ui-success);
      }
    }

    &:disabled {
      cursor: not-allowed;
      opacity: 0.65;
      border-color: var(--ui-border);
      background: var(--ui-surface-strong);
      color: var(--ui-text-muted);
    }
  }
}

// Responsive
@media (max-width: 768px) {
  .config-floating-buttons {
    position: fixed;
    right: 1rem;
    bottom: 1rem;
    top: auto;
    float: none;
    margin: 0;
    gap: 0.75rem;

    .floating-toast-container {
      right: auto;
      left: auto;
      top: auto;
      bottom: calc(100% + 1rem);
      max-width: calc(100vw - 2rem);
    }

    .tool-btn {
      width: @btn-size-mobile;
      height: @btn-size-mobile;
      font-size: 1.1rem;
    }
  }

  .page-config .config-tabs {
    padding: 0.5rem 0.5rem 0;
    gap: 0.35rem;
    overflow-x: auto;
    overflow-y: hidden;
    flex-wrap: nowrap;
    scroll-padding-inline: 2rem;
    scroll-snap-type: x proximity;
    scrollbar-width: thin;
    -webkit-overflow-scrolling: touch;

    .nav-link {
      min-height: 44px;
      padding: 0.625rem 1rem;
      font-size: 0.875rem;
      white-space: nowrap;
      scroll-snap-align: center;
    }
  }
}
</style>
