import test from 'node:test'
import assert from 'node:assert/strict'

import {
  PER_APP_GAMEPAD_MODES,
  normalizePerAppGamepadMode,
} from '../utils/gamepadModes.js'

test('per-app gamepad choices include every selectable Virtual HID profile', () => {
  assert.deepEqual(
    PER_APP_GAMEPAD_MODES.map(({ value }) => value),
    ['', 'auto', 'generic', 'x360', 'xone', 'xseries', 'ds4', 'ds5', 'switch'],
  )
})

test('per-app gamepad normalization accepts known modes only', () => {
  for (const { value } of PER_APP_GAMEPAD_MODES) {
    assert.equal(normalizePerAppGamepadMode(value), value)
  }

  assert.equal(normalizePerAppGamepadMode('invalid'), '')
  assert.equal(normalizePerAppGamepadMode(undefined), '')
})
