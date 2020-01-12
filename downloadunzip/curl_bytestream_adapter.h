#pragma once

class CURLBytestreamAdapter : public IBytestream {
 public:
  CURLBytestreamAdapter(curl_write_callback callback);
  ~CURLBytestreamAdapter();
  CURLBytestreamAdapter(const CURLBytestreamAdapter& other) = delete;
  CURLBytestreamAdapter(CURLBytestreamAdapter&& other) = delete;
  CURLBytestreamAdapter& operator=(const CURLBytestreamAdapter& other) = delete;
  CURLBytestreamAdapter& operator=(CURLBytestreamAdapter&& other) = delete;

  bool Initialize(CURL* curl);
  void RunToTheEnd();

 private:
  void ResetCURL();
  bool Read(PVOID ptr, std::size_t size, std::size_t* read);
  bool ReadCURL(bool wait);
  static std::size_t WriteProc(PSTR ptr, std::size_t dummy, std::size_t size,
                               PVOID userdata);

  curl_write_callback callback_;
  CURL* curl_{nullptr};
  std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)> curl_multi_{
      nullptr, curl_multi_cleanup};
  std::vector<char> buffer_;
  std::size_t read_pos_{0};
  int running_count_{0};
  bool curl_done_{false};
  bool done_{false};
};