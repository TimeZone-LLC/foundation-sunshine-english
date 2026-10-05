// AI output follows the product policy: the WebUI is English-only and never
// negotiates prompt language from browser, host, or saved-locale state.
export function getCurrentLocale() {
  return 'en'
}

export function getPromptLanguageName() {
  return 'English'
}

export function buildLocalizedInstruction() {
  return 'Use English for user-facing explanations and short reasons.'
}
