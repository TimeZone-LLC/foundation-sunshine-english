<template>
  <nav
    class="navbar navbar-light navbar-expand-md navbar-background header"
    :class="{ 'navbar-embedded': isEmbeddedGui }"
  >
    <div class="container-fluid">
      <a class="navbar-brand brand-enhanced" href="/" title="Sunshine">
        <BrandMark :size="24" />
      </a>
      <button
        class="navbar-toggler"
        type="button"
        data-bs-toggle="collapse"
        data-bs-target="#navbarSupportedContent"
        aria-controls="navbarSupportedContent"
        aria-expanded="false"
        aria-label="Toggle navigation"
      >
        <span class="navbar-toggler-icon"></span>
      </button>
      <div class="collapse navbar-collapse" id="navbarSupportedContent">
        <ul class="navbar-nav me-auto mb-2 mb-md-0">
          <li v-for="item in navItems" :key="item.path" class="nav-item">
            <a
              class="nav-link"
              :class="{ active: isActive(item.path) }"
              :href="item.path"
              :aria-current="isActive(item.path) ? 'page' : undefined"
            >
              <i :class="['fas', 'fa-fw', item.icon]" aria-hidden="true"></i> {{ $t(item.label) }}
            </a>
          </li>
        </ul>
        <div class="navbar-utilities ms-md-2">
          <AccountMenu />
        </div>
      </div>
    </div>
  </nav>
</template>

<script setup>
import { onMounted, onUnmounted, ref } from 'vue'
import AccountMenu from '../common/AccountMenu.vue'
import BrandMark from '../common/BrandMark.vue'
import { useBackground } from '../../composables/useBackground.js'
import { useTheme } from '../../composables/useTheme.js'

/*
 * Applies the stored/preferred theme on load by writing data-bs-theme, which is
 * what the whole palette keys off. This used to happen inside ThemeToggle's
 * onMounted; removing that control also removed the initialisation, which left
 * every page rendering in Bootstrap's default light theme.
 */
useTheme()

// 导航项配置
const navItems = Object.freeze([
  { path: '/', icon: 'fa-home', label: 'navbar.home' },
  { path: '/pin', icon: 'fa-qrcode', label: 'navbar.pin' },
  { path: '/apps', icon: 'fa-stream', label: 'navbar.applications' },
  { path: '/config', icon: 'fa-cog', label: 'navbar.configuration' },
])

// 使用背景管理 composable
const { loadBackground, addDragListeners } = useBackground()
const isEmbeddedGui = window.isTauri === true && window.parent !== window

if (isEmbeddedGui) {
  document.documentElement.dataset.sunshineGui = 'true'
}

// 当前路径（响应式）
const currentPath = ref(window.location.pathname)

// 检查路径是否激活
const isActive = (path) => {
  const current = currentPath.value
  if (path === '/') {
    return current === '/' || current === '/index.html'
  }
  const normalizedPath = path.replace(/\.html$/, '')
  return current === normalizedPath || current.startsWith(normalizedPath)
}

// 更新当前路径
const updateCurrentPath = () => {
  currentPath.value = window.location.pathname
}

// 清理函数引用
let removeDragListeners = null

// 链接点击处理函数
const handleLinkClick = (e) => {
  if (e.target.closest('a.nav-link')?.href) {
    setTimeout(updateCurrentPath, 0)
  }
}

// 错误处理函数
const handleBackgroundError = (error) => {
  console.error('Background error:', error)
}

onMounted(async () => {
  await loadBackground()
  updateCurrentPath()
  removeDragListeners = addDragListeners(handleBackgroundError)
  window.addEventListener('popstate', updateCurrentPath)
  document.addEventListener('click', handleLinkClick)
})

onUnmounted(() => {
  window.removeEventListener('popstate', updateCurrentPath)
  document.removeEventListener('click', handleLinkClick)
  removeDragListeners?.()
})
</script>

<style scoped>
.navbar-background {
  position: relative;
  z-index: 1030;
  /* Opaque bar; the hairline bottom border is the only separator. */
  background-color: var(--ui-nav-bg);
  border-bottom: 1px solid var(--ui-border);
}

.navbar-background.navbar-embedded {
  position: fixed;
  inset: 0 0 auto;
  z-index: 1030;
  margin-bottom: 0;
}

/*
 * No right-hand reservation: the shell uses the native Windows title bar, so
 * there is no longer a custom window-control cluster overlaid on this bar to
 * clear. The old 9rem is what left the void at the top right.
 */

.brand-enhanced {
  display: inline-flex;
  align-items: center;
  /*
   * The embedded shell offsets the page body by the bar height, so the bar and
   * that offset are driven by one token (--ui-navbar-height) and cannot drift.
   * This used to be min-height: 50px, sized for a mascot logo that no longer
   * exists, which is what made the bar 77px tall and left a void beside the
   * window controls.
   */
  min-height: var(--ui-control-height);
  color: var(--ui-text-primary);
  transition: var(--transition-default);
}

.brand-enhanced:hover,
.brand-enhanced:focus-visible {
  color: var(--ui-text-primary);
}

.navbar-utilities {
  display: flex;
  align-items: center;
  gap: 0.25rem;
}
</style>

