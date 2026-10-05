import test from 'node:test'
import assert from 'node:assert/strict'
import {
  getBootstrapConfig,
  resetBootstrapDataCache,
} from '../config/bootstrapData.js'

const jsonResponse = (data, { ok = true, status = 200 } = {}) => ({
  ok,
  status,
  json: async () => data,
})

test('deduplicates concurrent bootstrap config requests', async () => {
  resetBootstrapDataCache()
  const originalFetch = globalThis.fetch
  let calls = 0
  globalThis.fetch = async () => {
    calls += 1
    return jsonResponse({ platform: 'windows' })
  }

  try {
    const [first, second] = await Promise.all([getBootstrapConfig(), getBootstrapConfig()])
    assert.equal(calls, 1)
    assert.strictEqual(first, second)
  } finally {
    globalThis.fetch = originalFetch
    resetBootstrapDataCache()
  }
})

test('retries bootstrap data after a failed request', async () => {
  resetBootstrapDataCache()
  const originalFetch = globalThis.fetch
  let calls = 0
  globalThis.fetch = async () => {
    calls += 1
    return calls === 1
      ? jsonResponse({ error: 'temporary failure' }, { ok: false, status: 503 })
      : jsonResponse({ platform: 'linux' })
  }

  try {
    await assert.rejects(getBootstrapConfig(), /temporary failure/)
    assert.equal((await getBootstrapConfig()).platform, 'linux')
    assert.equal(calls, 2)
  } finally {
    globalThis.fetch = originalFetch
    resetBootstrapDataCache()
  }
})
