import { getCurrentScope, onScopeDispose } from 'vue'

/**
 * Page background management.
 *
 * The default background is the flat page token from styles/var.css, so the
 * static brutalist palette is always what renders. Two behaviours were removed
 * from this module:
 *
 *   1. A remote decorative wallpaper (assets.alkaidlab.com) that was painted
 *      onto <body> on every page load. Dropping it removes a third-party
 *      network request from every WebUI page load, and lets the OLED page
 *      token show through.
 *   2. A runtime canvas sampler that read the wallpaper's dominant color and
 *      overwrote --text-primary-color / --text-secondary-color /
 *      --text-muted-color / --text-title-color as inline styles on <html>.
 *      Inline styles beat every stylesheet, so the sampler permanently fought
 *      the design tokens. The palette in var.css is now the only definition.
 *
 * A user-supplied background is still honoured, but only from a local
 * data:/blob: URL produced by the drag-and-drop importer below; remote URLs
 * are never painted onto the page.
 */

// Empty means "no image": the CSS page token from styles/var.css renders.
const DEFAULT_BACKGROUND = ''
const STORAGE_KEY = 'customBackground'
const TEXT_COLOR_PROPERTIES = [
  '--text-primary-color',
  '--text-secondary-color',
  '--text-muted-color',
  '--text-title-color',
]

const isLocalImage = (imageUrl) =>
  typeof imageUrl === 'string' && (imageUrl.startsWith('data:') || imageUrl.startsWith('blob:'))

/**
 * Clear any text-color overrides and background classes left behind by the
 * retired color sampler, so the static palette is the only thing in play.
 */
const resetTextColorTheme = () => {
  const root = document.documentElement
  TEXT_COLOR_PROPERTIES.forEach((property) => root.style.removeProperty(property))
  root.classList.remove('text-color-transitioning')
  document.body.classList.remove('bg-light', 'bg-dark')
}

/**
 * Background image management composable.
 */
export function useBackground(options = {}) {
  const {
    defaultBackground = DEFAULT_BACKGROUND,
    storageKey = STORAGE_KEY,
    maxWidth = 1920,
    maxHeight = 1080,
    maxSizeMB = 2,
  } = options

  const getCurrentBackground = () => localStorage.getItem(storageKey) ?? defaultBackground

  const isDevMode = () => localStorage.getItem('sunshine_dev_mode') === '1'

  const clearDevBypass = () => {
    if (localStorage.getItem('sunshine_dev_mode') === '1') {
      localStorage.setItem('sunshine_dev_mode', '0')
    }
  }

  const setBackground = (imageUrl) => {
    resetTextColorTheme()

    // Clearing the inline style hands the page back to the --ui-page-bg token.
    document.body.style.background = ''
    if (isDevMode() || !isLocalImage(imageUrl)) {
      return Promise.resolve()
    }

    document.body.style.background = `url(${imageUrl}) center/cover fixed no-repeat`
    return Promise.resolve()
  }

  // Kept for API compatibility: brightness is no longer sampled at runtime.
  const recheckBackgroundBrightness = () => Promise.resolve()

  const loadBackground = () => setBackground(getCurrentBackground())

  const saveBackground = async (imageData) => {
    clearDevBypass()
    try {
      localStorage.setItem(storageKey, imageData)
    } catch (error) {
      if (error.name === 'QuotaExceededError') {
        localStorage.removeItem(storageKey)
        try {
          localStorage.setItem(storageKey, imageData)
        } catch {
          throw new Error('This image is too large to store. Choose a smaller image or lower its quality.')
        }
      } else {
        throw error
      }
    }
    await setBackground(imageData)
  }

  const calculateResizedDimensions = (width, height) => {
    if (width <= maxWidth && height <= maxHeight) return { width, height }
    const ratio = Math.min(maxWidth / width, maxHeight / height)
    return { width: width * ratio, height: height * ratio }
  }

  const compressWithQuality = (img, width, height, quality) => {
    const canvas = document.createElement('canvas')
    canvas.width = width
    canvas.height = height
    canvas.getContext('2d').drawImage(img, 0, 0, width, height)

    const dataUrl = canvas.toDataURL('image/jpeg', quality)
    const sizeInMB = (dataUrl.length * 3) / 4 / 1024 / 1024

    if (sizeInMB <= maxSizeMB) return dataUrl
    if (quality > 0.3) return compressWithQuality(img, width, height, quality - 0.1)
    return null
  }

  const compressImage = (file, initialQuality = 0.8) =>
    new Promise((resolve, reject) => {
      const reader = new FileReader()
      reader.onload = (event) => {
        const img = new Image()
        img.onload = () => {
          const { width, height } = calculateResizedDimensions(img.width, img.height)
          const result = compressWithQuality(img, width, height, initialQuality)
          result ? resolve(result) : reject(new Error('This image is too large to store. Please choose a smaller image.'))
        }
        img.onerror = () => reject(new Error('Failed to load the image'))
        img.src = event.target.result
      }
      reader.onerror = () => reject(new Error('Failed to read the file'))
      reader.readAsDataURL(file)
    })

  const handleDragOver = (e) => {
    e.preventDefault()
    if (e.dataTransfer?.types?.includes('Files')) {
      document.body.classList.add('dragover')
    }
  }

  const handleDragLeave = () => document.body.classList.remove('dragover')

  const handleDrop = async (e, onError) => {
    e.preventDefault()
    document.body.classList.remove('dragover')

    const file = e.dataTransfer?.files?.[0]
    if (!file?.type.startsWith('image/')) return

    try {
      await saveBackground(await compressImage(file))
    } catch (error) {
      onError?.(error) ?? alert(error.message || 'Something went wrong while processing the image')
    }
  }

  const addDragListeners = (onError) => {
    const handlers = {
      dragover: handleDragOver,
      dragleave: handleDragLeave,
      drop: (e) => handleDrop(e, onError),
    }

    Object.entries(handlers).forEach(([event, handler]) => document.addEventListener(event, handler))
    return () => Object.entries(handlers).forEach(([event, handler]) => document.removeEventListener(event, handler))
  }

  const clearBackground = () => {
    clearDevBypass()
    localStorage.removeItem(storageKey)
    return setBackground(defaultBackground)
  }

  // Listen for the developer background bypass toggle
  if (typeof document !== 'undefined') {
    const onBackgroundBypass = (e) => {
      if (e.detail?.enabled) {
        document.body.style.background = ''
      } else {
        loadBackground()
      }
    }
    window.addEventListener('sunshine-background-bypass', onBackgroundBypass)
    if (getCurrentScope()) {
      onScopeDispose(() => window.removeEventListener('sunshine-background-bypass', onBackgroundBypass))
    }
  }

  return {
    setBackground,
    loadBackground,
    saveBackground,
    compressImage,
    handleDragOver,
    handleDragLeave,
    handleDrop,
    addDragListeners,
    clearBackground,
    getCurrentBackground,
    recheckBackgroundBrightness,
  }
}
