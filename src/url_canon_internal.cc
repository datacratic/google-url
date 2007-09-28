// Copyright 2007, Google Inc.
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

#include <stdlib.h>
#include <string>

#include "googleurl/src/url_canon_internal.h"

namespace url_canon {

namespace {

template<typename CHAR, typename UCHAR>
static bool DoCanonicalizeEscaped(const CHAR* spec, int* begin, int end,
                                  CanonOutput* output) {
  char value;
  if (DecodeEscaped<CHAR>(spec, begin, end, &value)) {
    // Valid escape sequence, re-escape it so we normalize the case of the
    // hex digits in the canonical form.
    AppendEscapedChar(value, output);
    return true;
  }

  // Invalid escaped value, don't copy anything. The caller will pick up on the
  // next character after the percent and treat it normally.
  output->push_back('%');
  return false;
}

// This function assumes the input values are all contained in 8-bit,
// although it allows any type. Returns true if input is valid, false if not.
template<typename CHAR, typename UCHAR>
void DoAppendInvalidNarrowString(const CHAR* spec, int begin, int end,
                                 CanonOutput* output) {
  for (int i = begin; i < end; i++) {
    UCHAR uch = static_cast<UCHAR>(spec[i]);
    if (uch >= 0x80) {
      // Handle UTF-8/16 encodings. This call will correctly handle the error
      // case by appending the invalid character.
      AppendUTF8EscapedChar(spec, &i, end, output);
    } else if (uch <= ' ' || uch == 0x7f) {
      // This function is for error handling, so we escape all control
      // characters and spaces, but not anything else since we lack
      // context to do something more specific.
      AppendEscapedChar(static_cast<unsigned char>(uch), output);
    } else {
      output->push_back(static_cast<char>(uch));
    }
  }
}

// Overrides one component, if the override pointer is non-NULL, the given
// character pointer and dest_component will be updated to reflect that
// override string. Otherwise, no changes will be made.
void DoOverrideComponent(const char* override,
                         const char** dest,
                         url_parse::Component* dest_component) {
  if (override) {
    *dest = override;
    int len = static_cast<int>(strlen(override));
    if (len == 0)
      *dest_component = url_parse::Component();
    else
      *dest_component = url_parse::Component(0, len);
  }
}

}  // namespace

// See the header file for this array's declaration.
const unsigned char kSharedCharTypeTable[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x00 - 0x0f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x10 - 0x1f
    0,           // 0x20  ' ' (escape spaces in queries)
    CHAR_QUERY,  // 0x21  !
    CHAR_QUERY,  // 0x22  "  (IE doesn't escape this in the query!)
    0,           // 0x23  #  (invalid in query since it marks the ref)
    CHAR_QUERY,  // 0x24  $
    CHAR_QUERY,  // 0x25  %
    CHAR_QUERY,  // 0x26  &
    CHAR_QUERY,  // 0x27  '
    CHAR_QUERY,  // 0x28  (
    CHAR_QUERY,  // 0x29  )
    CHAR_QUERY,  // 0x2a  *
    CHAR_QUERY,  // 0x2b  +
    CHAR_QUERY,  // 0x2c  ,
    CHAR_QUERY,  // 0x2d  -
    CHAR_QUERY | CHAR_IPV4,  // 0x2e  .
    CHAR_QUERY,  // 0x2f  /
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT,  // 0x30  0
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT,  // 0x31  1
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT,  // 0x32  2
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT,  // 0x33  3
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT,  // 0x34  4
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT,  // 0x35  5
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT,  // 0x36  6
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT,  // 0x37  7
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC,             // 0x38  8
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX | CHAR_DEC,             // 0x39  9
    CHAR_QUERY,  // 0x3a  :
    CHAR_QUERY,  // 0x3b  ;
    CHAR_QUERY,  // 0x3c  <
    CHAR_QUERY,  // 0x3d  =
    CHAR_QUERY,  // 0x3e  >
    CHAR_QUERY,  // 0x3f  ?
    CHAR_QUERY,  // 0x40  @
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x41  A
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x42  B
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x43  C
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x44  D
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x45  E
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x46  F
    CHAR_QUERY,  // 0x47  G
    CHAR_QUERY,  // 0x48  H
    CHAR_QUERY,  // 0x49  I
    CHAR_QUERY,  // 0x4a  J
    CHAR_QUERY,  // 0x4b  K
    CHAR_QUERY,  // 0x4c  L
    CHAR_QUERY,  // 0x4d  M
    CHAR_QUERY,  // 0x4e  N
    CHAR_QUERY,  // 0x4f  O
    CHAR_QUERY,  // 0x50  P
    CHAR_QUERY,  // 0x51  Q
    CHAR_QUERY,  // 0x52  R
    CHAR_QUERY,  // 0x53  S
    CHAR_QUERY,  // 0x54  T
    CHAR_QUERY,  // 0x55  U
    CHAR_QUERY,  // 0x56  V
    CHAR_QUERY,  // 0x57  W
    CHAR_QUERY | CHAR_IPV4, // 0x58  X
    CHAR_QUERY,  // 0x59  Y
    CHAR_QUERY,  // 0x5a  Z
    CHAR_QUERY,  // 0x5b  [
    CHAR_QUERY,  // 0x5c  \ 
    CHAR_QUERY,  // 0x5d  ]
    CHAR_QUERY,  // 0x5e  ^
    CHAR_QUERY,  // 0x5f  _
    CHAR_QUERY,  // 0x60  `
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x61  a
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x62  b
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x63  c
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x64  d
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x65  e
    CHAR_QUERY | CHAR_IPV4 | CHAR_HEX,  // 0x66  f
    CHAR_QUERY,  // 0x67  g
    CHAR_QUERY,  // 0x68  h
    CHAR_QUERY,  // 0x69  i
    CHAR_QUERY,  // 0x6a  j
    CHAR_QUERY,  // 0x6b  k
    CHAR_QUERY,  // 0x6c  l
    CHAR_QUERY,  // 0x6d  m
    CHAR_QUERY,  // 0x6e  n
    CHAR_QUERY,  // 0x6f  o
    CHAR_QUERY,  // 0x70  p
    CHAR_QUERY,  // 0x71  q
    CHAR_QUERY,  // 0x72  r
    CHAR_QUERY,  // 0x73  s
    CHAR_QUERY,  // 0x74  t
    CHAR_QUERY,  // 0x75  u
    CHAR_QUERY,  // 0x76  v
    CHAR_QUERY,  // 0x77  w
    CHAR_QUERY | CHAR_IPV4,  // 0x78  x
    CHAR_QUERY,  // 0x79  y
    CHAR_QUERY,  // 0x7a  z
    CHAR_QUERY,  // 0x7b  {
    CHAR_QUERY,  // 0x7c  |
    CHAR_QUERY,  // 0x7d  }
    CHAR_QUERY,  // 0x7e  ~
    0,           // 0x7f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80 - 0x8f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90 - 0x9f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xa0 - 0xaf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xb0 - 0xbf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xc0 - 0xcf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xd0 - 0xdf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xe0 - 0xef
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xf0 - 0xff
};

const char kHexCharLookup[0x10] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
};

