<template>
  <Transition name="fade">
    <div v-if="show" class="diagnosis-overlay" @click.self="$emit('close')">
      <div class="diagnosis-modal">
        <div class="diagnosis-header">
          <h5>
            <i class="fas fa-robot me-2"></i>{{ $t('troubleshooting.ai_diagnosis_title') }}
          </h5>
          <button class="btn-close" :aria-label="$t('close')" @click="$emit('close')"></button>
        </div>

        <div class="diagnosis-body">
          <!-- Config Section (collapsible) -->
          <div class="config-section mb-3">
            <button
              class="btn btn-sm btn-outline-secondary w-100 text-start"
              @click="showConfig = !showConfig"
            >
              <i class="fas fa-cog me-1"></i>
              {{ $t('troubleshooting.ai_config') }}
              <i class="fas ms-1" :class="showConfig ? 'fa-chevron-up' : 'fa-chevron-down'"></i>
            </button>

            <div v-if="showConfig" class="config-form mt-2">
              <div class="alert alert-info py-2 mb-2">
                <i class="fas fa-circle-info me-1"></i>
                {{ diagnosisText('managedConfig') }}
              </div>
              <div class="row g-2">
                <div class="col-md-3">
                  <label class="form-label form-label-sm">{{ diagnosisText('status') }}</label>
                  <div class="form-control form-control-sm bg-body-secondary">
                    {{ config?.enabled ? diagnosisText('enabled') : diagnosisText('disabled') }}
                  </div>
                </div>
                <div class="col-md-3">
                  <label class="form-label form-label-sm">{{ $t('troubleshooting.ai_provider') }}</label>
                  <div class="form-control form-control-sm bg-body-secondary">{{ config?.provider || '-' }}</div>
                </div>
                <div class="col-md-3">
                  <label class="form-label form-label-sm">{{ diagnosisText('compatibility') }}</label>
                  <div class="form-control form-control-sm bg-body-secondary">{{ config?.compatibility || '-' }}</div>
                </div>
                <div class="col-md-3">
                  <label class="form-label form-label-sm">{{ $t('troubleshooting.ai_model') }}</label>
                  <div class="form-control form-control-sm bg-body-secondary">{{ config?.model || '-' }}</div>
                </div>
              </div>
              <div class="text-muted small mt-2">
                <i class="fas fa-lock me-1"></i>
                {{ diagnosisText('keysLocal') }}
                <span v-if="isConfigLoading || isSavingConfig" class="ms-1" role="status" aria-live="polite">
                  <span class="spinner-border spinner-border-sm"></span>
                  <span class="visually-hidden">{{ loadingStatusText }}</span>
                </span>
              </div>
            </div>
          </div>

          <div v-if="localFindings?.length" class="local-diagnostics mb-3">
            <div class="local-diagnostics-header">
              <i class="fas fa-magnifying-glass-chart me-1"></i>
              <strong>{{ diagnosisText('localPreDiagnosis') }}</strong>
            </div>
            <div class="local-finding-list">
              <div
                v-for="finding in localFindings"
                :key="finding.id"
                class="local-finding"
                :class="`local-finding--${finding.severity || 'info'}`"
              >
                <div class="local-finding-title">
                  {{ getFindingLabel(finding) }}
                  <span v-if="finding.count > 1" class="badge bg-secondary ms-1">x{{ finding.count }}</span>
                </div>
                <code v-if="finding.evidence?.[0]?.text" class="local-finding-evidence">
                  {{ finding.evidence[0].text }}
                </code>
              </div>
            </div>
          </div>

          <div v-if="localSuggestions?.length" class="local-suggestions mb-3">
            <div class="local-diagnostics-header">
              <i class="fas fa-screwdriver-wrench me-1"></i>
              <strong>{{ diagnosisText('suggestedFixes') }}</strong>
            </div>
            <div class="local-suggestion-list">
              <div
                v-for="suggestion in localSuggestions"
                :key="suggestion.id"
                class="local-suggestion"
                :class="`local-suggestion--${suggestion.severity || 'info'}`"
              >
                <div class="local-finding-title">{{ getSuggestionTitle(suggestion) }}</div>
                <ul class="local-suggestion-actions">
                  <li v-for="(action, index) in getSuggestionActions(suggestion)" :key="`${index}-${action}`">{{ action }}</li>
                </ul>
              </div>
            </div>
          </div>

          <!-- Diagnose Button -->
          <div class="text-center mb-3" v-if="!result && !error">
            <button class="btn btn-primary" :disabled="isLoading" @click="$emit('diagnose')">
              <span v-if="isLoading">
                <span class="spinner-border spinner-border-sm me-1"></span>
                {{ $t('troubleshooting.ai_analyzing') }}
              </span>
              <span v-else>
                <i class="fas fa-search-plus me-1"></i>
                {{ $t('troubleshooting.ai_start_diagnosis') }}
              </span>
            </button>
          </div>

          <!-- Loading -->
          <div v-if="isLoading && !result" class="text-center text-muted py-4">
            <div class="spinner-border text-primary mb-2"></div>
            <p>{{ $t('troubleshooting.ai_analyzing_logs') }}</p>
          </div>

          <!-- Error -->
          <div v-if="error" class="alert alert-danger d-flex align-items-start">
            <i class="fas fa-exclamation-circle me-2 mt-1"></i>
            <div>
              <strong>{{ $t('troubleshooting.ai_error') }}</strong>
              <p class="mb-1">{{ error }}</p>
              <button class="btn btn-sm btn-outline-danger" @click="$emit('diagnose')">
                <i class="fas fa-redo me-1"></i>{{ $t('troubleshooting.ai_retry') }}
              </button>
            </div>
          </div>

          <!-- Result -->
          <div v-if="result" class="diagnosis-result">
            <div class="result-header mb-2">
              <i class="fas fa-clipboard-check text-success me-1"></i>
              <strong>{{ $t('troubleshooting.ai_result') }}</strong>
              <button class="btn btn-sm btn-outline-secondary ms-auto" @click="copyResult">
                <i class="fas fa-copy me-1"></i>{{ $t('troubleshooting.ai_copy_result') }}
              </button>
            </div>
            <div class="result-content" v-html="renderMarkdown(result)"></div>
            <div class="text-center mt-3">
              <button class="btn btn-sm btn-outline-primary" @click="$emit('diagnose')">
                <i class="fas fa-redo me-1"></i>{{ $t('troubleshooting.ai_reanalyze') }}
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  </Transition>
</template>

