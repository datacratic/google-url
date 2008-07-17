// Copyright 2008, Google Inc.
// All rights reserved.
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

#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_canon_stdstring.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(URLUtilTest, FindAndCompareScheme) {
  url_parse::Component found_scheme;

  // Simple case where the scheme is found and matches.
  const char kStr1[] = "http://www.com/";
  EXPECT_TRUE(url_util::FindAndCompareScheme(
      kStr1, static_cast<int>(strlen(kStr1)), "http", NULL));
  EXPECT_TRUE(url_util::FindAndCompareScheme(
      kStr1, static_cast<int>(strlen(kStr1)), "http", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component(0, 4));

  // A case where the scheme is found and doesn't match.
  EXPECT_FALSE(url_util::FindAndCompareScheme(
      kStr1, static_cast<int>(strlen(kStr1)), "https", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component(0, 4));

  // A case where there is no scheme.
  const char kStr2[] = "httpfoobar";
  EXPECT_FALSE(url_util::FindAndCompareScheme(
      kStr2, static_cast<int>(strlen(kStr2)), "http", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component());

  // When there is an empty scheme, it should match the empty scheme.
  const char kStr3[] = ":foo.com/";
  EXPECT_TRUE(url_util::FindAndCompareScheme(
      kStr3, static_cast<int>(strlen(kStr3)), "", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component(0, 0));

  // But when there is no scheme, it should fail.
  EXPECT_FALSE(url_util::FindAndCompareScheme("", 0, "", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component());
}

TEST(URLUtilTest, ReplaceComponents) {
  url_parse::Parsed parsed;
  url_canon::RawCanonOutputT<char> output;
  url_parse::Parsed new_parsed;

  // Check that the following calls do not cause crash
  url_canon::Replacements<char> replacements;
  replacements.SetRef("test", url_parse::Component(0, 4));
  url_util::ReplaceComponents(NULL, 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents("", 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  replacements.ClearRef();
  replacements.SetHost("test", url_parse::Component(0, 4));
  url_util::ReplaceComponents(NULL, 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents("", 0, parsed, replacements, NULL, &output,
                              &new_parsed);

  replacements.ClearHost();
  url_util::ReplaceComponents(NULL, 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents("", 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents(NULL, 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents("", 0, parsed, replacements, NULL, &output,
                              &new_parsed);
}

