#include "stdafx.h"

#include "curl_globals.h"

CURLGlobals::~CURLGlobals() {
  if (initialized_) curl_global_cleanup();
}

bool CURLGlobals::Initialize() {
  assert(!initialized_);
  auto res = curl_global_init(CURL_GLOBAL_ALL);
  if (res == CURLE_OK)
    initialized_ = true;
  else
    std::cerr << "error initializing CURL, code " << res << std::endl;
  return initialized_;
}