<script setup>
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'

const { locale } = useI18n()

const props = defineProps({
  show: Boolean,
  config: Object,
  isConfigLoading: Boolean,
  isSavingConfig: Boolean,
  isLoading: Boolean,
  result: String,
  error: String,
  localFindings: {
    type: Array,
    default: () => [],
  },
  localSuggestions: {
    type: Array,
    default: () => [],
  },
})

defineEmits(['close', 'diagnose'])

const showConfig = ref(false)

const DIAGNOSIS_TEXT = {
  managedConfig: 'AI configuration is managed elsewhere. This dialog only uses the shared Sunshine AI proxy.',
  status: 'Status',
  enabled: 'Enabled',
  disabled: 'Disabled',
  compatibility: 'Compatibility',
  keysLocal: "API keys stay in Sunshine's local config and are not edited here.",
  localPreDiagnosis: 'Local pre-diagnosis',
  suggestedFixes: 'Suggested fixes',
  loadingConfig: 'Loading AI configuration...',
  savingConfig: 'Saving AI configuration...',
}

function diagnosisText(key) {
  return DIAGNOSIS_TEXT[key] || key
}

const loadingStatusText = computed(() => (
  props.isSavingConfig ? diagnosisText('savingConfig') : diagnosisText('loadingConfig')
))

function copyResult() {
  const el = document.querySelector('.result-content')
  if (el) {
    navigator.clipboard.writeText(el.innerText)
  }
}

function getFindingLabel(finding) {
  return finding?.message || finding?.type || ''
}

function getSuggestionTitle(suggestion) {
  return suggestion?.title || suggestion?.findingType || ''
}

function getSuggestionActions(suggestion) {
  return Array.isArray(suggestion?.actions) ? suggestion.actions : []
}

