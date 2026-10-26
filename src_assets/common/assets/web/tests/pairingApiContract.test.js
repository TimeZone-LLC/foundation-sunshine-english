import test from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'

const coreSource = readFileSync(
  new URL('../../../../../src/confighttp.cpp', import.meta.url),
  'utf8',
)
const webPairingSource = readFileSync(new URL('../composables/usePin.js', import.meta.url), 'utf8')

test('host and Web UI retain the bounded pairing API contract', () => {
  for (const method of ['GET', 'POST', 'DELETE']) {
    assert.match(
      coreSource,
      new RegExp(`server\\.resource\\["\\^/api/pin\\$"\\]\\["${method}"\\]`),
      `Sunshine Core no longer registers ${method} /api/pin`,
    )
  }

  assert.match(coreSource, /input\.value\("pairing_id", ""\)/)
  assert.match(coreSource, /is_valid_pairing_id\(pairing_id\)/)
  assert.match(webPairingSource, /apiJson\('\/api\/pin'\)/)
  assert.match(webPairingSource, /pairing_id:\s*selectedPairingId\.value/)
  assert.match(webPairingSource, /method:\s*'DELETE'/)
})
