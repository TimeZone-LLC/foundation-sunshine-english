#!/usr/bin/env node

import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const scriptDirectory = path.dirname(fileURLToPath(import.meta.url))
const localeDirectory = path.join(
  scriptDirectory,
  '../src_assets/common/assets/web/public/assets/locale',
)
const localePath = path.join(localeDirectory, 'en.json')
const checkMode = process.argv.slice(2).includes('--check')

function sortObject(value) {
  if (value === null || typeof value !== 'object' || Array.isArray(value)) {
    return value
  }

  return Object.fromEntries(
    Object.keys(value)
      .sort()
      .map((key) => [key, sortObject(value[key])]),
  )
}

const localeFiles = fs
  .readdirSync(localeDirectory, { withFileTypes: true })
  .filter((entry) => entry.isFile() && entry.name.endsWith('.json'))
  .map((entry) => entry.name)
  .sort()

if (localeFiles.length !== 1 || localeFiles[0] !== 'en.json') {
  console.error(`Expected only en.json; found: ${localeFiles.join(', ') || 'none'}`)
  process.exit(1)
}

let formatted
try {
  const parsed = JSON.parse(fs.readFileSync(localePath, 'utf8'))
  formatted = `${JSON.stringify(sortObject(parsed), null, 2)}\n`
} catch (error) {
  console.error(`Could not format en.json: ${error.message}`)
  process.exit(1)
}

if (checkMode) {
  if (fs.readFileSync(localePath, 'utf8') !== formatted) {
    console.error('en.json is not formatted. Run npm run i18n:format.')
    process.exit(1)
  }
  console.log('en.json is formatted.')
} else {
  fs.writeFileSync(localePath, formatted, 'utf8')
  console.log('Formatted en.json.')
}
