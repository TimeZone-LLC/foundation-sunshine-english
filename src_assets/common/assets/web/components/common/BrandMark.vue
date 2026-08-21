<template>
  <span class="brand-mark" :class="{ 'brand-mark-stacked': stacked }">
    <!--
      Inline copy of public/images/logo-sunshine.svg. It is inlined rather than
      referenced with <img> for two reasons: currentColor only resolves for
      inline SVG, and the config server exposes no catch-all route under
      /images, so only explicitly registered files are reachable over HTTP.
    -->
    <svg
      class="brand-mark-glyph"
      xmlns="http://www.w3.org/2000/svg"
      viewBox="0 0 24 24"
      :width="size"
      :height="size"
      fill="currentColor"
      aria-hidden="true"
      focusable="false"
    >
      <rect x="8" y="8" width="8" height="8" />
      <rect x="10" y="1" width="4" height="4" />
      <rect x="10" y="19" width="4" height="4" />
      <rect x="1" y="10" width="4" height="4" />
      <rect x="19" y="10" width="4" height="4" />
    </svg>
    <span class="brand-mark-word">Sunshine</span>
  </span>
</template>

<script setup>
defineProps({
  // Edge length of the square glyph, in CSS pixels.
  size: {
    type: Number,
    default: 22,
  },
  // Stack the wordmark under the glyph instead of setting it beside.
  stacked: {
    type: Boolean,
    default: false,
  },
})
</script>

<style scoped>
.brand-mark {
  display: inline-flex;
  align-items: center;
  gap: 0.6rem;
  color: inherit;
}

.brand-mark-stacked {
  flex-direction: column;
  gap: 0.5rem;
}

.brand-mark-glyph {
  display: block;
  flex: none;
}

/*
 * The wordmark is plain text, not an image, so it scales with the page and
 * inherits the theme foreground. Case and tracking come from the label tokens
 * (sentence case, no tracking) rather than being set here.
 */
.brand-mark-word {
  font-family: var(--font-family-sans);
  font-size: 1rem;
  font-weight: var(--ui-label-weight);
  text-transform: var(--ui-label-transform);
  letter-spacing: var(--ui-label-tracking);
  line-height: 1;
  white-space: nowrap;
}

.brand-mark-stacked .brand-mark-word {
  font-size: 1.2rem;
}
</style>
