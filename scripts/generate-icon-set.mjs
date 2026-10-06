#!/usr/bin/env node

/**
 * Generate every first-party application and tray icon from one definition.
 *
 * No image library is used: the icons are deliberately hard-edged geometric
 * marks, so emitting RGBA PNGs directly keeps every size crisp and makes the
 * output reproducible on Windows, macOS, and Linux build hosts.
 */

import { deflateSync } from 'node:zlib'
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const repositoryRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const panelRoot = resolve(repositoryRoot, 'src_assets/common/sunshine-control-panel')
const checkOnly = process.argv.includes('--check')

const geometry = Object.freeze({
  appInset: 0.21875,
  trayOuter: 0.140625,
  trayIdleInner: 0.265625,
  trayPausedInner: 0.28125,
  trayBarTop: 0.4375,
  trayBarBottom: 0.5625,
})

const icoSizes = [16, 24, 32, 48, 64, 128, 256]
const icnsSizes = new Map([
  [16, 'icp4'],
  [32, 'icp5'],
  [64, 'icp6'],
  [128, 'ic07'],
  [256, 'ic08'],
  [512, 'ic09'],
  [1024, 'ic10'],
])

const generated = new Map()

function crc32(buffer) {
  let crc = 0xffffffff
  for (const byte of buffer) {
    crc ^= byte
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1))
    }
  }
  return (crc ^ 0xffffffff) >>> 0
}

function pngChunk(type, data) {
  const name = Buffer.from(type, 'ascii')
  const length = Buffer.alloc(4)
  length.writeUInt32BE(data.length)
  const checksum = Buffer.alloc(4)
  checksum.writeUInt32BE(crc32(Buffer.concat([name, data])))
  return Buffer.concat([length, name, data, checksum])
}

function bounds(size, start, end = 1 - start) {
  return [Math.round(size * start), Math.round(size * end)]
}

function inside(value, start, end) {
  return value >= start && value < end
}

function renderPixel(state, size, x, y) {
  if (state === 'app') {
    const [start, end] = bounds(size, geometry.appInset)
    return inside(x, start, end) && inside(y, start, end)
      ? [255, 255, 255, 255]
      : [0, 0, 0, 255]
  }

  const [outerStart, outerEnd] = bounds(size, geometry.trayOuter)
  const inOuter = inside(x, outerStart, outerEnd) && inside(y, outerStart, outerEnd)
  if (!inOuter) return [0, 0, 0, 0]

  if (state === 'playing') return [255, 255, 255, 255]

  const innerRatio = state === 'paused'
    ? geometry.trayPausedInner
    : geometry.trayIdleInner
  const [innerStart, innerEnd] = bounds(size, innerRatio)
  const inInner = inside(x, innerStart, innerEnd) && inside(y, innerStart, innerEnd)

  if (state === 'locked') {
    const [barStart, barEnd] = bounds(size, geometry.trayBarTop, geometry.trayBarBottom)
    const inBar = inside(y, barStart, barEnd)
    return (!inInner || inBar) ? [255, 255, 255, 255] : [0, 0, 0, 0]
  }

  return inInner ? [0, 0, 0, 0] : [255, 255, 255, 255]
}

function makePng(size, state) {
  const stride = size * 4 + 1
  const raw = Buffer.alloc(stride * size)
  for (let y = 0; y < size; y += 1) {
    const row = y * stride
    raw[row] = 0
    for (let x = 0; x < size; x += 1) {
      const pixel = renderPixel(state, size, x, y)
      const offset = row + 1 + x * 4
      raw[offset] = pixel[0]
      raw[offset + 1] = pixel[1]
      raw[offset + 2] = pixel[2]
      raw[offset + 3] = pixel[3]
    }
  }

  const header = Buffer.alloc(13)
  header.writeUInt32BE(size, 0)
  header.writeUInt32BE(size, 4)
  header[8] = 8
  header[9] = 6

  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    pngChunk('IHDR', header),
    pngChunk('IDAT', deflateSync(raw, { level: 9 })),
    pngChunk('IEND', Buffer.alloc(0)),
  ])
}