const char kCharToHexLookup[8] = {
    0,         // 0x00 - 0x1f
    '0',       // 0x20 - 0x3f: digits 0 - 9 are 0x30 - 0x39
    'A' - 10,  // 0x40 - 0x5f: letters A - F are 0x41 - 0x46
    'a' - 10,  // 0x60 - 0x7f: letters a - f are 0x61 - 0x66
    0,         // 0x80 - 0x9F
    0,         // 0xA0 - 0xBF
    0,         // 0xC0 - 0xDF
    0,         // 0xE0 - 0xFF
};

const wchar_t kUnicodeReplacementCharacter = 0xfffd;

bool CanonicalizeEscaped(const char* spec, int* begin, int end,
                         CanonOutput* output) {
  return DoCanonicalizeEscaped<char, unsigned char>(spec, begin, end, output);
}

bool CanonicalizeEscaped(const wchar_t* spec, int* begin, int end,
                         CanonOutput* output) {
  return DoCanonicalizeEscaped<wchar_t, wchar_t>(spec, begin, end, output);
}

void AppendInvalidNarrowString(const char* spec, int begin, int end,
                               CanonOutput* output) {
  DoAppendInvalidNarrowString<char, unsigned char>(spec, begin, end, output);
}

void AppendInvalidNarrowString(const wchar_t* spec, int begin, int end,
                               CanonOutput* output) {
  DoAppendInvalidNarrowString<wchar_t, wchar_t>(spec, begin, end, output);
}

bool ConvertUTF16ToUTF8(const wchar_t* input, int input_len,
                        CanonOutput* output) {
  bool success = true;
  for (int i = 0; i < input_len; i++) {
    unsigned code_point;
    success &= ReadUTFChar(input, &i, input_len, &code_point);
    AppendUTF8Value(code_point, output);
  }
  return success;
}

bool ConvertUTF8ToUTF16(const char* input, int input_len,
                        CanonOutputT<wchar_t>* output) {
  bool success = true;
  for (int i = 0; i < input_len; i++) {
    unsigned code_point;
    success &= ReadUTFChar(input, &i, input_len, &code_point);
    AppendUTF16Value(code_point, output);
  }
  return success;
}

void SetupOverrideComponents(const char* base,
                             const URLComponentSource<char>& repl,
                             URLComponentSource<char>* source,
                             url_parse::Parsed* parsed) {

  DoOverrideComponent(repl.scheme, &source->scheme, &parsed->scheme);
  DoOverrideComponent(repl.username, &source->username, &parsed->username);
  DoOverrideComponent(repl.password, &source->password, &parsed->password);

  // Our host should be empty if not present, so override the default setup.
  DoOverrideComponent(repl.host, &source->host, &parsed->host);
  if (parsed->host.len == -1)
    parsed->host.len = 0;

  DoOverrideComponent(repl.port, &source->port, &parsed->port);
  DoOverrideComponent(repl.path, &source->path, &parsed->path);
  DoOverrideComponent(repl.query, &source->query, &parsed->query);
  DoOverrideComponent(repl.ref, &source->ref, &parsed->ref);
}

}  // namespace url_canon
