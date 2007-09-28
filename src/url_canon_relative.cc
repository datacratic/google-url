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

// Canonicalizer functions for working with and resolving relative URLs.

#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_canon_internal.h"
#include "googleurl/src/url_file.h"
#include "googleurl/src/url_parse_internal.h"

namespace url_canon {

namespace {

// Firefox does a case-sensitive compare (which is probably wrong--Mozilla bug
// 379034), whereas IE is case-insensetive.
//
// We choose to be more permissive like IE. We don't need to worry about
// unescaping or anything here: neither IE or Firefox allow this. We also
// don't have to worry about invalid scheme characters since we are comparing
// against the canonical scheme of the base.
//
// The base URL should always be canonical, therefore is ASCII.
template<typename CHAR>
bool AreSchemesEqual(const char* base,
                     const url_parse::Component& base_scheme,
                     const CHAR* cmp,
                     const url_parse::Component& cmp_scheme) {
  if (base_scheme.len != cmp_scheme.len)
    return false;
  for (int i = 0; i < base_scheme.len; i++) {
    // We assume the base is already canonical, so we don't have to
    // canonicalize it.
    if (CanonicalSchemeChar(cmp[cmp_scheme.begin + i]) !=
        base[base_scheme.begin + i])
      return false;
  }
  return true;
}

#ifdef WIN32

// Here, we also allow Windows paths to be represented as "/C:/" so we can be
// consistent about URL paths beginning with slashes. This function is like
// DoesBeginWindowsDrivePath except that it also requires a slash at the
// beginning.
template<typename CHAR>
bool DoesBeginSlashWindowsDriveSpec(const CHAR* spec, int start_offset,
                                    int spec_len) {
  if (start_offset >= spec_len)
    return false;
  return url_parse::IsURLSlash(spec[start_offset]) &&
      url_parse::DoesBeginWindowsDriveSpec(spec, start_offset + 1, spec_len);
}

#endif  // WIN32

// See IsRelativeURL in the header file for usage.
template<typename CHAR>
bool DoIsRelativeURL(const char* base,
                     const url_parse::Parsed& base_parsed,
                     const CHAR* url,
                     int url_len,
                     bool is_base_hierarchical,
                     bool* is_relative,
                     url_parse::Component* relative_component) {
  *is_relative = false;  // So we can default later to not relative.

  // Trim whitespace and construct a new range for the substring.
  int begin = 0;
  url_parse::TrimURL(url, &begin, &url_len);
  if (begin >= url_len) {
    // Empty URLs are relative, but do nothing.
    *relative_component = url_parse::Component(begin, 0);
    *is_relative = true;
    return true;
  }

#ifdef WIN32
  // We special case paths like "C:\foo" so they can link directly to the
  // file on Windows (IE compatability). The security domain stuff should
  // prevent a link like this from actually being followed if its on a
  // web page.
  //
  // We treat "C:/foo" as an absolute URL. We can go ahead and treat "/c:/"
  // as relative, as this will just replace the path and the answer will
  // still be correct.
  if (url_parse::DoesBeginWindowsDriveSpec(url, begin, url_len))
    return true;
#endif  // WIN32

  // Beginning with a slash means for sure this scheme is relative. We do
  // this before checking the scheme in case we have input like "/foo:bar"
  // that the scheme finder will think is a scheme.
  if (begin < url_len && url_parse::IsURLSlash(url[begin])) {
    *relative_component = url_parse::MakeRange(begin, url_len);
    *is_relative = true;
    return true;
  }

  // See if we've got a scheme, if not, we know this is a relative URL.
  // BUT: Just because we have a scheme, doesn't make it absolute.
  // "http:foo.html" is a relative URL with path "foo.html".
  url_parse::Component scheme;
  if (!url_parse::ExtractScheme(url, url_len, &scheme)) {
    // Don't allow relative URLs if the base scheme doesn't support it.
    if (!is_base_hierarchical)
      return false;

    *relative_component = url_parse::MakeRange(begin, url_len);
    *is_relative = true;
    return true;
  }

  // If the scheme is not the same, then we can't count it as relative.
  if (!AreSchemesEqual(base, base_parsed.scheme, url, scheme))
    return true;

  // When the scheme that they both share is not hierarchical, treat the
  // incoming scheme as absolute (this way with the base of "data:foo",
  // "data:bar" will be reported as absolute.
  if (!is_base_hierarchical)
    return true;

  // ExtractScheme guarantees that the colon immediately follows what it
  // considers to be the scheme. CountConsecutiveSlashes will handle the
  // case where the begin offset is the end of the input.
  int colon_offset = scheme.end();
  int num_slashes = url_parse::CountConsecutiveSlashes(url, colon_offset + 1,
                                                       url_len);

  if (num_slashes == 0 || num_slashes == 1) {
    // No slashes means it's a relative path like "http:foo.html". One slash
    // is an absolute path. "http:/home/foo.html"
    *is_relative = true;
    *relative_component = url_parse::MakeRange(colon_offset + 1, url_len);
    return true;
  }

  // Two or more slashes after the scheme we treat as absolute.
  return true;
}

// Copies all characters in the range [begin, end) of |spec| to the output,
// up until and including the last slash. There should be a slash in the
// range, if not, nothing will be copied.
//
// The input is assumed to be canonical, so we search only for exact slashes
// and not backslashes as well. We also know that it's ASCII.
void CopyToLastSlash(const char* spec,
                     int begin,
                     int end,
                     CanonOutput* output) {
  // Find the last slash.
  int last_slash = -1;
  for (int i = end - 1; i >= begin; i--) {
    if (spec[i] == '/') {
      last_slash = i;
      break;
    }
  }
  if (last_slash < 0)
    return;  // No slash.

  // Copy.
  for (int i = begin; i <= last_slash; i++)
    output->push_back(spec[i]);
}

// Copies a single component from the source to the output. This is used
// when resolving relative URLs and a given component is unchanged. Since the
// source should already be canonical, we don't have to do anything special,
// and the input is ASCII.
void CopyOneComponent(const char* source,
                      const url_parse::Component& source_component,
                      CanonOutput* output,
                      url_parse::Component* output_component) {
  if (source_component.len < 0) {
    // This component is not present.
    *output_component = url_parse::Component();
    return;
  }

  output_component->begin = output->length();
  int source_end = source_component.end();
  for (int i = source_component.begin; i < source_end; i++)
    output->push_back(source[i]);
  output_component->len = output->length() - output_component->begin;
}

#ifdef WIN32

// Called on Windows when the base URL is a file URL, this will copy the "C:"
// to the output, if there is a drive letter and if that drive letter is not
// being overridden by the relative URL. Otherwise, do nothing.
//
// It will return the index of the beginning of the next character in the
// base to be processed: if there is a "C:", the slash after it, or if
// there is no drive letter, the slash at the beginning of the path, or
// the end of the base. This can be used as the starting offset for further
// path processing.
template<typename CHAR>
int CopyBaseDriveSpecIfNecessary(const char* base_url,
                                 int base_path_begin,
                                 int base_path_end,
                                 const CHAR* relative_url,
                                 int path_start,
                                 int relative_url_len,
                                 CanonOutput* output) {
  if (base_path_begin >= base_path_end)
    return base_path_begin;  // No path.

  // If the relative begins with a drive spec, don't do anything. The existing
  // drive spec in the base will be replaced.
  if (url_parse::DoesBeginWindowsDriveSpec(relative_url,
                                           path_start, relative_url_len)) {
    return base_path_begin;  // Relative URL path is "C:/foo"
  }

  // The path should begin with a slash (as all canonical paths do). We check
  // if it is followed by a drive letter and copy it.
  if (DoesBeginSlashWindowsDriveSpec(base_url, base_path_begin, base_path_end)) {
    // Copy the two-character drive spec to the output. It will now look like
    // "file:///C:" so the rest of it can be treated like a standard path.
    output->push_back('/');
    output->push_back(base_url[base_path_begin + 1]);
    output->push_back(base_url[base_path_begin + 2]);
    return base_path_begin + 3;
  }

  return base_path_begin;
}

#endif  // WIN32

// TODO(brettw) treat two slashes as root like Mozilla for FTP?
template<typename CHAR>
bool DoResolveRelativeURL(const char* base_url,
                          const url_parse::Parsed& base_parsed,
                          bool base_is_file,
                          const CHAR* relative_url,
                          const url_parse::Component& relative_component,
                          CanonOutput* output,
                          url_parse::Parsed* out_parsed) {
  // Starting point for our output parsed. We'll fix what we change.
  *out_parsed = base_parsed;

  // Sanity check: the input should have a host or we'll break badly below.
  // We can only resolve relative URLs with base URLs that have hosts and
  // paths (even the default path of "/" is OK).
  //
  // We allow hosts with no length so we can handle file URLs, for example.
  if (base_parsed.host.len < 0 || base_parsed.path.len <= 0) {
    // On error, return the input (resolving a relative URL on a non-relative
    // base = the base).
    int base_len = base_parsed.Length();
    for (int i = 0; i < base_len; i++)
      output->push_back(base_url[i]);
    return false;
  }

  bool success = true;
  if (relative_component.len <= 0) {
    // Empty relative URL, make no changes.
    int base_len = base_parsed.Length();
    for (int i = 0; i < base_len; i++)
      output->push_back(base_url[i]);
    return true;
  }
  url_parse::Component path, query, ref;
  url_parse::ParsePathInternal(relative_url, relative_component, &path, &query, &ref);

  // When this function is called, we know the input URL is relative, so
  // we know the authority section didn't change, copy it to the output. We
  // also know we have a path so can copy up to there.
  for (int i = 0; i < base_parsed.path.begin; i++)
    output->push_back(base_url[i]);

  if (path.len > 0) {
    // The path is replaced or modified.
    int true_path_begin = output->length();

#ifdef WIN32
    // Special case relative URLs beginning with drive letters.
    if (url_parse::DoesBeginWindowsDriveSpec(relative_url,
                                             relative_component.begin,
                                             relative_component.end())) {
      // The relative URL is a drive letter like "C:\foo". We can just replace
      // the path and following items with those in the input. We need to use
      // the special file path canonicalizer so that the drive gets
      // canonicalized properly (uppercase letter and a colon instead of a
      // pipe).
      success &= FileCanonicalizePath(relative_url, path,
                                      output, &out_parsed->path);
    } else
#endif  // WIN32
    {
      // For file: URLs on Windows, we don't want to treat the drive letter and
      // colon as part of the path for relative file resolution when the
      // incoming URL does not provide a drive spec. We save the true path
      // beginning so we can fix it up after we are done.
      int base_path_begin = base_parsed.path.begin;
      if (base_is_file) {
        base_path_begin = CopyBaseDriveSpecIfNecessary(
            base_url, base_parsed.path.begin, base_parsed.path.end(),
            relative_url, relative_component.begin, relative_component.end(),
            output);
        // Now the output looks like either "file://" or "file:///C:"
        // and we can start appending the rest of the path. |base_path_begin|
        // points to the character in the base that comes next.
      }

      if (url_parse::IsURLSlash(relative_url[path.begin])) {
        // Easy case: the path is an absolute path on the server, so we can
        // just replace everything from the path on with the new versions.
        // Since the input should be canonical hierarchical URL, we should
        // always have a path.
        success &= CanonicalizePath(relative_url, path,
                                    output, &out_parsed->path);
      } else {
        // Relative path, replace the query, and reference. We take the
        // original path with the file part stripped, and append the new path.
        // The canonicalizer will take care of resolving ".." and "."
        int path_begin = output->length();
        CopyToLastSlash(base_url, base_path_begin, base_parsed.path.end(),
                        output);
        success &= CanonicalizePartialPath(relative_url, path, path_begin,
                                           output);
        out_parsed->path = url_parse::MakeRange(path_begin, output->length());

        // Copy the rest of the stuff after the path from the relative path.
      }
    }

    // Finish with the query and reference part (ignore failures for refs)
    CanonicalizeQuery(relative_url, query, NULL, output, &out_parsed->query);
    CanonicalizeRef(relative_url, ref, output, &out_parsed->ref);

    // Fix the path beginning to add back the "C:" we may have written above.
    out_parsed->path = url_parse::MakeRange(true_path_begin,
                                            out_parsed->path.end());
    return success;
  }

  // If we get here, the path is unchanged: copy to output.
  CopyOneComponent(base_url, base_parsed.path, output, &out_parsed->path);

  if (query.len >= 0) {
    // Just the query specified, replace the query and reference (ignore
    // failures for refs)
    CanonicalizeQuery(relative_url, query, NULL, output, &out_parsed->query);
    CanonicalizeRef(relative_url, ref, output, &out_parsed->ref);
    return success;
  }

  // If we get here, the query is unchanged: copy to output.
  CopyOneComponent(base_url, base_parsed.query, output, &out_parsed->query);

  if (ref.len >= 0) {
    // Just the reference specified: replace it (ignoring failures).
    CanonicalizeRef(relative_url, ref, output, &out_parsed->ref);
    return success;
  }

  // If we get here, the reference is unchanged: copy to output.
  CopyOneComponent(base_url, base_parsed.ref, output, &out_parsed->ref);
  return success;
}

}  // namespace

bool IsRelativeURL(const char* base,
                   const url_parse::Parsed& base_parsed,
                   const char* fragment,
                   int fragment_len,
                   bool is_base_hierarchical,
                   bool* is_relative,
                   url_parse::Component* relative_component) {
  return DoIsRelativeURL<char>(
      base, base_parsed, fragment, fragment_len, is_base_hierarchical,
      is_relative, relative_component);
}

bool IsRelativeURL(const char* base,
                   const url_parse::Parsed& base_parsed,
                   const wchar_t* fragment,
                   int fragment_len,
                   bool is_base_hierarchical,
                   bool* is_relative,
                   url_parse::Component* relative_component) {
  return DoIsRelativeURL<wchar_t>(
      base, base_parsed, fragment, fragment_len, is_base_hierarchical,
      is_relative, relative_component);
}

bool ResolveRelativeURL(const char* base_url,
                        const url_parse::Parsed& base_parsed,
                        bool base_is_file,
                        const char* relative_url,
                        const url_parse::Component& relative_component,
                        CanonOutput* output,
                        url_parse::Parsed* out_parsed) {
  return DoResolveRelativeURL<char>(
      base_url, base_parsed, base_is_file, relative_url,
      relative_component, output, out_parsed);
}

bool ResolveRelativeURL(const char* base_url,
                        const url_parse::Parsed& base_parsed,
                        bool base_is_file,
                        const wchar_t* relative_url,
                        const url_parse::Component& relative_component,
                        CanonOutput* output,
                        url_parse::Parsed* out_parsed) {
  return DoResolveRelativeURL<wchar_t>(
      base_url, base_parsed, base_is_file, relative_url,
      relative_component, output, out_parsed);
}

}  // namespace url_canon
