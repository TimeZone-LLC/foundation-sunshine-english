<template>
  <aside v-if="fatalLogs.length" class="startup-alert" role="alert">
    <span class="startup-alert-icon" aria-hidden="true">
      <i class="fas fa-triangle-exclamation"></i>
    </span>

    <div class="startup-alert-content">
      <p class="startup-alert-heading" v-html="$t('index.startup_errors')"></p>
      <ul class="startup-alert-list">
        <li v-for="log in fatalLogs" :key="log.timestamp" class="startup-alert-item">
          <span class="startup-alert-marker" aria-hidden="true"></span>
          <span>{{ log.value }}</span>
        </li>
      </ul>
    </div>

    <a class="startup-alert-action" href="/troubleshooting/#logs">
      <i class="fas fa-list-ul" aria-hidden="true"></i>
      <span>{{ $t('index.view_logs') }}</span>
      <i class="fas fa-chevron-right startup-alert-arrow" aria-hidden="true"></i>
    </a>
  </aside>
</template>

<script setup>
defineProps({
  fatalLogs: {
    type: Array,
    required: true
  }
})
</script>

<style scoped lang="less">
.startup-alert {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto;
  gap: 0.72rem;
  align-items: start;
  margin-bottom: 0.9rem;
  padding: 0.78rem 0.85rem;
  border: 1px solid color-mix(in srgb, var(--ui-danger-border) 72%, var(--ui-border));
  border-radius: 0;
  background: var(--ui-surface-strong);
  color: var(--ui-text-primary);
}

.startup-alert-icon {
  display: inline-flex;
  width: var(--ui-control-height);
  height: var(--ui-control-height);
  align-items: center;
  justify-content: center;
  border: 1px solid var(--ui-danger-border);
  border-radius: 0;
  background: var(--ui-danger-soft);
  color: var(--ui-danger-text);
  font-size: var(--font-size-sm);
}

.startup-alert-content {
  min-width: 0;
}

.startup-alert-heading {
  margin: 0.05rem 0 0;
  color: var(--ui-text-primary);
  font-size: var(--font-size-sm);
  line-height: 1.45;
}

.startup-alert-heading :deep(b),
.startup-alert-heading :deep(strong) {
  color: var(--ui-danger-text);
  font-weight: 700;
}

.startup-alert-list {
  display: grid;
  gap: 0.28rem;
  margin: 0.5rem 0 0;
  padding: 0;
  list-style: none;
}

.startup-alert-item {
  display: grid;
  min-width: 0;
  grid-template-columns: 0.42rem minmax(0, 1fr);
  gap: 0.5rem;
  align-items: baseline;
  padding: 0.38rem 0.5rem;
  border-radius: 0;
  background: var(--ui-surface-strong);
  color: var(--ui-text-secondary);
  font-size: var(--font-size-xs);
  line-height: 1.4;
  overflow-wrap: anywhere;
}

.startup-alert-marker {
  width: 0.34rem;
  height: 0.34rem;
  border-radius: 0;
  background: var(--ui-danger-text);
}

.startup-alert-action {
  display: inline-flex;
  min-height: var(--ui-control-height);
  align-items: center;
  justify-content: center;
  gap: 0.42rem;
  padding: 0.42rem 0.62rem;
  border: 1px solid var(--ui-danger-border);
  border-radius: 0;
  background: var(--ui-surface-strong);
  color: var(--ui-danger-text);
  font-size: var(--font-size-xs);
  font-weight: var(--ui-label-weight);
  text-decoration: none;
  white-space: nowrap;
  transition: var(--transition-default);
}

.startup-alert-action:hover,
.startup-alert-action:focus-visible {
  border-color: var(--ui-danger-text);
  background: var(--ui-danger-soft);
  color: var(--ui-danger-text);
}

.startup-alert-action:focus-visible {
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 0;
}

.startup-alert-arrow {
  font-size: 0.56rem;
  opacity: 0.72;
  transition: var(--transition-default);
}

@media (max-width: 767.98px) {
  .startup-alert {
    grid-template-columns: auto minmax(0, 1fr);
    padding: 0.72rem;
  }

  .startup-alert-action {
    grid-column: 2;
    justify-self: start;
  }
}
</style>

