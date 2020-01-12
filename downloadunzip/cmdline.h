#pragma once
#include "unzip.h"  // UnzipOptions

struct ProgramOptions {
  PSTR url;
  PSTR login_url;
  PSTR login_path;
  PSTR login_post_data;
  bool sha256;
  BYTE sha256_bytes[32];
  bool save;
  bool overwrite;
  bool dryrun;
  bool verbose;
};

bool ParseCommandLine(int argc, PSTR argv[], ProgramOptions *options);
void PrintHelp();
