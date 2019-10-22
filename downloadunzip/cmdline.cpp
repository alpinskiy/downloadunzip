#include "stdafx.h"

#include "cmdline.h"

namespace {

bool InitLoginUrl(ProgramOptions* options) {
  static char Buffer[MAX_PATH];
  if (!options->login_path) return true;
  constexpr char kHttp[] = "http://";
  constexpr char kHttps[] = "https://";
  constexpr auto kHttpLen = sizeof(kHttp) - 1;
  constexpr auto kHttpsLen = sizeof(kHttps) - 1;
  std::size_t len = 0;
  if (!strncmp(options->url, kHttp, kHttpLen))
    len = kHttpLen;
  else if (!strncmp(options->url, kHttps, kHttpsLen))
    len = kHttpsLen;
  else
    return false;
  for (auto it = options->url + len; *it && *it != '/'; ++it, ++len)
    ;
  if (MAX_PATH <= len) return false;
  auto ptr = Buffer;
  std::size_t size = MAX_PATH;
  strncpy_s(ptr, MAX_PATH, options->url, len);
  ptr += len;
  size -= len;
  len = strlen(options->login_path);
  if (size <= len) return false;
  strncpy_s(ptr, size, options->login_path, len);
  options->login_url = Buffer;
  return true;
}

inline bool CharToInt(char ch, int* val) {
  if ('0' <= ch && ch <= '9')
    *val = ch - '0';
  else if ('a' <= ch && ch <= 'f')
    *val = ch - 'a' + 10;
  else
    return false;
  return true;
}

bool HexStringToByteArray(PCSTR hex, BYTE (&bytes)[32]) {
  for (auto i = 0, j = 0; hex[i] && j < sizeof(bytes); i += 2, ++j) {
    auto i1 = 0, i2 = 0;
    if (!CharToInt(hex[i], &i1) || !CharToInt(hex[i + 1], &i2)) return false;
    bytes[j] = static_cast<BYTE>((i1 << 4) | i2);
  }
  return true;
}

}  // namespace

bool ParseCommandLine(int argc, PSTR argv[], ProgramOptions* options) {
  if (argc < 2) return false;
  for (int i = 1; i < argc; ++i) {
    if (strncmp(argv[i], "--", 2) == 0) {
      // option
      char* name = argv[i] + 2;
      if (strcmp(name, "login") == 0) {
        if (++i == argc) return false;
        options->login_path = argv[i];
      } else if (strcmp(name, "login-data") == 0) {
        if (++i == argc) return false;
        options->login_post_data = argv[i];
      } else if (strcmp(name, "sha256") == 0) {
        if (++i == argc) return false;
        if (!HexStringToByteArray(argv[i], options->sha256_bytes)) return false;
        options->sha256 = true;
      } else if (strcmp(name, "overwrite") == 0)
        options->overwrite = true;
      else if (strcmp(name, "save") == 0)
        options->save = true;
      else if (strcmp(name, "dryrun") == 0)
        options->dryrun = true;
      else if (strcmp(name, "verbose") == 0)
        options->verbose = true;
      else
        return false;  // unknown option
    } else
      options->url = argv[i];
  }
  return options->url && options->url[0] && InitLoginUrl(options);
}

void PrintHelp() {
  // clang-format off
  std::cerr << "Download and unzip a file.\n";
  std::cerr << "\n";
  std::cerr << "Usage:\n";
  std::cerr << "  download-unzip [OPTIONS] URL\n";
  std::cerr << "\n";
  std::cerr << "Options:\n";
  std::cerr << "  --login PATH\n";
  std::cerr << "  POST request will be made here before downloading.\n";
  std::cerr << "  All received cookies will be sent with the following GET request.\n";
  std::cerr << "  Should be relative to URL host.\n";
  std::cerr << "  \n";
  std::cerr << "  --login-data DATA\n";
  std::cerr << "  x-www-form-urlencoded data to be sent with login POST request.\n";
  std::cerr << "  \n";
  std::cerr << "  --overwrite\n";
  std::cerr << "  Overwrite local files.\n";
  std::cerr << "  \n";
  std::cerr << "  --sha256 HEXSTRING\n";
  std::cerr << "  SHA-256 digest to verify file integrity.\n";
  std::cerr << "  \n";
  std::cerr << "  --save\n";
  std::cerr << "  Save ZIP file to disk.\n";
  std::cerr << "  \n";
  std::cerr << "  --dryrun\n";
  std::cerr << "  Operate as usual but write nothing to disk.\n";
  std::cerr << "  \n";
  std::cerr << "  --verbose\n";
  std::cerr << "  Set verbose mode on.\n";
  std::cerr << "  \n";
  // clang-format on
}