function renderMarkdown(text) {
  if (!text) return ''
  return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/```([\s\S]*?)```/g, '<pre class="code-block">$1</pre>')
    .replace(/`([^`]+)`/g, '<code>$1</code>')
    .replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>')
    .replace(/^\s*[-*]\s+(.+)$/gm, '<li>$1</li>')
    .replace(/(<li>.*<\/li>)/s, '<ul>$1</ul>')
    .replace(/^### (.+)$/gm, '<h6 class="mt-3 mb-1">$1</h6>')
    .replace(/^## (.+)$/gm, '<h5 class="mt-3 mb-1">$1</h5>')
    .replace(/^# (.+)$/gm, '<h4 class="mt-3 mb-1">$1</h4>')
    .replace(/\n/g, '<br>')
}
</script>

<style scoped>
.diagnosis-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  padding: 1rem;
  background: var(--ui-overlay);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1050;
}

.diagnosis-modal {
  background: var(--ui-surface-strong);
  border: 1px solid var(--ui-border);
  border-radius: 0;
  width: 100%;
  max-width: 700px;
  max-height: 80vh;
  display: flex;
  flex-direction: column;
  color: var(--ui-text-primary);
}

.diagnosis-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 1rem 1.5rem;
  border-bottom: 1px solid var(--ui-border);
}

.diagnosis-header h5 {
  margin: 0;
  font-weight: 600;
}

.diagnosis-body {
  padding: 1.5rem;
  overflow-y: auto;
  flex: 1;
}

.config-form {
  background: var(--ui-surface);
  border: 1px solid var(--ui-border);
  border-radius: 0;
  padding: 1rem;
}

.local-diagnostics {
  border: 1px solid var(--ui-border);
  border-radius: 0;
  padding: 0.75rem;
  background: var(--ui-surface);
}

.local-diagnostics-header {
  display: flex;
  align-items: center;
  margin-bottom: 0.5rem;
}

.local-finding-list {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.local-suggestion-list {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.local-finding {
  border-left: 4px solid var(--ui-accent);
  border-radius: 0;
  padding: 0.5rem 0.65rem;
  background: var(--ui-surface-strong);
}

.local-suggestions {
  border: 1px solid var(--ui-border);
  border-radius: 0;
  padding: 0.75rem;
  background: var(--ui-surface);
}

.local-suggestion {
  border-left: 4px solid var(--ui-accent);
  border-radius: 0;
  padding: 0.5rem 0.65rem;
  background: var(--ui-surface-strong);
}

.local-finding--error,
.local-finding--fatal,
.local-suggestion--error,
.local-suggestion--fatal {
  border-left-color: var(--ui-danger);
}

.local-finding--warning,
.local-suggestion--warning {
  border-left-color: var(--ui-warning);
}

.local-finding-title {
  font-weight: 600;
  line-height: 1.35;
}

.local-finding-evidence {
  display: block;
  margin-top: 0.35rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--ui-text-secondary);
  background: var(--ui-accent-soft);
  padding: 0.25rem 0.4rem;
  border-radius: 0;
}

.local-suggestion-actions {
  margin: 0.4rem 0 0;
  padding-left: 1.25rem;
  color: var(--ui-text-secondary);
}

.local-suggestion-actions li {
  margin-bottom: 0.25rem;
}

.result-header {
  display: flex;
  align-items: center;
}

.result-content {
  background: var(--ui-surface);
  border: 1px solid var(--ui-border);
  border-radius: 0;
  padding: 1rem 1.25rem;
  font-size: 0.9rem;
  line-height: 1.6;
  max-height: 400px;
  overflow-y: auto;
}

.result-content :deep(code) {
  background: var(--ui-accent-soft);
  color: var(--ui-text-primary);
  padding: 0.15em 0.4em;
  border-radius: 0;
  font-size: 0.85em;
}

.result-content :deep(.code-block) {
  background: var(--ui-surface-strong);
  color: var(--ui-text-primary);
  font-family: var(--font-family-mono);
  padding: 0.75rem 1rem;
  border-radius: 0;
  font-size: 0.8rem;
  overflow-x: auto;
}

.result-content :deep(ul) {
  padding-left: 1.5rem;
  margin: 0.5rem 0;
}

.result-content :deep(li) {
  margin-bottom: 0.25rem;
}

.fade-enter-active,
.fade-leave-active {
  transition: var(--transition-default);
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}
</style>
