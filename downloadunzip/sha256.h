#pragma once

class SHA256 {
 public:
  SHA256() = default;
  ~SHA256();
  SHA256(const SHA256& other) = delete;
  SHA256(SHA256&& other) = delete;
  SHA256& operator=(const SHA256& other) = delete;
  SHA256& operator=(SHA256&& other) = delete;

  bool Initialize();
  bool Hash(PBYTE ptr, std::size_t size);
  bool Finish(BYTE (&bytes)[32]);

 private:
  BCRYPT_ALG_HANDLE alg_{nullptr};
  BCRYPT_HASH_HANDLE hash_{nullptr};
};