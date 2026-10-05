import test from 'node:test'
import assert from 'node:assert/strict'

import {
  analytics,
  initFirebase,
  trackEvent,
  trackEvents,
} from '../config/firebase.js'

test('analytics compatibility facade never initializes an outbound client', async () => {
  let scheduled = false
  let fetched = false
  const originalIdleCallback = globalThis.requestIdleCallback
  const originalFetch = globalThis.fetch
  globalThis.requestIdleCallback = () => { scheduled = true }
  globalThis.fetch = async () => { fetched = true }

  try {
    trackEvent('ignored', { private: 'value' })
    trackEvents.pageView('home')
    assert.equal(await initFirebase(), null)
    assert.equal(analytics, null)
    assert.equal(scheduled, false)
    assert.equal(fetched, false)
  } finally {
    globalThis.requestIdleCallback = originalIdleCallback
    globalThis.fetch = originalFetch
  }
})

test('analytics compatibility methods remain safe no-ops', () => {
  const circularValue = {}
  circularValue.self = circularValue

  assert.doesNotThrow(() => trackEvent('null_params', null))
  assert.doesNotThrow(() => trackEvent('circular_params', { value: circularValue }))
  for (const method of Object.values(trackEvents)) {
    assert.doesNotThrow(() => method('value', { detail: true }))
  }
})
