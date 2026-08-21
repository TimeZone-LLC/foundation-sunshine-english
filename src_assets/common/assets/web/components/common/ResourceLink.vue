<script setup>
import { computed } from 'vue'

const props = defineProps({
  href: { type: String, required: true },
  title: { type: String, required: true },
  description: { type: String, default: '' },
  icon: { type: String, default: '' },
  imageSrc: { type: String, default: '' },
  imageAlt: { type: String, default: '' },
  variant: { type: String, default: 'accent' },
  arrowIcon: { type: String, default: 'fas fa-external-link-alt' },
  arrowClass: { type: String, default: '' },
  compact: { type: Boolean, default: false },
  target: { type: String, default: '_blank' },
  action: { type: Boolean, default: false },
})

defineEmits(['activate'])

const classes = computed(() => [
  `resource-link--${props.variant}`,
  { 'resource-link--compact': props.compact },
])

const rel = computed(() => (props.target === '_blank' ? 'noopener noreferrer' : undefined))
</script>

<template>
  <component
    :is="action ? 'button' : 'a'"
    class="resource-link"
    :class="classes"
    :href="action ? undefined : href"
    :type="action ? 'button' : undefined"
    :target="action ? undefined : target || undefined"
    :rel="action ? undefined : rel"
    :aria-haspopup="action ? 'dialog' : undefined"
    @click="$emit('activate', $event)"
  >
    <span class="resource-icon" :class="{ 'resource-icon--logo': imageSrc }">
      <img v-if="imageSrc" class="resource-logo" :src="imageSrc" :alt="imageAlt" />
      <i v-else :class="icon" aria-hidden="true"></i>
    </span>
    <span class="resource-content">
      <span class="resource-title">{{ title }}</span>
      <span v-if="description" class="resource-desc">{{ description }}</span>
    </span>
    <i class="resource-arrow" :class="[arrowIcon, arrowClass]" aria-hidden="true"></i>
  </component>
</template>

<style scoped>
.resource-link {
  --link-color: var(--ui-accent-rgb);
  --link-foreground: var(--ui-accent);
  display: flex;
  align-items: center;
  padding: 0.6em 0.8em;
  border: 1px solid var(--ui-border);
  border-radius: 0;
  background: var(--ui-surface-strong);
  color: var(--ui-text-primary);
  isolation: isolate;
  position: relative;
  text-decoration: none;
  font: inherit;
  text-align: left;
  cursor: pointer;
  appearance: none;
  transition: var(--transition-default);
}

.resource-link:hover,
.resource-link:focus-visible {
  border-color: var(--ui-border-strong);
  background: var(--ui-surface-hover);
  color: var(--ui-text-primary);
  text-decoration: none;
}

.resource-link:focus-visible {
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 0;
}

.resource-link--compact {
  min-height: 3.3rem;
  padding: 0.45rem 0.6rem;
}

.resource-icon {
  width: 36px;
  height: 36px;
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  justify-content: center;
  margin-right: 0.8em;
  border-radius: 0;
  border: 1px solid var(--ui-border);
  background: var(--ui-surface-hover);
  color: var(--link-foreground);
  font-size: 1.1rem;
}

.resource-link--compact .resource-icon {
  width: 32px;
  height: 32px;
  margin-right: 0.45rem;
  border-radius: 0;
  font-size: 0.76rem;
}

/*
 * A white plate, not a palette choice: the partner logos are dark-on-transparent
 * PNGs supplied by their owners and are unreadable on the dark ground. The plate
 * is scoped to those images only.
 */
.resource-icon--logo {
  width: 86px;
  height: 44px;
  padding: 4px;
  overflow: visible;
  background: #fff;
}

.resource-logo {
  width: auto;
  max-width: 100%;
  height: auto;
  max-height: 100%;
  object-fit: contain;
}

.resource-content {
  min-width: 0;
  flex: 1;
}

.resource-title,
.resource-desc {
  display: block;
}

.resource-title {
  margin-bottom: 1px;
  color: var(--ui-text-primary);
  font-size: 0.9rem;
  font-weight: 600;
}

.resource-desc {
  color: var(--ui-text-secondary);
  font-size: 0.75rem;
}

.resource-link--compact .resource-title {
  margin-bottom: 1px;
  font-size: var(--font-size-sm);
}

.resource-link--compact .resource-desc {
  overflow: hidden;
  font-size: var(--font-size-xs);
  line-height: 1.25;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.resource-arrow {
  margin-left: 0.5rem;
  color: var(--ui-text-muted);
  font-size: 0.8rem;
  transition: var(--transition-default);
}

.resource-link--accent-alt {
  --link-color: var(--ui-info-rgb);
  --link-foreground: var(--ui-info-text);
}

.resource-link--android {
  --link-color: var(--ui-success-rgb);
  --link-foreground: var(--ui-success-text);
}

.resource-link--apple {
  --link-color: 100, 116, 139;
  --link-foreground: var(--ui-text-secondary);
}

.resource-link--desktop,
.resource-link--github {
  --link-color: 100, 116, 139;
  --link-foreground: var(--ui-text-secondary);
}

.resource-link--harmony {
  --link-color: var(--ui-info-rgb);
  --link-foreground: var(--ui-info-text);
}

.resource-link--moonlink {
  --link-color: 142, 126, 173;
  --link-foreground: var(--ui-text-secondary);
}

[data-bs-theme='dark'] .resource-link--apple {
  --link-color: 184, 171, 154;
}

[data-bs-theme='dark'] .resource-link--desktop,
[data-bs-theme='dark'] .resource-link--github {
  --link-color: 184, 171, 154;
}

@media (max-width: 575.98px) {
  .resource-link--compact .resource-desc {
    white-space: normal;
  }
}
</style>
