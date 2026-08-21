<script setup>
import { nextTick, onBeforeUnmount, ref, watch } from 'vue'
import { getFocusableElements } from '../../utils/focus.js'

const props = defineProps({
  show: { type: Boolean, default: false },
  dialogId: { type: String, required: true },
  title: { type: String, required: true },
  titleIcon: { type: String, default: '' },
  tone: { type: String, default: 'accent' },
  dismissible: { type: Boolean, default: true },
  closeLabel: { type: String, default: 'Close' },
  maxWidth: { type: String, default: '500px' },
})

const emit = defineEmits(['close'])
const dialog = ref(null)
let previousFocus = null
let previousBodyOverflow = null

const setBodyScrollLock = (locked) => {
  if (locked) {
    if (previousBodyOverflow === null) previousBodyOverflow = document.body.style.overflow
    document.body.style.overflow = 'hidden'
    return
  }

  if (previousBodyOverflow !== null) {
    document.body.style.overflow = previousBodyOverflow
    previousBodyOverflow = null
  }
}

const close = () => {
  if (props.dismissible) emit('close')
}

const handleKeydown = (event) => {
  if (event.key === 'Escape') {
    event.preventDefault()
    close()
    return
  }
  if (event.key !== 'Tab') return

  const focusable = getFocusableElements(dialog.value)

  if (!focusable.length) {
    event.preventDefault()
    dialog.value?.focus()
    return
  }

  const first = focusable[0]
  const last = focusable[focusable.length - 1]
  const activeIndex = focusable.indexOf(document.activeElement)
  if (event.shiftKey && activeIndex <= 0) {
    event.preventDefault()
    last.focus()
  } else if (!event.shiftKey && (activeIndex === -1 || document.activeElement === last)) {
    event.preventDefault()
    first.focus()
  }
}

watch(
  () => props.show,
  async (show) => {
    if (show) {
      setBodyScrollLock(true)
      previousFocus = document.activeElement
      await nextTick()
      dialog.value?.focus()
    } else {
      setBodyScrollLock(false)
      if (previousFocus instanceof HTMLElement) {
        previousFocus.focus()
        previousFocus = null
      }
    }
  },
  { immediate: true },
)

onBeforeUnmount(() => {
  setBodyScrollLock(false)
  if (previousFocus instanceof HTMLElement) previousFocus.focus()
})
</script>

<template>
  <Teleport to="body">
    <Transition name="confirm-dialog">
      <div v-if="show" class="confirm-dialog-overlay" @click.self="close">
        <section
          ref="dialog"
          class="confirm-dialog-panel"
          :class="`confirm-dialog-panel--${tone}`"
          :style="{ '--confirm-dialog-max-width': maxWidth }"
          role="dialog"
          aria-modal="true"
          :aria-labelledby="`${dialogId}-title`"
          tabindex="-1"
          @keydown="handleKeydown"
        >
          <header class="confirm-dialog-header">
            <h5 :id="`${dialogId}-title`">
              <i v-if="titleIcon" :class="titleIcon" aria-hidden="true"></i>
              {{ title }}
            </h5>
            <button
              v-if="dismissible"
              type="button"
              class="confirm-dialog-close"
              :aria-label="closeLabel"
              @click="close"
            >
              <i class="fas fa-times" aria-hidden="true"></i>
            </button>
          </header>
          <div class="confirm-dialog-body">
            <slot />
          </div>
          <footer v-if="$slots.actions" class="confirm-dialog-footer">
            <slot name="actions" />
          </footer>
        </section>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
.confirm-dialog-overlay {
  position: fixed;
  inset: 0;
  z-index: 9999;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: var(--spacing-lg, 1.5rem);
  overflow: hidden;
  background: var(--ui-overlay);
}

/*
 * A dialog is an overlay, so it is the one place in this component tree that
 * earns the deepest elevation step. It still sits on a panel fill with a
 * hairline border - the shadow lifts it off the scrim, it does not decorate it.
 */
.confirm-dialog-panel {
  width: min(var(--confirm-dialog-max-width), 100%);
  max-height: calc(100vh - 2.5rem);
  display: flex;
  flex-direction: column;
  overflow: hidden;
  border: 1px solid var(--ui-border);
  border-radius: var(--ui-radius);
  outline: none;
  background: var(--ui-panel);
  color: var(--ui-text-primary);
  box-shadow: var(--ui-shadow-md);
}

.confirm-dialog-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--ui-space-3);
  padding: var(--ui-space-3) var(--ui-space-4);
  border-bottom: 1px solid var(--ui-border);
}

.confirm-dialog-header h5 {
  display: flex;
  align-items: center;
  gap: var(--ui-space-2);
  margin: 0;
  color: var(--ui-text-primary);
  font-size: var(--font-size-md);
  font-weight: var(--ui-label-weight);
}

.confirm-dialog-header h5 i {
  color: var(--ui-text-muted);
}

.confirm-dialog-panel--danger .confirm-dialog-header h5 i {
  color: var(--ui-danger);
}

.confirm-dialog-panel--warning .confirm-dialog-header h5 i {
  color: var(--ui-warning);
}

.confirm-dialog-close {
  width: var(--ui-control-height);
  height: var(--ui-control-height);
  display: inline-flex;
  flex: 0 0 auto;
  align-items: center;
  justify-content: center;
  padding: 0;
  border: 0;
  border-radius: var(--ui-radius);
  background: transparent;
  color: var(--ui-text-muted);
  cursor: pointer;
  transition: var(--transition-default);
}

.confirm-dialog-close:hover,
.confirm-dialog-close:focus-visible {
  background: var(--ui-surface-hover);
  color: var(--ui-text-primary);
  outline: none;
}

.confirm-dialog-close:focus-visible {
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 0;
}

.confirm-dialog-body {
  min-height: 0;
  flex: 1;
  overflow-y: auto;
  padding: var(--ui-space-4);
  color: var(--ui-text-secondary);
  line-height: 1.6;
}

.confirm-dialog-body :deep(p:last-child) {
  margin-bottom: 0;
}

.confirm-dialog-footer {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: var(--ui-space-2);
  padding: var(--ui-space-3) var(--ui-space-4);
  border-top: 1px solid var(--ui-border);
  background: var(--ui-surface);
}

.confirm-dialog-enter-active,
.confirm-dialog-leave-active {
  transition: var(--transition-default);
}

.confirm-dialog-enter-active .confirm-dialog-panel,
.confirm-dialog-leave-active .confirm-dialog-panel {
  transition: var(--transition-default);
}

.confirm-dialog-enter-from,
.confirm-dialog-leave-to {
  opacity: 0;
}

.confirm-dialog-enter-from .confirm-dialog-panel,
.confirm-dialog-leave-to .confirm-dialog-panel {
  opacity: 0;
}

@media (max-width: 575.98px) {
  .confirm-dialog-overlay {
    align-items: flex-end;
    padding: 0.75rem;
  }

  .confirm-dialog-panel {
    max-height: calc(100vh - 1.5rem);
    border-radius: var(--ui-radius);
  }

  .confirm-dialog-header,
  .confirm-dialog-body,
  .confirm-dialog-footer {
    padding: var(--ui-space-3);
  }

  .confirm-dialog-footer {
    flex-direction: column-reverse;
  }

  .confirm-dialog-footer :deep(.btn) {
    width: 100%;
  }
}
</style>
