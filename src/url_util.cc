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

#include <string.h>
#include <vector>

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/url_util.h"

namespace url_util {

namespace {

const char kFileScheme[] = "file";  // Used in a number of places.

const int kNumStandardURLSchemes = 5;
const char* kStandardURLSchemes[kNumStandardURLSchemes] = {
  "http",
  "https",
  kFileScheme,  // Yes, file urls can have a hostname!
  "ftp",
  "gopher",
};

// List of the currently installed standard schemes. This list is lazily
// initialized by InitStandardSchemes and is leaked on shutdown to prevent
// any destructors from being called that will slow us down or cause problems.
std::vector<const char*>* standard_schemes = NULL;

// Ensures that the standard_schemes list is initialized, does nothing if it
// already has values.
void InitStandardSchemes() {
  if (standard_schemes)
    return;
  standard_schemes = new std::vector<const char*>;
  for (int i = 0; i < kNumStandardURLSchemes; i++)
    standard_schemes->push_back(kStandardURLSchemes[i]);
}

// Given a string and a range inside the string, compares it to the given
// lower-case |compare_to| buffer.
template<typename CHAR>
inline bool CompareSchemeComponent(const CHAR* spec,
                                   const url_parse::Component& component,
                                   const char* compare_to) {
  return LowerCaseEqualsASCII(&spec[component.begin],
                              &spec[component.end()],
                              compare_to);
}

// Returns true if the given scheme is one of the registered "standard"
// schemes.
template<typename CHAR>
bool IsStandardSchemeT(const CHAR* scheme,
                       int scheme_len) {
  InitStandardSchemes();
  for (size_t i = 0; i < standard_schemes->size(); i++) {
    if (LowerCaseEqualsASCII(scheme, &scheme[scheme_len],
                             standard_schemes->at(i)))
      return true;
  }
  return false;
}

template<typename CHAR>
bool IsStandardT(const CHAR* spec, int spec_len) {
  // TODO(brettw) bug 772441: treat URLs with "://" and possible ":/" as
  // standard.
  url_parse::Component scheme;
  if (!url_parse::ExtractScheme(spec, spec_len, &scheme))
    return false;
  return IsStandardScheme(&spec[scheme.begin], scheme.len);
}

template<typename CHAR>
bool FindAndCompareSchemeT(const CHAR* str,
                           int str_len,
                           const char* compare,
                           url_parse::Component* found_scheme) {
  url_parse::Component our_scheme;
  if (!url_parse::ExtractScheme(str, str_len, &our_scheme)) {
    // No scheme.
    if (found_scheme)
      *found_scheme = url_parse::Component();
    return false;
  }
  if (found_scheme)
    *found_scheme = our_scheme;
  return CompareSchemeComponent(str, our_scheme, compare);
}

template<typename CHAR>
bool CanonicalizeT(const CHAR* spec,
                   int spec_len,
                   url_canon::CanonOutput* output,
                   url_parse::Parsed* output_parsed) {
  url_parse::Component scheme;
  if(!url_parse::ExtractScheme(spec, spec_len, &scheme))
    return false;

  // This is the parsed version of the input URL, we have to canonicalize it
  // before storing it in our object.
  bool success;
  url_parse::Parsed parsed_input;
  if (CompareSchemeComponent(spec, scheme, kFileScheme)) {
    // File URLs are special.
    url_parse::ParseFileURL(spec, spec_len, &parsed_input);
    success = url_canon::CanonicalizeFileURL(spec, spec_len, parsed_input,
                                             output, output_parsed);

  } else if (IsStandardScheme(&spec[scheme.begin], scheme.len)) {
    // All "normal" URLs.
    url_parse::ParseStandardURL(spec, spec_len, &parsed_input);
    success = url_canon::CanonicalizeStandardURL(spec, spec_len, parsed_input,
                                                 output, output_parsed);

  } else {
    // "Weird" URLs like data: and javascript:
    url_parse::ParsePathURL(spec, spec_len, &parsed_input);
    success = url_canon::CanonicalizePathURL(spec, spec_len, parsed_input,
                                             output, output_parsed);
  }
  return success;
}

template<typename CHAR>
bool ResolveRelativeT(const char* base_spec,
                      const url_parse::Parsed& base_parsed,
                      const CHAR* relative,
                      int relative_length,
                      url_canon::CanonOutput* output,
                      url_parse::Parsed* output_parsed) {
  bool standard_base_scheme =
      IsStandardScheme(&base_spec[base_parsed.scheme.begin],
                       base_parsed.scheme.len);

  bool is_relative;
  url_parse::Component relative_component;
  if (!url_canon::IsRelativeURL(base_spec, base_parsed,
                                relative, relative_length,
                                standard_base_scheme,
                                &is_relative,
                                &relative_component)) {
    // Error resolving.
    return false;
  }

  if (is_relative) {
    // Relative, resolve and canonicalize.
    bool file_base_scheme =
        CompareSchemeComponent(base_spec, base_parsed.scheme, kFileScheme);
    return url_canon::ResolveRelativeURL(base_spec, base_parsed,
                                         file_base_scheme, relative,
                                         relative_component,
                                         output, output_parsed);
  }

  // Not relative, canonicalize the input.
  return CanonicalizeT(relative, relative_length, output, output_parsed);
}

}  // namespace

void AddStandardScheme(const char* new_scheme) {
  size_t scheme_len = strlen(new_scheme);
  if (scheme_len == 0)
    return;

  // Dulicate the scheme into a new buffer and add it to the list of standard
  // schemes. This pointer will be leaked on shutdown.
  char* dup_scheme = new char[scheme_len + 1];
  memcpy(dup_scheme, new_scheme, scheme_len + 1);

  InitStandardSchemes();
  standard_schemes->push_back(dup_scheme);
}

bool IsStandardScheme(const char* scheme, int scheme_len) {
  return IsStandardSchemeT(scheme, scheme_len);
}

bool IsStandardScheme(const wchar_t* scheme, int scheme_len) {
  return IsStandardSchemeT(scheme, scheme_len);
}

bool IsStandard(const char* spec, int spec_len) {
  return IsStandardT(spec, spec_len);
}

bool IsStandard(const wchar_t* spec, int spec_len) {
  return IsStandardT(spec, spec_len);
}

bool FindAndCompareScheme(const char* str,
                          int str_len,
                          const char* compare,
                          url_parse::Component* found_scheme) {
  return FindAndCompareSchemeT(str, str_len, compare, found_scheme);
}

bool FindAndCompareScheme(const wchar_t* str,
                          int str_len,
                          const char* compare,
                          url_parse::Component* found_scheme) {
  return FindAndCompareSchemeT(str, str_len, compare, found_scheme);
}

bool Canonicalize(const char* spec,
                  int spec_len,
                  url_canon::CanonOutput* output,
                  url_parse::Parsed* output_parsed) {
  return CanonicalizeT(spec, spec_len, output, output_parsed);
}

bool Canonicalize(const wchar_t* spec,
                  int spec_len,
                  url_canon::CanonOutput* output,
                  url_parse::Parsed* output_parsed) {
  return CanonicalizeT(spec, spec_len, output, output_parsed);
}

bool ResolveRelative(const char* base_spec,
                     const url_parse::Parsed& base_parsed,
                     const char* relative,
                     int relative_length,
                     url_canon::CanonOutput* output,
                     url_parse::Parsed* output_parsed) {
  return ResolveRelativeT(base_spec, base_parsed, relative, relative_length,
                          output, output_parsed);
}

bool ResolveRelative(const char* base_spec,
                     const url_parse::Parsed& base_parsed,
                     const wchar_t* relative,
                     int relative_length,
                     url_canon::CanonOutput* output,
                     url_parse::Parsed* output_parsed) {
  return ResolveRelativeT(base_spec, base_parsed, relative, relative_length,
                          output, output_parsed);
}

bool ReplaceComponents(const char* spec,
                       int spec_len,
                       const url_parse::Parsed& parsed,
                       const url_canon::URLComponentSource<char>& replacements,
                       url_canon::CanonOutput* output,
                       url_parse::Parsed* out_parsed) {
  int scheme_len = 0;
  if (replacements.scheme)
    scheme_len = static_cast<int>(strlen(replacements.scheme));

  // Note that we dispatch to the parser according the the scheme type of
  // the OUTPUT URL. Normally, this is the same as our scheme, but if the
  // scheme is being overridden, we need to test that.

  if (// Either the scheme is not replaced and the old one is a file,
      (!replacements.scheme &&
       CompareSchemeComponent(spec, parsed.scheme, kFileScheme)) ||
      // Or it is being replaced and the new one is a file.
      (replacements.scheme && scheme_len > 0 &&
       CompareSchemeComponent(replacements.scheme,
                              url_parse::Component(0, scheme_len),
                              kFileScheme))) {
    return url_canon::ReplaceFileURL(spec, spec_len, parsed, replacements,
                                     output, out_parsed);
  }

  if (// Either the scheme is not replaced and the old one is standard,
      (!replacements.scheme &&
       IsStandardScheme(&spec[parsed.scheme.begin], parsed.scheme.len)) ||
      // Or it is being replaced and the new one is standard.
      (replacements.scheme && scheme_len > 0 &&
       IsStandardScheme(replacements.scheme, scheme_len))) {
    // Standard URL with all parts.
    return url_canon::ReplaceStandardURL(spec, spec_len, parsed, replacements,
                                         output, out_parsed);

  }

  return url_canon::ReplacePathURL(spec, spec_len, parsed, replacements,
                                   output, out_parsed);
}

}  // namespace url_util
