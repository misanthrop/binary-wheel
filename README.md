# binary-wheel
Minimalistic structured data binary serialization library.

## Quick example

```coffee
bw = require 'binary-wheel'

Emotion = bw.enum ['Smile', 'Sad', 'Evil']

Message = bw.struct [
  ['nickname', bw.string]
  ['message', bw.string]
  ['emotion', bw.optional Emotion]]
  
# pack binary data
buffer = bw.pack Message,
  nickname: 'John'
  message: 'Hi!'
  emotion: 'Smile'

# unpack to JS object
message = bw.unpack Message, buffer
```

## Supported types

Name(s)         | Size in bits
---             | ---
bool            | 1
int8, uint8     | 8
int16, uint16   | 16
int32, uint32   | 32
float32         | 32
scaled          | 8 or 16 or 32
enum            | 1 .. 32
varint          | 2 + (8 or 16 or 32)
optional T      | 1 + (0 or sizeof T)
string          | sizeof varint + length*8
list T          | sizeof varint + length*sizeof T
struct          | sum sizeof members

## Generate C++14

`coffee binary-wheel/cpp.coffee types.coffee`
