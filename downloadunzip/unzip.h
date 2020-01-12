#pragma once

struct UnzipOptions {
  bool overwrite;
  bool dryrun;
};

bool Unzip(IBytestream* stream, UnzipOptions* options);