#include "stdafx.h"

#include "bytestream.h"

bool Read(IBytestream* stream, PVOID ptr, std::size_t size) {
  for (std::size_t read = 0; size && stream->Read(ptr, size, &read) && read;
       size -= read, ptr = reinterpret_cast<PBYTE>(ptr) + read)
    ;
  return size == 0;
}

bool Skip(IBytestream* stream, std::size_t size) {
  constexpr std::size_t kBufferSize = 0x400;  // 1 KiB
  static BYTE Buffer[kBufferSize];
  for (; kBufferSize <= size && Read(stream, Buffer, kBufferSize);
       size -= kBufferSize)
    ;
  return Read(stream, Buffer, size);
}