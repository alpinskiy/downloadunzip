#include "stdafx.h"
// https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
#include "unzip.h"

namespace {

// Local file header
#pragma pack(2)
struct alignas(2) LocalFileHeader {
  static constexpr std::uint32_t kSignature = 0x04034b50;
  std::uint16_t version_needed_to_extract;
  std::uint16_t general_purpose_bit_flag;
  std::uint16_t compression_method;
  std::uint16_t last_mod_file_time;
  std::uint16_t last_mod_file_date;
  std::uint32_t crc32;
  std::uint32_t compressed_size;
  std::uint32_t uncompressed_size;
  std::uint16_t file_name_length;
  std::uint16_t extra_field_length;
  // (variable size) file name
  // (variable size) extra field
};

// Central directory header
#pragma pack(2)
struct alignas(2) CentralDirectoryHeader {
  static constexpr std::uint32_t kSignature = 0x02014b50;
  std::uint16_t version_made_by;
  std::uint16_t version_needed_to_extract;
  std::uint16_t general_purpose_bit_flag;
  std::uint16_t compression_method;
  std::uint16_t last_mod_file_time;
  std::uint16_t last_mod_file_date;
  std::uint32_t crc32;
  std::uint32_t compressed_size;
  std::uint32_t uncompressed_size;
  std::uint16_t file_name_length;
  std::uint16_t extra_field_length;
  std::uint16_t file_comment_length;
  std::uint16_t disk_number_start;
  std::uint16_t internal_file_attributes;
  std::uint32_t external_file_attributes;
  std::uint32_t relative_offset_of_local_header;
  // (variable size) file name
  // (variable size) extra field
  // (variable size) file comment
};

// End of central directory record
#pragma pack(2)
struct alignas(2) EndOfCentralDirectoryRecord {
  static constexpr std::uint32_t kSignature = 0x06054b50;
  std::uint16_t number_of_this_disk;
  std::uint16_t number_of_the_disk_with_the_start_of_the_central_directory;
  std::uint16_t total_number_of_entries_in_the_central_directory_on_this_disk;
  std::uint16_t total_number_of_entries_in_the_central_directory;
  std::uint32_t size_of_the_central_directory;
  std::uint32_t
      offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number;
  std::uint16_t zip_file_comment_length;
  // (variable size) zip_file_comment;
};

constexpr std::size_t kFileNameSize = MAX_PATH;
constexpr std::size_t kChunkSize = 0x4000;  // 16 KiB

struct UnzipContext {
  UnzipContext(IBytestream* stream, UnzipOptions* options)
      : stream{stream}, options{options}, filename{}, in{}, out{} {}

