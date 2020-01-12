#include "stdafx.h"

#include "cmdline.h"
#include "curl_bytestream_adapter.h"
#include "curl_globals.h"
#include "sha256.h"

ProgramOptions Options;
char ContentDisposition[MAX_PATH];
bool ContentDispositionFound;
HANDLE ZipFile;
DWORD ZipFileError;
SHA256 Sha256;
BYTE Sha256Bytes[32];
bool Sha256Error;

std::size_t CURLHeaderFunction(PSTR ptr, std::size_t size, std::size_t nitems,
                               PVOID userdata) {
  if (ContentDispositionFound) return size * nitems;
  size *= nitems;
  constexpr char kContentDisposition[] = "content-disposition:";
  constexpr auto kContentDispositionLen = sizeof(kContentDisposition) - 1;
  for (int i = 0; i < size; ++i) {
    int j = 0;
    for (; i < size && j < kContentDispositionLen &&
           tolower(ptr[i]) == kContentDisposition[j];
         ++i, ++j)
      ;
    ContentDispositionFound = (j == kContentDispositionLen);
    j = i;
    for (; i < size && ptr[i] != '\r' && ptr[i] != '\n'; ++i)
      ;
    if (ContentDispositionFound) {
      strncpy_s(ContentDisposition, MAX_PATH - 1, ptr + j, i - j);
      ContentDisposition[MAX_PATH - 1] = 0;
      break;
    }
    for (; i < size && (ptr[i] == '\r' || ptr[i] == '\n'); ++i)
      ;
  }
  return size;
}

bool OpenZipFile(PCSTR path) {
  assert(!ZipFile || ZipFile == INVALID_HANDLE_VALUE);
  if (Options.dryrun) return true;
  ZipFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                        Options.overwrite ? CREATE_ALWAYS : CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL, NULL);
  if (ZipFile != INVALID_HANDLE_VALUE) return true;
  ZipFileError = GetLastError();
  return false;
}

bool OpenZipFile() {
  // try get filename from Content-Disposition header
  if (ContentDispositionFound) {
    constexpr char kFileName[] = "filename=";
    constexpr auto kFileNameLen = sizeof(kFileName) - 1;
    auto filename = strstr(ContentDisposition, kFileName);
    if (filename != NULL) {
      filename += kFileNameLen;
      auto end = strchr(filename, ';');
      if (end) *end = 0;
      if (OpenZipFile(filename)) return true;
    }
  }
  // didn't get filename from Content-Disposition, get from URL
  auto first = Options.url + strlen(Options.url);
  auto last = first;
  for (auto i = 0; i < 2; ++i) {
    // going from the end to the begining, skip the query string
    // (values past '?')
    for (; first != Options.url && *(first - 1) != '/' && *(first - 1) != '?';
         --first)
      ;
    // get everything to the right of the '/'
    if (Options.url == first || *(first - 1) == '/') break;
    last = first - 1;
    first = last;
    // URL might end up with '/', repeat one more time
  }
  if (first == last) {
    std::cerr << "error getting ZIP file name" << std::endl;
    ZipFileError = ERROR_PATH_NOT_FOUND;
    return false;
  }
  return OpenZipFile(std::string{first, last}.c_str());
}

std::size_t WriteZipFile(PSTR ptr, std::size_t dummy, std::size_t size,
                         PVOID userdata) {
  if (ZipFileError) return size;
  if (!ZipFile && !OpenZipFile()) {
    assert(ZipFileError);
    return size;
  }
  assert(ZipFile);
  assert(ZipFile != INVALID_HANDLE_VALUE);
  auto ret = 0;
  for (; size;) {
    DWORD written = 0;
    auto ok = WriteFile(ZipFile, ptr, size, &written, NULL);
    if (!ok) {
      ZipFileError = GetLastError();
      std::cerr << "error writing file (code " << ZipFileError << ')'
                << std::endl;
      break;
    }
    ptr += written;
    size -= written;
    ret += written;
  }
  return ret;
}

std::size_t CURLWriteFunction(PSTR ptr, std::size_t size, std::size_t nmemb,
                              PVOID userdata) {
  if (Options.save && !ZipFileError) WriteZipFile(ptr, size, nmemb, userdata);
  if (Options.sha256 && !Sha256Error)
    Sha256Error = !Sha256.Hash(reinterpret_cast<PBYTE>(ptr), size);
  return size * nmemb;
}

std::size_t CURLDiscardFunction(PSTR ptr, std::size_t size, std::size_t nmemb,
                                PVOID userdata) {
  return size * nmemb;
}

int main(int argc, char *argv[]) {
  if (!ParseCommandLine(argc, argv, &Options)) {
    PrintHelp();
    return 1;
  }
  try {
    CURLGlobals curl_globals;
    if (!curl_globals.Initialize()) return 1;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl{curl_easy_init(),
                                                             curl_easy_cleanup};
    if (!curl) {
      std::cerr << "error initializing CURL" << std::endl;
      return 1;
    }
    if (Options.verbose) curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1);
    if (Options.login_url) {
      curl_easy_setopt(curl.get(), CURLOPT_URL, Options.login_url);
      curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, Options.login_post_data);
      curl_easy_setopt(curl.get(), CURLOPT_COOKIEFILE, "");
      curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, CURLDiscardFunction);
      auto res = curl_easy_perform(curl.get());
      if (res != CURLE_OK) return 1;
    }
    CURLBytestreamAdapter curl_bytestream_adapter{CURLWriteFunction};
    if (!curl_bytestream_adapter.Initialize(curl.get())) return 1;
    curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_URL, Options.url);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    if (Options.save)
      curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, CURLHeaderFunction);
    if (Options.sha256) Sha256Error = !Sha256.Initialize();
    UnzipOptions unzip_options{};
    unzip_options.overwrite = Options.overwrite;
    unzip_options.dryrun = Options.dryrun;
    auto ok = Unzip(&curl_bytestream_adapter, &unzip_options);
    if (Options.save || Options.sha256) {
      curl_bytestream_adapter.RunToTheEnd();
      if (Options.sha256) {
        if (!Sha256.Finish(Sha256Bytes))
          ok = false;
        else if (!std::equal(std::cbegin(Sha256Bytes), std::cend(Sha256Bytes),
                             std::cbegin(Options.sha256_bytes))) {
          ok = false;
          std::cerr << "SHA-256 hash doesn't match" << std::endl;
        }
      }
    }
    return ok ? 0 : 1;
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}