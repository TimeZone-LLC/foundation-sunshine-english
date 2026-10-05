import test from 'node:test'
import assert from 'node:assert/strict'

import {
  buildLocalizedInstruction,
  getCurrentLocale,
  getPromptLanguageName,
} from '../utils/aiLocale.js'

test('AI prompt language is always English', () => {
  assert.equal(getCurrentLocale(), 'en')
  assert.equal(getPromptLanguageName(), 'English')
  assert.equal(getPromptLanguageName('zh-TW'), 'English')
  assert.equal(getPromptLanguageName('de'), 'English')
  assert.equal(buildLocalizedInstruction('fr'), 'Use English for user-facing explanations and short reasons.')
})
