#!/usr/bin/env node
/**
 * Generate welcome.html from welcome.html.template.
 *
 * The WebUI is English-only, so no translation payload is inlined any more:
 * this build step now only strips the placeholder that used to carry it.
 */

import fs from 'fs'
import path from 'path'
import { fileURLToPath } from 'url'

const __filename = fileURLToPath(import.meta.url)
const __dirname = path.dirname(__filename)

const templatePath = path.join(__dirname, '../welcome.html.template')
const outputPath = path.join(__dirname, '../welcome.html')

if (!fs.existsSync(templatePath)) {
  console.error(`Error: Template file not found: ${templatePath}`)
  process.exit(1)
}

const template = fs.readFileSync(templatePath, 'utf8')

if (!template.includes('WELCOME_LOCALES_INLINE_PLACEHOLDER')) {
  console.error(`Error: Placeholder not found in template file`)
  process.exit(1)
}

// Drop the placeholder line entirely so the generated page carries no stray comment.
const output = template.replace(/[ \t]*<!-- WELCOME_LOCALES_INLINE_PLACEHOLDER -->\r?\n?/, '')

fs.writeFileSync(outputPath, output, 'utf8')
console.log('Generated welcome.html from template')