function makeIco(state) {
  const images = icoSizes.map((size) => ({ size, data: makePng(size, state) }))
  const header = Buffer.alloc(6 + images.length * 16)
  header.writeUInt16LE(0, 0)
  header.writeUInt16LE(1, 2)
  header.writeUInt16LE(images.length, 4)

  let dataOffset = header.length
  images.forEach(({ size, data }, index) => {
    const offset = 6 + index * 16
    header[offset] = size === 256 ? 0 : size
    header[offset + 1] = size === 256 ? 0 : size
    header[offset + 2] = 0
    header[offset + 3] = 0
    header.writeUInt16LE(1, offset + 4)
    header.writeUInt16LE(32, offset + 6)
    header.writeUInt32LE(data.length, offset + 8)
    header.writeUInt32LE(dataOffset, offset + 12)
    dataOffset += data.length
  })

  return Buffer.concat([header, ...images.map(({ data }) => data)])
}

function makeIcns() {
  const chunks = [...icnsSizes].map(([size, type]) => {
    const data = makePng(size, 'app')
    const chunk = Buffer.alloc(8 + data.length)
    chunk.write(type, 0, 4, 'ascii')
    chunk.writeUInt32BE(chunk.length, 4)
    data.copy(chunk, 8)
    return chunk
  })
  const header = Buffer.alloc(8)
  header.write('icns', 0, 4, 'ascii')
  header.writeUInt32BE(8 + chunks.reduce((sum, chunk) => sum + chunk.length, 0), 4)
  return Buffer.concat([header, ...chunks])
}

function add(relativePath, contents) {
  generated.set(resolve(repositoryRoot, relativePath), Buffer.from(contents))
}

const appIco = makeIco('app')
const idleIco = makeIco('idle')
const playingIco = makeIco('playing')
const pausedIco = makeIco('paused')
const lockedIco = makeIco('locked')
const appIcns = makeIcns()

add('sunshine.svg', `<?xml version="1.0" encoding="utf-8"?>\n<!-- Generated by scripts/generate-icon-set.mjs. -->\n<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">\n  <rect width="256" height="256" fill="#000000"/>\n  <rect x="56" y="56" width="144" height="144" fill="#ffffff"/>\n</svg>\n`)
add('sunshine.png', makePng(512, 'app'))
add('sunshine.ico', appIco)
add('sunshine.icns', appIcns)

const webImages = 'src_assets/common/assets/web/public/images'
add(`${webImages}/logo-sunshine-16.png`, makePng(16, 'app'))
add(`${webImages}/logo-sunshine-45.png`, makePng(45, 'app'))
add(`${webImages}/logo-sunshine-1024.png`, makePng(1024, 'app'))
add(`${webImages}/sunshine.ico`, idleIco)
for (const [name, state, ico] of [
  ['sunshine-playing', 'playing', playingIco],
  ['sunshine-pausing', 'paused', pausedIco],
  ['sunshine-locked', 'locked', lockedIco],
]) {
  add(`${webImages}/${name}-16.png`, makePng(16, state))
  add(`${webImages}/${name}-45.png`, makePng(45, state))
  add(`${webImages}/${name}.png`, makePng(256, state))
  add(`${webImages}/${name}.ico`, ico)
}

const panelIcons = 'src_assets/common/sunshine-control-panel/src-tauri/icons'
add(`${panelIcons}/32x32.png`, makePng(32, 'app'))
add(`${panelIcons}/128x128.png`, makePng(128, 'app'))
add(`${panelIcons}/128x128@2x.png`, makePng(256, 'app'))
add(`${panelIcons}/icon.icns`, appIcns)
add(`${panelIcons}/icon.ico`, appIco)
add(`${panelIcons}/tray/sunshine.ico`, idleIco)
add(`${panelIcons}/tray/sunshine-playing.ico`, playingIco)
add(`${panelIcons}/tray/sunshine-pausing.ico`, pausedIco)
add(`${panelIcons}/tray/sunshine-locked.ico`, lockedIco)
add('src_assets/common/sunshine-control-panel/src/assets/sunshine.ico', appIco)

const mismatches = []
for (const [path, expected] of generated) {
  if (checkOnly) {
    if (!existsSync(path) || !readFileSync(path).equals(expected)) {
      mismatches.push(path.slice(repositoryRoot.length + 1))
    }
    continue
  }
  mkdirSync(dirname(path), { recursive: true })
  writeFileSync(path, expected)
}

if (checkOnly && mismatches.length > 0) {
  console.error('Generated icon files are stale:')
  for (const path of mismatches) console.error(`  ${path}`)
  console.error('Run: npm run icons:generate')
  process.exitCode = 1
} else if (checkOnly) {
  console.log(`Icon validation passed: ${generated.size} files match the canonical definition.`)
} else {
  console.log(`Generated ${generated.size} monochrome icon files.`)
}
