#!/usr/bin/env node

import fs from 'fs'
import path from 'path'
import { fileURLToPath } from 'url'

const scriptDir = path.dirname(fileURLToPath(import.meta.url))
const localeDir = path.join(scriptDir, '../src_assets/common/assets/web/public/assets/locale')
const expectedLocale = 'en.json'

function fail(message) {
  console.error(`English resource validation failed: ${message}`)
  process.exit(1)
}

if (!fs.existsSync(localeDir)) {
  fail(`resource directory does not exist: ${localeDir}`)
}

const localeFiles = fs
  .readdirSync(localeDir, { withFileTypes: true })
  .filter((entry) => entry.isFile() && entry.name.endsWith('.json'))
  .map((entry) => entry.name)
  .sort()

if (localeFiles.length !== 1 || localeFiles[0] !== expectedLocale) {
  fail(`expected only ${expectedLocale}; found ${localeFiles.join(', ') || 'no JSON files'}`)
}

const localePath = path.join(localeDir, expectedLocale)
let resource
try {
  resource = JSON.parse(fs.readFileSync(localePath, 'utf8'))
} catch (error) {
  fail(`${expectedLocale} is not valid JSON: ${error.message}`)
}

if (!resource || typeof resource !== 'object' || Array.isArray(resource)) {
  fail(`${expectedLocale} must contain a JSON object`)
}

let keyCount = 0
const stack = [{ path: '', value: resource }]
while (stack.length > 0) {
  const current = stack.pop()
  for (const [key, value] of Object.entries(current.value)) {
    const keyPath = current.path ? `${current.path}.${key}` : key
    if (!key.trim()) {
      fail(`empty key at ${current.path || '<root>'}`)
    }

    if (typeof value === 'string') {
      keyCount += 1
      if (!value.trim()) {
        fail(`${keyPath} has an empty value`)
      }
      continue
    }

    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      fail(`${keyPath} must be a string or a nested object`)
    }
    stack.push({ path: keyPath, value })
  }
}

if (keyCount === 0) {
  fail(`${expectedLocale} has no resource strings`)
}

console.log(`English resource validation passed: ${keyCount} strings in ${expectedLocale}`)