  IBytestream* stream;
  UnzipOptions* options;
  char filename[kFileNameSize];
  TBYTE in[kChunkSize];
  TBYTE out[kChunkSize];
};

bool Read(PVOID ptr, std::size_t size, UnzipContext* ctx) {
  return Read(ctx->stream, ptr, size);
}

bool Read(std::size_t size, UnzipContext* ctx) {
  assert(size <= kChunkSize);
  return Read(ctx->in, size, ctx);
}

bool Read(std::size_t size, UnzipContext* ctx, std::size_t* read) {
  size = (std::min)(size, kChunkSize);
  return ctx->stream->Read(ctx->in, size, read);
}

bool Skip(std::size_t size, UnzipContext* ctx) {
  return Skip(ctx->stream, size);
}

bool Write(PVOID ptr, std::size_t size, UnzipContext* ctx, HANDLE dst) {
  if (ctx->options->dryrun) return true;
  DWORD written = 0;
  if (WriteFile(dst, ptr, size, &written, NULL)) return true;
  auto error = GetLastError();
  std::cerr << "error writing file " << ctx->filename << " (code " << error
            << ')' << std::endl;
  return false;
}

bool Write(std::size_t size, UnzipContext* ctx, HANDLE dst) {
  assert(size <= kChunkSize);
  return Write(ctx->out, size, ctx, dst);
}

HANDLE OpenFileForWriting(UnzipContext* ctx) {
  auto path = ctx->filename;
  auto options = ctx->options;
  DWORD creation_disposition = options->overwrite ? CREATE_ALWAYS : CREATE_NEW;
  auto file = ::CreateFileA(path, GENERIC_WRITE, 0, NULL, creation_disposition,
                            FILE_ATTRIBUTE_NORMAL, NULL);
  if (file != INVALID_HANDLE_VALUE) return file;
  auto error = GetLastError();
  if (error != ERROR_PATH_NOT_FOUND) {
    std::cerr << "error creating file " << path << " (code " << error << ')'
              << std::endl;
    return nullptr;
  }
  // create directory
  for (int i = 0; path[i]; ++i) {
    for (; path[i] && path[i] != '/' && path[i] != '\\'; ++i)
      ;
    if (!path[i]) break;
    auto ch = path[i];
    path[i] = 0;
    auto succeeded = CreateDirectory(path, NULL);
    error = GetLastError();
    path[i] = ch;
    if (!succeeded && error != ERROR_ALREADY_EXISTS) {
      error = GetLastError();
      std::cerr << "error creating directory" << path << " (code " << error
                << ')' << std::endl;
      return nullptr;
    }
  }
  file = ::CreateFileA(path, GENERIC_WRITE, 0, NULL, creation_disposition,
                       FILE_ATTRIBUTE_NORMAL, NULL);
  if (file != INVALID_HANDLE_VALUE) return file;
  error = GetLastError();
  std::cerr << "error creating file " << path << " (code " << error << ')'
            << std::endl;
  return nullptr;
}

bool Inflate(std::uint32_t size, UnzipContext* ctx, HANDLE dst, uLong* crc) {
  // https://zlib.net/zpipe.c
  z_stream strm{};
  auto res = inflateInit2(&strm, -MAX_WBITS);
  if (res != Z_OK) {
    std::cerr << "error initializing zlib (code " << res << ')' << std::endl;
    return false;
  }
  using z_stream_guard = std::unique_ptr<z_stream, decltype(&inflateEnd)>;
  auto guard = z_stream_guard{&strm, inflateEnd};
  do {  // until deflate stream ends or end of file
    std::size_t read = min(size, kChunkSize);
    if (!Read(read, ctx)) return false;
    size -= read;
    strm.avail_in = read;
    if (strm.avail_in == 0) break;
    strm.next_in = ctx->in;
    // run inflate() on input until output buffer not full
    do {
      strm.avail_out = kChunkSize;
      strm.next_out = ctx->out;
      res = inflate(&strm, Z_NO_FLUSH);
      assert(res != Z_STREAM_ERROR);  // state not clobbered
      switch (res) {
        case Z_NEED_DICT:
          res = Z_DATA_ERROR;  // and fall through
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          std::cerr << "zlib error (code " << res << ')' << std::endl;
          return false;
      }
      auto inflated = kChunkSize - strm.avail_out;
      if (!Write(inflated, ctx, dst)) return false;
      *crc = crc32(*crc, ctx->out, inflated);
    } while (strm.avail_out == 0);
  } while (res != Z_STREAM_END && size);
  return res == Z_STREAM_END;
}

bool Unzip(LocalFileHeader* header, UnzipContext* ctx) {
  if (!header->file_name_length) {
    std::cerr << "unsupported or invalid zip file format" << std::endl;
    return false;
  }
  // read file name
  if (kFileNameSize - 1 < header->file_name_length) {
    std::cerr << "file name length exceeds " << kFileNameSize - 1
              << " characters" << std::endl;
    return false;
  }
  auto filename = ctx->filename;
  auto options = ctx->options;
  auto ok = Read(filename, header->file_name_length, ctx) &&
            Skip(header->extra_field_length, ctx);
  if (!ok) {
    std::cerr << "unsupported or invalid zip file format" << std::endl;
    return false;
  }
  filename[header->file_name_length] = 0;
  // create file or directory
  auto ch = filename[header->file_name_length - 1];
  if (ch == '/' || ch == '\\') {
    if (header->compressed_size || header->uncompressed_size) {
      std::cerr << "unsupported or invalid zip file format" << std::endl;
      return false;
    }
    if (options->dryrun) return true;
    auto ok = CreateDirectoryA(filename, NULL);
    auto error = GetLastError();
    if (ok || error == ERROR_ALREADY_EXISTS) return true;
    std::cerr << "error creating directory " << filename << " (code " << error
              << ')' << std::endl;
    return false;
  }
  if (!header->compressed_size && header->uncompressed_size) {
    std::cerr << "unsupported or invalid zip file format" << std::endl;
    return false;
  }
  using file_t = std::unique_ptr<void, decltype(&CloseHandle)>;
  auto file = file_t{nullptr, CloseHandle};
  if (!options->dryrun) {
    file.reset(OpenFileForWriting(ctx));
    if (!file) return false;
  }
  if (!header->compressed_size) return true;
  // read file contents
  uLong crc = 0;
  switch (header->compression_method) {
    case 0:  // file is stored (no compression)
    {
      if (header->compressed_size != header->uncompressed_size) {
        std::cerr << "unsupported or invalid zip file format" << std::endl;
        return false;
      }
      auto size = header->uncompressed_size;
      for (std::size_t read = 0; size && Read(size, ctx, &read) &&
                                 Write(ctx->in, read, ctx, file.get());
           crc = crc32(crc, ctx->in, read), size -= read)
        ;
      if (size) return false;
      break;
    }
    case 8:  // file is deflated
    {
      if (!Inflate(header->compressed_size, ctx, file.get(), &crc))
        return false;
      break;
    }
    default:
      std::cerr << "compression method is not supported" << std::endl;
      return false;
  }
  // verify crc32
  if (crc == header->crc32) return true;
  std::cerr << "crc32 does not match for " << filename << ", expected "
            << header->crc32 << ", actual " << crc << std::endl;
  return false;
}

bool Unzip(UnzipContext* ctx) {
  assert(ctx);
  assert(ctx->stream);
  assert(ctx->options);
  // Local file header
  int count = 0;
  std::uint32_t signature = 0;
  for (LocalFileHeader header{};; ++count) {
    if (!Read(&signature, sizeof(std::uint32_t), ctx)) return false;
    if (signature != LocalFileHeader::kSignature) break;
    auto ok =
        Read(&header, sizeof(LocalFileHeader), ctx) && Unzip(&header, ctx);
    if (!ok) return false;
  }
  // Central directory
  for (CentralDirectoryHeader header{}; count; --count) {
    if (signature != CentralDirectoryHeader::kSignature) {
      std::cerr << "unsupported or invalid zip file format" << std::endl;
      return false;
    }
    auto ok = Read(&header, sizeof(CentralDirectoryHeader), ctx) &&
              Skip(header.file_name_length, ctx) &&
              Skip(header.extra_field_length, ctx) &&
              Skip(header.file_comment_length, ctx) &&
              Read(&signature, sizeof(std::uint32_t), ctx);
    if (!ok) return false;
  }
  // End of central directory record
  if (signature != EndOfCentralDirectoryRecord::kSignature) {
    std::cerr << "unsupported or invalid zip file format" << std::endl;
    return false;
  }
  std::size_t read = 0;
  EndOfCentralDirectoryRecord record{};
  auto ok = Read(&record, sizeof(EndOfCentralDirectoryRecord), ctx) &&
            Skip(record.zip_file_comment_length, ctx) && Read(1, ctx, &read);
  if (!ok) return false;
  if (read == 0) return true;
  std::cerr << "unsupported or invalid zip file format" << std::endl;
  return false;
}

}  // namespace

bool Unzip(IBytestream* stream, UnzipOptions* options) {
  return Unzip(std::make_unique<UnzipContext>(stream, options).get());
}