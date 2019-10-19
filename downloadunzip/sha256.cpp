#include "stdafx.h"

#include "sha256.h"

SHA256::~SHA256() {
  BCryptDestroyHash(hash_);
  BCryptCloseAlgorithmProvider(alg_, 0);
}

bool SHA256::Initialize() {
  assert(!alg_);
  assert(!hash_);
  auto res =
      BCryptOpenAlgorithmProvider(&alg_, BCRYPT_SHA256_ALGORITHM, NULL, 0);
  if (!BCRYPT_SUCCESS(res)) {
    std::cerr << "error opening SHA-256 BCrypt algorithm provider (code " << res
              << ")" << std::endl;
    return false;
  }
  res = BCryptCreateHash(alg_, &hash_, NULL, 0, NULL, 0, 0);
  if (BCRYPT_SUCCESS(res)) return true;
  std::cerr << "error creating SHA-256 BCrypt hash (code " << res << ")"
            << std::endl;
  return false;
}

bool SHA256::Hash(PBYTE ptr, std::size_t size) {
  auto res = BCryptHashData(hash_, ptr, size, 0);
  if (BCRYPT_SUCCESS(res)) return true;
  std::cerr << "error updating SHA-256 BCrypt hash (code " << res << ")"
            << std::endl;
  return false;
}

bool SHA256::Finish(BYTE (&bytes)[32]) {
  auto res = BCryptFinishHash(hash_, bytes, sizeof(bytes), 0);
  if (BCRYPT_SUCCESS(res)) return true;
  std::cerr << "error finalizing SHA-256 BCrypt hash (code " << res << ")"
            << std::endl;
  return false;
}