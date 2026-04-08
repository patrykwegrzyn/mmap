# mmap-native

Native mmap for Node.js. Thin N-API wrapper, nothing fancy.

## Install

```
npm install mmap-native
```

Needs `node-gyp` and a C compiler.

## Usage

```js
const { mmap, munmap, msync } = require('mmap-native')
const fs = require('fs')

const fd = fs.openSync('data.bin', 'r+')
const buf = mmap(fd, 4096, 1) // fd, size, writable
fs.closeSync(fd)

buf[0] = 0x42
msync(buf)
munmap(buf)
```

```js
import { mmap, munmap, msync } from 'mmap-native'
```

## API

| Function | What it does |
|----------|-------------|
| `mmap(fd, size, writable)` | Map file into memory. Returns a `Buffer`. `writable`: 0 = read-only, 1 = read-write |
| `munmap(buf)` | Release the mapping |
| `msync(buf)` | Flush dirty pages to disk |

## License

MIT
