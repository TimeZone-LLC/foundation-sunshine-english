const LANGUAGE_NAMES = {
  bg: 'Bulgarian',
  cs: 'Czech',
  de: 'German',
  en: 'English',
  en_GB: 'British English',
  en_US: 'American English',
  es: 'Spanish',
  fr: 'French',
  it: 'Italian',
  ja: 'Japanese',
  ko: 'Korean',
  pl: 'Polish',
  pt: 'Portuguese',
  pt_BR: 'Brazilian Portuguese',
  ru: 'Russian',
  sv: 'Swedish',
  tr: 'Turkish',
  uk: 'Ukrainian',
  zh: 'Simplified Chinese',
  zh_TW: 'Traditional Chinese',
}

function normalizeLocale(raw) {
  if (!raw || typeof raw !== 'string') return 'en'

  const value = raw.replace('-', '_')
  const lower = value.toLowerCase()

  if (lower.startsWith('zh') && /(_tw|_hk|_mo|hant)/.test(lower)) {
    return 'zh_TW'
  }
  if (lower.startsWith('zh')) {
    return 'zh'
  }
  if (lower === 'pt_br') {
    return 'pt_BR'
  }
  if (lower === 'en_gb') {
    return 'en_GB'
  }
  if (lower === 'en_us') {
    return 'en_US'
  }

  const primary = value.split('_')[0]
  return LANGUAGE_NAMES[primary] ? primary : 'en'
}

// The product is English-only, so the locale is fixed rather than negotiated.
// This used to fall back to navigator.languages when the <html lang> attribute
// was missing, which meant a browser configured for another language could still
// steer AI prompts - and the callers that branch on the result - away from English.
export function getCurrentLocale() {
  return 'en'
}

export function getPromptLanguageName(locale = getCurrentLocale()) {
  const normalized = normalizeLocale(locale)
  return LANGUAGE_NAMES[normalized] || LANGUAGE_NAMES.en
}

export function buildLocalizedInstruction(locale = getCurrentLocale()) {
  const languageName = getPromptLanguageName(locale)
  return `Use ${languageName} for user-facing explanations and short reasons.`
}
