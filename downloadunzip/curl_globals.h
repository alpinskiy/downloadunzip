#pragma once

class CURLGlobals {
 public:
  CURLGlobals() = default;
  ~CURLGlobals();
  CURLGlobals(const CURLGlobals& other) = delete;
  CURLGlobals(CURLGlobals&& other) = delete;
  CURLGlobals& operator=(const CURLGlobals& other) = delete;
  CURLGlobals& operator=(CURLGlobals&& other) = delete;

  bool Initialize();

 private:
  bool initialized_{false};
};