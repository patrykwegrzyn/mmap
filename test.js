'use strict'

const { describe, it } = require('node:test')
const assert = require('node:assert/strict')
const fs = require('fs')
const path = require('path')
const os = require('os')
const { mmap, munmap, msync } = require('./build/Release/mmap.node')

const tmp = path.join(os.tmpdir(), 'mmap-test-' + process.pid)

describe('mmap-native', () => {
  it('mmap read-only', () => {
    const file = path.join(tmp, 'ro.bin')
    fs.mkdirSync(tmp, { recursive: true })
    fs.writeFileSync(file, Buffer.from([0xDE, 0xAD, 0xBE, 0xEF]))
    const fd = fs.openSync(file, 'r')
    const buf = mmap(fd, 4, 0)
    fs.closeSync(fd)
    assert.ok(Buffer.isBuffer(buf))
    assert.equal(buf.length, 4)
    assert.equal(buf[0], 0xDE)
    assert.equal(buf[3], 0xEF)
    munmap(buf)
  })

  it('mmap read-write', () => {
    const file = path.join(tmp, 'rw.bin')
    fs.mkdirSync(tmp, { recursive: true })
    fs.writeFileSync(file, Buffer.alloc(8, 0))
    const fd = fs.openSync(file, 'r+')
    const buf = mmap(fd, 8, 1)
    fs.closeSync(fd)
    buf[0] = 0x42
    buf[7] = 0xFF
    msync(buf)
    munmap(buf)

    const raw = fs.readFileSync(file)
    assert.equal(raw[0], 0x42)
    assert.equal(raw[7], 0xFF)
  })

  it('large mapping', () => {
    const file = path.join(tmp, 'large.bin')
    const size = 1 << 20 // 1MB
    fs.writeFileSync(file, Buffer.alloc(size, 0xAA))
    const fd = fs.openSync(file, 'r')
    const buf = mmap(fd, size, 0)
    fs.closeSync(fd)
    assert.equal(buf.length, size)
    assert.equal(buf[0], 0xAA)
    assert.equal(buf[size - 1], 0xAA)
    munmap(buf)
  })
})

fs.rmSync(tmp, { recursive: true, force: true })
