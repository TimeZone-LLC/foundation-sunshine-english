import assert from 'node:assert/strict'
import { readdir, readFile } from 'node:fs/promises'
import test from 'node:test'

const localeDirectory = new URL('../public/assets/locale/', import.meta.url)
const recommendationPattern = /recommend/i

test('experimental USB/IP routing is not labelled as recommended', async () => {
  const localeFiles = (await readdir(localeDirectory)).filter((name) => name.endsWith('.json'))

  for (const localeFile of localeFiles) {
    const locale = JSON.parse(await readFile(new URL(localeFile, localeDirectory), 'utf8'))
    const label = locale.config?.microphone_redirect_backend_auto

    assert.equal(typeof label, 'string', `${localeFile} is missing the automatic microphone backend label`)
    assert.doesNotMatch(
      label,
      recommendationPattern,
      `${localeFile} recommends an experimental backend`,
    )
  }
})

test('English-only microphone copy is complete', async () => {
  const localeFiles = (await readdir(localeDirectory)).filter((name) => name.endsWith('.json'))
  assert.deepEqual(localeFiles, ['en.json'])

  const english = JSON.parse(await readFile(new URL('en.json', localeDirectory), 'utf8'))
  const microphoneKeys = [
    ...Object.keys(english.config).filter((key) => key.startsWith('microphone_redirect_')),
    'stream_mic_test_note_usbip',
    'stream_mic_test_success_usbip',
    'stream_mic_test_usbip_unavailable',
  ]

  for (const key of microphoneKeys) {
    assert.equal(typeof english.config?.[key], 'string', `en.json is missing ${key}`)
    assert.notEqual(english.config[key].trim(), '', `en.json has empty copy for ${key}`)
  }
})
