// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/window/platform_message.h"

#include <utility>

#include "flutter/common/threads.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/tonic/dart_state.h"
#include "lib/tonic/logging/dart_invoke.h"

namespace blink {

PlatformMessage::PlatformMessage(std::string name,
                                 std::vector<char> data,
                                 tonic::DartPersistentValue callback)
    : name_(std::move(name)),
      data_(std::move(data)),
      callback_(std::move(callback)) {}

PlatformMessage::~PlatformMessage() {
  if (!callback_.is_empty()) {
    Threads::UI()->PostTask(
        ftl::MakeCopyable([callback = std::move(callback_)]() mutable {
          callback.Clear();
        }));
  }
}

void PlatformMessage::InvokeCallback(std::vector<char> data) {
  if (callback_.is_empty())
    return;
  Threads::UI()->PostTask(ftl::MakeCopyable(
      [ callback = std::move(callback_), data = std::move(data) ]() mutable {
        tonic::DartState* dart_state = callback.dart_state().get();
        if (!dart_state)
          return;
        tonic::DartState::Scope scope(dart_state);

        Dart_Handle byte_buffer;
        if (data.empty()) {
          byte_buffer = Dart_Null();
        } else {
          byte_buffer =
              Dart_NewTypedData(Dart_TypedData_kByteData, data.size());
          DART_CHECK_VALID(byte_buffer);

          void* buffer;
          intptr_t length;
          Dart_TypedData_Type type;
          DART_CHECK_VALID(
              Dart_TypedDataAcquireData(byte_buffer, &type, &buffer, &length));
          FTL_CHECK(type = Dart_TypedData_kByteData);
          FTL_CHECK(length = data.size());
          memcpy(buffer, data.data(), length);
          Dart_TypedDataReleaseData(byte_buffer);
        }

        tonic::DartInvoke(callback.Release(), {byte_buffer});
      }));
}

void PlatformMessage::InvokeCallbackWithError() {
  InvokeCallback(std::vector<char>());
}

void PlatformMessage::ClearData() {
  data_.clear();
}

}  // namespace blink