<style>
html[data-sunshine-gui='true'] body {
  padding-top: var(--ui-navbar-height) !important;
}

.header .navbar-utilities .nav-link,
.header .navbar-utilities .nav-utility-button {
  color: var(--ui-text-secondary) !important;
  border: 0;
  border-radius: 0;
  transition: var(--transition-default);
}

.header {
  view-transition-name: sunshine-navbar;
}

.header .navbar-utilities .nav-link:hover,
.header .navbar-utilities .nav-link:focus,
.header .navbar-utilities .nav-utility-button:hover,
.header .navbar-utilities .nav-utility-button:focus {
  color: var(--ui-text-primary) !important;
  background-color: var(--ui-surface-hover);
}

.header .navbar-utilities .nav-utility-button {
  padding: 0.5rem 0.75rem;
}

.header .navbar-utilities .dropdown-menu {
  min-width: 13rem;
  z-index: 1060;
}

.header .navbar-nav > .nav-item > .nav-link {
  position: relative;
  background: transparent;
  color: var(--ui-text-secondary) !important;
  border-radius: 0;
  transition: var(--transition-default);
}

/*
 * Active-page indicator: a solid square underline that is simply present or
 * absent. It never grows, slides or fades.
 */
.header .navbar-nav > .nav-item > .nav-link::after {
  position: absolute;
  right: 0.75rem;
  bottom: 0.3rem;
  left: 0.75rem;
  height: 3px;
  border-radius: 0;
  background: transparent;
  content: '';
}

.header .navbar-nav > .nav-item > .nav-link:hover,
.header .navbar-nav > .nav-item > .nav-link.active,
.header .navbar-nav > .nav-item > .nav-link:focus-visible {
  color: var(--ui-text-primary) !important;
  background: transparent;
}

.header .navbar-nav > .nav-item > .nav-link.active::after {
  background: var(--ui-accent);
}

.header .navbar-nav > .nav-item > .nav-link:focus-visible {
  outline: 2px solid var(--ui-text-primary);
  outline-offset: 0;
}

.header .navbar-toggler {
  color: var(--ui-text-secondary) !important;
  border: 1px solid var(--ui-border-strong) !important;
  border-radius: 0;
  background-color: transparent;
}

/*
 * Flat hamburger glyph: butt caps and miter joins instead of rounded strokes.
 * A background image is its own document, so currentColor cannot reach it and
 * each theme has to name its stroke explicitly.
 */
.header .navbar-toggler-icon {
  --bs-navbar-toggler-icon-bg: url("data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 30 30'%3e%3cpath stroke='%23000000' stroke-linecap='butt' stroke-linejoin='miter' stroke-width='2' d='M4 7h22M4 15h22M4 23h22'/%3e%3c/svg%3e") !important;
}

[data-bs-theme='dark'] .header .navbar-toggler-icon {
  --bs-navbar-toggler-icon-bg: url("data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 30 30'%3e%3cpath stroke='%23ffffff' stroke-linecap='butt' stroke-linejoin='miter' stroke-width='2' d='M4 7h22M4 15h22M4 23h22'/%3e%3c/svg%3e") !important;
}

.form-control::placeholder {
  opacity: 0.5;
}

@media (max-width: 767.98px) {
  .header .navbar-collapse {
    flex-basis: 100%;
    padding: 0.5rem 0 0.25rem;
    border-top: 1px solid var(--ui-border);
  }

  .header .navbar-nav {
    width: 100%;
    margin-right: 0 !important;
    gap: 0.25rem;
  }

  .header .navbar-nav > .nav-item,
  .header .navbar-nav > .nav-item > .nav-link {
    width: 100%;
  }

  .header .navbar-nav > .nav-item > .nav-link {
    display: flex;
    align-items: center;
    min-height: 2.5rem;
    padding: 0.5rem 0.75rem;
    border-radius: 0;
    gap: 0.5rem;
  }

  .header .navbar-nav > .nav-item > .nav-link:hover,
  .header .navbar-nav > .nav-item > .nav-link.active,
  .header .navbar-nav > .nav-item > .nav-link:focus-visible {
    background: var(--ui-surface-hover);
  }

  .header .navbar-utilities {
    align-items: center;
    flex-direction: row;
    gap: 0.5rem;
    width: 100%;
    margin-left: 0 !important;
    padding-top: 0.5rem;
    border-top: 1px solid var(--ui-border);
  }

  .header .navbar-utilities .bd-mode-toggle {
    flex: 1 1 auto;
  }

  .header .navbar-utilities .bd-mode-toggle .nav-link {
    display: flex;
    align-items: center;
    width: 100%;
    min-height: 2.5rem;
    gap: 0.35rem;
  }

  .header .navbar-utilities .account-menu {
    flex: 0 0 auto;
    margin-left: auto;
  }

  .header .navbar-utilities .account-menu .nav-utility-button {
    width: auto;
    min-width: 2.5rem;
    min-height: 2.5rem;
  }

  .header .navbar-utilities .dropdown-menu {
    width: max-content;
    max-width: calc(100vw - 1.5rem);
  }

  .header .navbar-toggler {
    border-radius: 0;
  }
}
</style>
