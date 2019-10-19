#include "stdafx.h"

#include "curl_bytestream_adapter.h"

CURLBytestreamAdapter::CURLBytestreamAdapter(curl_write_callback callback)
    : callback_{callback},
      curl_{nullptr},
      curl_multi_{nullptr, curl_multi_cleanup},
      read_pos_{0},
      running_count_{0},
      curl_done_{false} {}

bool CURLBytestreamAdapter::Initialize(CURL* curl) {
  assert(curl);
  assert(!curl_);
  curl_multi_.reset(curl_multi_init());
  if (!curl_multi_) return false;
  auto error = curl_multi_add_handle(curl_multi_.get(), curl);
  if (error != CURLM_OK) return false;
  curl_ = curl;  // initialized
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteProc);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
  return true;
}

CURLBytestreamAdapter::~CURLBytestreamAdapter() {
  if (curl_) {
    curl_multi_remove_handle(curl_multi_.get(), curl_);
    ResetCURL();
  }
}

void CURLBytestreamAdapter::RunToTheEnd() {
  if (callback_)
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, callback_);
  else
    ResetCURL();
  for (auto i = 0u; !curl_done_ && ReadCURL(0 < i); ++i)
    ;
}

void CURLBytestreamAdapter::ResetCURL() {
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, stderr);
  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, NULL);
}

bool CURLBytestreamAdapter::Read(PVOID ptr, std::size_t size,
                                 std::size_t* read) {
  if (done_) return false;
  auto avail = buffer_.size() - read_pos_;
  for (auto i = 0u; size && (avail || !curl_done_); ++i) {
    if (avail) {
      auto it = reinterpret_cast<PSTR>(ptr);
      auto read_size = (std::min)(size, avail);
      for (std::size_t i = 0; i < read_size; ++i) *it++ = buffer_[read_pos_++];
      ptr = it;
      size -= read_size;
      *read += read_size;
    } else {
      read_pos_ = 0;
      buffer_.clear();
      if (!ReadCURL(0 < i)) curl_done_ = true;
    }
    avail = buffer_.size() - read_pos_;
  }
  if (size) done_ = true;
  return true;
}

bool CURLBytestreamAdapter::ReadCURL(bool wait) {
  if (wait) {
    auto error = curl_multi_wait(curl_multi_.get(), NULL, 0, 1000, NULL);
    assert(error == CURLE_OK);
  }
  auto error = curl_multi_perform(curl_multi_.get(), &running_count_);
  if (error != CURLE_OK) return false;
  auto msgs_in_queue = 0;
  auto msg = curl_multi_info_read(curl_multi_.get(), &msgs_in_queue);
  assert(!msgs_in_queue);
  if (msg) {
    assert(msg->msg == CURLMSG_DONE);
    curl_done_ = true;
    assert(msg->data.result == CURLE_OK);
  }
  return true;
}

std::size_t CURLBytestreamAdapter::WriteProc(PSTR ptr, std::size_t dummy,
                                             std::size_t size, PVOID userdata) {
  assert(dummy == 1);
  assert(userdata);
  auto this_ = reinterpret_cast<CURLBytestreamAdapter*>(userdata);
  std::copy(ptr, ptr + size, std::back_inserter(this_->buffer_));
  if (this_->callback_) this_->callback_(ptr, dummy, size, nullptr);
  return size;
}