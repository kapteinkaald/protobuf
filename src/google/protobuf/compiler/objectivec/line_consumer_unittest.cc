// Protocol Buffers - Google's data interchange format
// Copyright 2014 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "google/protobuf/compiler/objectivec/line_consumer.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "absl/strings/str_cat.h"

#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {
namespace {

class TestLineCollector : public LineConsumer {
 public:
  explicit TestLineCollector(std::vector<std::string>* inout_lines,
                    const std::string* reject_line = nullptr,
                    bool skip_msg = false)
    : lines_(inout_lines), reject_(reject_line), skip_msg_(skip_msg) {}

  bool ConsumeLine(const absl::string_view& line, std::string* out_error) override {
    if (reject_ && *reject_ == line) {
      if (!skip_msg_) {
        *out_error = std::string("Rejected '") + *reject_ + "'";
      }
      return false;
    }
    if (lines_) {
      lines_->emplace_back(line);
    }
    return true;
  }

 private:
  std::vector<std::string>* lines_;
  const std::string* reject_;
  bool skip_msg_;
};

const int kBlockSizes[] = {-1, 1, 2, 5, 64};
const int kBlockSizeCount = ABSL_ARRAYSIZE(kBlockSizes);

TEST(ObjCHelper, ParseSimple_BasicsSuccess) {
  const std::vector<std::pair<std::string, std::vector<std::string>>> tests = {
    {"", {}},
    {"a", {"a"}},
    {"a c", {"a c"}},
    {" a c ", {"a c"}},
    {"\ta c ", {"a c"}},
    {"abc\n", {"abc"}},
    {"abc\nd f", {"abc", "d f"}},
    {"\n abc \n def \n\n", {"abc", "def"}},
  };

  for (const auto& test : tests) {
    for (int i = 0; i < kBlockSizeCount; i++) {
      io::ArrayInputStream input(test.first.data(), test.first.size(), kBlockSizes[i]);
      std::string err_str;
      std::vector<std::string> lines;
      TestLineCollector collector(&lines);
      EXPECT_TRUE(ParseSimpleStream(input, "dummy", &collector, &err_str));
      EXPECT_EQ(lines, test.second);
      EXPECT_TRUE(err_str.empty());
    }
  }
}

TEST(ObjCHelper, ParseSimple_DropsComments) {
  const std::vector<std::pair<std::string, std::vector<std::string>>> tests = {
    {"# nothing", {}},
    {"#", {}},
    {"##", {}},
    {"\n# nothing\n", {}},
    {"a # same line", {"a"}},
    {"a # same line\n", {"a"}},
    {"a\n# line\nc", {"a", "c"}},
    {"# n o t # h i n g #", {}},
    {"## n o # t h i n g #", {}},
    {"a# n o t # h i n g #", {"a"}},
    {"a\n## n o # t h i n g #", {"a"}},
  };

  for (const auto& test : tests) {
    for (int i = 0; i < kBlockSizeCount; i++) {
      io::ArrayInputStream input(test.first.data(), test.first.size(), kBlockSizes[i]);
      std::string err_str;
      std::vector<std::string> lines;
      TestLineCollector collector(&lines);
      EXPECT_TRUE(ParseSimpleStream(input, "dummy", &collector, &err_str));
      EXPECT_EQ(lines, test.second);
      EXPECT_TRUE(err_str.empty());
    }
  }
}

TEST(ObjCHelper, ParseSimple_RejectLines) {
  const std::vector<std::tuple<std::string, std::string, int>> tests = {
    std::make_tuple("a\nb\nc", "a", 1),
    std::make_tuple("a\nb\nc", "b", 2),
    std::make_tuple("a\nb\nc", "c", 3),
    std::make_tuple("a\nb\nc\n", "c", 3),
  };

  for (const auto& test : tests) {
    for (int i = 0; i < kBlockSizeCount; i++) {
      io::ArrayInputStream input(std::get<0>(test).data(), std::get<0>(test).size(),
                                 kBlockSizes[i]);
      std::string err_str;
      TestLineCollector collector(nullptr, &std::get<1>(test));
      EXPECT_FALSE(ParseSimpleStream(input, "dummy", &collector, &err_str));
      std::string expected_err =
        absl::StrCat("error: dummy Line ", std::get<2>(test), ", Rejected '", std::get<1>(test), "'");
      EXPECT_EQ(err_str, expected_err);
    }
  }
}

TEST(ObjCHelper, ParseSimple_RejectLinesNoMessage) {
  const std::vector<std::tuple<std::string, std::string, int>> tests = {
    std::make_tuple("a\nb\nc", "a", 1),
    std::make_tuple("a\nb\nc", "b", 2),
    std::make_tuple("a\nb\nc", "c", 3),
    std::make_tuple("a\nb\nc\n", "c", 3),
  };

  for (const auto& test : tests) {
    for (int i = 0; i < kBlockSizeCount; i++) {
      io::ArrayInputStream input(std::get<0>(test).data(), std::get<0>(test).size(),
                                 kBlockSizes[i]);
      std::string err_str;
      TestLineCollector collector(nullptr, &std::get<1>(test), true /* skip msg */);
      EXPECT_FALSE(ParseSimpleStream(input, "dummy", &collector, &err_str));
      std::string expected_err =
        absl::StrCat("error: dummy Line ", std::get<2>(test),
               ", ConsumeLine failed without setting an error.");
      EXPECT_EQ(err_str, expected_err);
    }
  }
}

}  // namespace
}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
