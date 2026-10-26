import test from 'node:test'
import assert from 'node:assert/strict'
import { existsSync, readFileSync } from 'node:fs'

const readWebSource = (relativePath) =>
  readFileSync(new URL(`../${relativePath}`, import.meta.url), 'utf8')

test('setup and AI flows do not negotiate non-English locales', () => {
  const setup = readWebSource('composables/useSetupWizard.js')
  const prompts = readWebSource('utils/aiLocale.js')
  const resources = readWebSource('config/resources.js')

  assert.doesNotMatch(setup, /navigator|sessionStorage|hasLocale|language_saved|startsWith\(['"]zh/)
  assert.doesNotMatch(prompts, /LANGUAGE_NAMES|Simplified Chinese|Traditional Chinese|navigator/)
  assert.match(prompts, /return 'en'/)
  assert.match(prompts, /return 'English'/)
  assert.doesNotMatch(resources, /zhHref|resolveResourceHref|FEATURED_RESOURCES/)
})

test('decorative backgrounds, update checking, and promotional setup cards stay removed', () => {
  const navbar = readWebSource('components/layout/Navbar.vue')
  const home = readWebSource('views/Home.vue')
  const setup = readWebSource('components/SetupWizard.vue')
  const constants = readWebSource('utils/constants.js')

  assert.equal(existsSync(new URL('../composables/useBackground.js', import.meta.url)), false)
  assert.equal(existsSync(new URL('../composables/useVersion.js', import.meta.url)), false)
  assert.equal(existsSync(new URL('../components/common/VersionCard.vue', import.meta.url)), false)
  assert.doesNotMatch(navbar, /useBackground|addDragListeners|loadBackground/)
  assert.doesNotMatch(home, /useVersion|VersionCard|fetchVersions|stableBuildAvailable/)
  assert.doesNotMatch(home, /hostConfig(?:\.value)?\?*\.version|config\.version|versionLabel/)
  assert.doesNotMatch(setup, /FEATURED_RESOURCES|promo-service|featured_services/)
  assert.doesNotMatch(constants, /Desktop Pet/)
  assert.match(constants, /name: 'Toolbar'/)
})

test('analytics facade and dependency graph contain no outbound Firebase client', () => {
  const analyticsFacade = readWebSource('config/firebase.js')
  const packageJson = JSON.parse(readFileSync(new URL('../../../../../package.json', import.meta.url), 'utf8'))

  assert.doesNotMatch(analyticsFacade, /firebase\/app|firebase\/analytics|initializeApp|getAnalytics|logEvent/)
  assert.equal(packageJson.dependencies?.firebase, undefined)
  assert.equal(packageJson.dependencies?.marked, undefined)
})
