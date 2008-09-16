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

#include "googleurl/src/url_canon_ip.h"

#include <stdlib.h>

#include "googleurl/src/url_canon_internal.h"

namespace url_canon {

namespace {

// Converts one of the character types that represent a numerical base to the
// corresponding base.
int BaseForType(SharedCharTypes type) {
  switch (type) {
    case CHAR_HEX:
      return 16;
    case CHAR_DEC:
      return 10;
    case CHAR_OCT:
      return 8;
    default:
      return 0;
  }
}

template<typename CHAR, typename UCHAR>
bool DoFindIPv4Components(const CHAR* spec,
                        const url_parse::Component& host,
                        url_parse::Component components[4]) {
  int cur_component = 0;  // Index of the component we're working on.
  int cur_component_begin = host.begin;  // Start of the current component.
  int end = host.end();
  for (int i = host.begin; /* nothing */; i++) {
    if (i == end || spec[i] == '.') {
      // Found the end of the current component.
      int component_len = i - cur_component_begin;
      components[cur_component] =
          url_parse::Component(cur_component_begin, component_len);

      // The next component starts after the dot.
      cur_component_begin = i + 1;
      cur_component++;

      // Don't allow empty components (two dots in a row), except we may
      // allow an empty component at the end (this would indicate that the
      // input ends in a dot). We also want to error if the component is
      // empty and it's the only component (cur_component == 1).
      if (component_len == 0 && (i != end || cur_component == 1))
        return false;

      if (i == end)
        break;  // End of the input.

      if (cur_component == 4) {
        // Anything else after the 4th component is an error unless it is a
        // dot that would otherwise be treated as the end of input.
        if (spec[i] == '.' && i + 1 == end)
          break;
        return false;  
      }
    } else if (static_cast<UCHAR>(spec[i]) >= 0x80 ||
               !IsIPv4Char(static_cast<unsigned char>(spec[i]))) {
      // Invalid character for an IPv4 address.
      return false;
    }
  }

  // Fill in any unused components.
  while (cur_component < 4)
    components[cur_component++] = url_parse::Component();
  return true;
}

// Converts an IPv4 component to a 32-bit number, returning true on success.
// False means that the number is invalid and that the input can not be an
// IP address. The number will be truncated to 32 bits.
//
// The input is assumed to be ASCII. FindIPv4Components should have stripped
// out any input that is greater than 7 bits. The components are assumed
// to be non-empty.
template<typename CHAR>
bool IPv4ComponentToNumber(const CHAR* spec,
                           const url_parse::Component& component,
                           uint32_t* number) {
  // Figure out the base
  SharedCharTypes base;
  int base_prefix_len = 0;  // Size of the prefix for this base.
  if (spec[component.begin] == '0') {
    // Either hex or dec, or a standalone zero.
    if (component.len == 1) {
      base = CHAR_DEC;
    } else if (spec[component.begin + 1] == 'X' ||
               spec[component.begin + 1] == 'x') {
      base = CHAR_HEX;
      base_prefix_len = 2;
    } else {
      base = CHAR_OCT;
      base_prefix_len = 1;
    }
  } else {
    base = CHAR_DEC;
  }

  // Reject any components that are too long. This is generous, Windows
  // allows at most 16 characters for the entire host name, and 12 per
  // component, while Mac and Linux will take up to 10 per component.
  const int kMaxComponentLen = 16;
  if (component.len - base_prefix_len > kMaxComponentLen)
    return false;

  // Put the component, minus any base prefix, to a NULL-terminated buffer so
  // we can call the standard library. We know the input is 7-bit, so convert
  // to narrow (if this is the wide version of the template) by casting.
  char buf[kMaxComponentLen + 1];
  int dest_i = 0;
  for (int i = base_prefix_len; i < component.len; i++, dest_i++) {
    char input = static_cast<char>(spec[component.begin + i]);

    // Validate that this character is OK for the given base.
    if (!IsCharOfType(input, base))
      return false;
    buf[dest_i] = input;
  }
  buf[dest_i] = 0;

  // Use the 64-bit strtoi so we get a big number (no hex, decimal, or octal
  // number can overflow a 64-bit number in <= 16 characters). Then cast to
  // truncate down to a 32-bit number. This may be further truncated later.
  *number = static_cast<uint32_t>(_strtoui64(buf, NULL, BaseForType(base)));
  return true;
}

// Writes the given address (with each character representing one dotted
// part of an IPv4 address) to the output, and updating |*out_host| to
// identify the added portion.
void AppendIPv4Address(const unsigned char address[4],
                       CanonOutput* output,
                       url_parse::Component* out_host) {
  out_host->begin = output->length();
  for (int i = 0; i < 4; i++) {
    char str[16];
    _itoa_s(address[i], str, 10);

    for (int ch = 0; str[ch] != 0; ch++)
      output->push_back(str[ch]);

    if (i != 3)
      output->push_back('.');
  }
  out_host->len = output->length() - out_host->begin;
}

template<typename CHAR, typename UCHAR>
bool DoCanonicalizeIPv4Address(const CHAR* spec,
                               const url_parse::Component& host,
                               CanonOutput* output,
                               url_parse::Component* out_host) {
  // The identified components. Not all may exist.
  url_parse::Component components[4];
  if (!FindIPv4Components(spec, host, components))
    return false;

  // Convert existing components to digits. Values up to
  // |existing_components| will be valid.
  uint32_t component_values[4];
  int existing_components = 0;
  for (int i = 0; i < 4; i++) {
    if (components[i].len <= 0)
      continue;
    if (!IPv4ComponentToNumber(spec, components[i],
                               &component_values[existing_components]))
      return false;
    existing_components++;
  }

  // Use that sequence of numbers to fill out the 4-component IP address.
  unsigned char address[4];

  // ...first fill all but the last component by truncating to one byte.
  for (int i = 0; i < existing_components - 1; i++)
    address[i] = static_cast<unsigned char>(component_values[i]);

  // ...then fill out the rest of the bytes by filling them with the last
  // component.
  uint32_t last_value = component_values[existing_components - 1];
  if (existing_components == 1)
    address[0] = (last_value & 0xFF000000) >> 24;
  if (existing_components <= 2)
    address[1] = (last_value & 0x00FF0000) >> 16;
  if (existing_components <= 3)
    address[2] = (last_value & 0x0000FF00) >> 8;
  address[3] = last_value & 0xFF;

  AppendIPv4Address(address, output, out_host);
  return true;
}

// This function does NO canonicalization.  It does _some_ validation
// and then copies the component as is to the output.
// TODO: Actual canonicalization!
template<typename CHAR, typename UCHAR>
bool DoCanonicalizeIPv6Address(const CHAR* spec,
                               const url_parse::Component& host,
                               CanonOutput* output,
                               url_parse::Component* out_host) {
  // Make sure the component is bounded by '[' and ']'.
  int end = host.end();
  if (!host.is_nonempty() || spec[host.begin] != '[' || spec[end - 1] != ']')
    return false;

  int num_colons = 0;
  int num_dots = 0;
  int num_hex = 0;
  for (int i = host.begin + 1; i < end - 1; i++) {
    if (static_cast<UCHAR>(spec[i]) >= 0x80)
      return false;

    unsigned char u = static_cast<unsigned char>(spec[i]);
    if (IsHexChar(u)) {
      // No block between ':'s can be more than 4 hex characters.
      if (num_hex > 3)
        return false;
      num_hex++;
    } else if (u == ':') {
      // No ':'s can appear after '.'s have appeared and there can be no
      // more than 7 ':'s separating the 8 hex shorts.
      if (num_dots > 0 || num_colons > 6)
        return false;
      num_colons++;
      num_hex = 0;
    } else if (u == '.') {
      // No hex chars between ':'s is fine (signifies successive
      // zeroed shorts concatentated, but can only be used once).  Not
      // valid for embedded IPv4 addresses, however.
      if (num_hex < 1)
        return false;
      num_dots++;
      num_hex = 0;
    } else {
      // Invalid characters for an IPv6 address.
      return false;
    }
  }
  if (num_colons < 2)
    return false;
  if (num_dots != 0 && num_dots != 3)
    return false;

  // This passed all the checks thus far, so just copy input to output.
  // NOTE: It may still be invalid, and it's definitely not canonicalized.
  // TODO: Actually canonicalize.
  out_host->begin = output->length();
  for (int i = host.begin; i < end; i++)
    output->push_back(static_cast<char>(spec[i]));
  out_host->len = output->length() - out_host->begin;
  return true;
}

}  // namespace

bool FindIPv4Components(const char* spec,
                        const url_parse::Component& host,
                        url_parse::Component components[4]) {
  return DoFindIPv4Components<char, unsigned char>(spec, host, components);
}

bool FindIPv4Components(const UTF16Char* spec,
                        const url_parse::Component& host,
                        url_parse::Component components[4]) {
  return DoFindIPv4Components<UTF16Char, UTF16Char>(spec, host, components);
}

bool CanonicalizeIPAddress(const char* spec,
                           const url_parse::Component& host,
                           CanonOutput* output,
                           url_parse::Component* out_host) {
  return
      DoCanonicalizeIPv4Address<char, unsigned char>(
          spec, host, output, out_host) ||
      DoCanonicalizeIPv6Address<char, unsigned char>(
          spec, host, output, out_host);
}

bool CanonicalizeIPAddress(const UTF16Char* spec,
                           const url_parse::Component& host,
                           CanonOutput* output,
                           url_parse::Component* out_host) {
  return
      DoCanonicalizeIPv4Address<UTF16Char, UTF16Char>(
          spec, host, output, out_host) ||
      DoCanonicalizeIPv6Address<UTF16Char, UTF16Char>(
          spec, host, output, out_host);
}

}  // namespace url_canon
