#pragma once

struct IBytestream {
  virtual bool Read(PVOID ptr, std::size_t size, std::size_t* read) = 0;
};

bool Read(IBytestream* stream, PVOID ptr, std::size_t size);
bool Skip(IBytestream* stream, std::size_t size);