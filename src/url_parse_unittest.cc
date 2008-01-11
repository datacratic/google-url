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

#include "base/basictypes.h"
#include "googleurl/src/url_parse.h"
#include "testing/base/gunit.h"

// Some implementations of base/basictypes.h may define ARRAYSIZE.
// If it's not defined, we define it to the ARRAYSIZE_UNSAFE macro
// which is in our version of basictypes.h.
#ifndef ARRAYSIZE
#define ARRAYSIZE ARRAYSIZE_UNSAFE
#endif

// Interesting IE file:isms...
//
//  file:/foo/bar              file:///foo/bar
//      The result here seems totally invalid!?!? This isn't UNC.
//
//  file:/
//  file:// or any other number of slashes
//      IE6 doesn't do anything at all if you click on this link. No error:
//      nothing. IE6's history system seems to always color this link, so I'm
//      guessing that it maps internally to the empty URL.
//
//  C:\                        file:///C:/
//  /                          file:///C:/
//  /foo                       file:///C:/foo
//      Interestingly, IE treats "/" as an alias for "c:\", which makes sense,
//      but is weird to think about on Windows.
//
//  file:foo/                  file:foo/  (invalid?!?!?)
//  file:/foo/                 file:///foo/  (invalid?!?!?)
//  file://foo/                file://foo/   (UNC to server "foo")
//  file:///foo/               file:///foo/  (invalid)
//  file:////foo/              file://foo/   (UNC to server "foo")
//      Any more than four slashes is also treated as UNC.
//
//  file:C:/                   file://C:/
//  file:/C:/                  file://C:/
//      The number of slashes after "file:" don't matter if the thing following
//      it looks like an absolute drive path. Also, slashes and backslashes are
//      equally valid here.

namespace {

// Used for regular URL parse cases.
struct URLParseCase {
  const char* input;

  const char* scheme;
  const char* username;
  const char* password;
  const char* host;
  int port;
  const char* path;
  const char* query;
  const char* ref;
};

// Simpler version of the above for testing path URLs.
struct PathURLParseCase {
  const char* input;

  const char* scheme;
  const char* path;
};

bool ComponentMatches(const char* input,
                      const char* reference,
                      const url_parse::Component& component) {
  // If the component is nonexistant (length == -1), it should begin at 0.
  EXPECT_TRUE(component.len >= 0 || component.len == -1);

  // Begin should be valid.
  EXPECT_LE(0, component.begin);

  // A NULL reference means the component should be nonexistant.
  if (!reference)
    return component.len == -1;
  if (component.len < 0)
    return false;  // Reference is not NULL but we don't have anything

  if (strlen(reference) != component.len)
    return false;  // Lengths don't match

  // Now check the actual characters.
  return strncmp(reference, &input[component.begin], component.len) == 0;
}

}  // namespace

// Standard -------------------------------------------------------------------

// Input                               Scheme  Usrname Passwd     Host         Port Path       Query        Ref
// ------------------------------------ ------- ------- ---------- ------------ --- ---------- ------------ -----
static URLParseCase cases[] = {
  // Regular URL with all the parts
{"http://user:pass@foo:21/bar;par?b#c", "http", "user", "pass",    "foo",       21, "/bar;par","b",          "c"},

  // Known schemes should lean towards authority identification
{"http:foo.com",                        "http", NULL,  NULL,      "foo.com",    -1, NULL,      NULL,        NULL},

  // Spaces!
{"\t   :foo.com   \n",                  "",     NULL,  NULL,      "foo.com",    -1, NULL,      NULL,        NULL},
{" foo.com  ",                          NULL,   NULL,  NULL,      "foo.com",    -1, NULL,      NULL,        NULL},
{"a:\t foo.com",                        "a",    NULL,  NULL,      "\t foo.com", -1, NULL,      NULL,        NULL},
{"http://f:21/ b ? d # e ",             "http", NULL,  NULL,      "f",          21, "/ b ",    " d ",       " e"},

  // Invalid port numbers should be identified and turned into -2, empty port
  // numbers should be -1. Spaces aren't allowed in port numbers
{"http://f:/c",                         "http", NULL,  NULL,      "f",          -1, "/c",      NULL,        NULL},
{"http://f:0/c",                        "http", NULL,  NULL,      "f",           0, "/c",      NULL,        NULL},
{"http://f:00000000000000/c",           "http", NULL,  NULL,      "f",           0, "/c",      NULL,        NULL},
{"http://f:00000000000000000000080/c",  "http", NULL,  NULL,      "f",          80, "/c",      NULL,        NULL},
{"http://f:b/c",                        "http", NULL,  NULL,      "f",          -2, "/c",      NULL,        NULL},
{"http://f: /c",                        "http", NULL,  NULL,      "f",          -2, "/c",      NULL,        NULL},
{"http://f:\n/c",                       "http", NULL,  NULL,      "f",          -2, "/c",      NULL,        NULL},
{"http://f:fifty-two/c",                "http", NULL,  NULL,      "f",          -2, "/c",      NULL,        NULL},
{"http://f:999999/c",                   "http", NULL,  NULL,      "f",          -2, "/c",      NULL,        NULL},
{"http://f: 21 / b ? d # e ",           "http", NULL,  NULL,      "f",          -2, "/ b ",    " d ",       " e"},

  // Creative URLs missing key elements
{"",                                    NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{"  \t",                                NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{":foo.com/",                           "",     NULL,  NULL,      "foo.com",    -1, "/",       NULL,        NULL},
{":foo.com\\",                          "",     NULL,  NULL,      "foo.com",    -1, "\\",      NULL,        NULL},
{":",                                   "",     NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{":a",                                  "",     NULL,  NULL,      "a",          -1, NULL,      NULL,        NULL},
{":/",                                  "",     NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{":\\",                                 "",     NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{":#",                                  "",     NULL,  NULL,      "",           -1, NULL,      NULL,        ""},
{"#",                                   NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        ""},
{"#/",                                  NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        "/"},
{"#\\",                                 NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        "\\"},
{"#;?",                                 NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        ";?"},
{"?",                                   NULL,   NULL,  NULL,      "",           -1, NULL,      "",          NULL},
{"/",                                   NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{":23",                                 "",     NULL,  NULL,      "23",         -1, NULL,      NULL,        NULL},
{"/:23",                                NULL,   NULL,  NULL,      "",           23, NULL,      NULL,        NULL},
{"//",                                  NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{"/:",                                  NULL,   NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{"::",                                  "",     NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{"::23",                                "",     NULL,  NULL,      "",           23, NULL,      NULL,        NULL},
{"foo://",                              "foo",  NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},

  // Username/passwords and things that look like them
{"http://a:b@c:29/d",                   "http", "a",   "b",       "c",          29, "/d",      NULL,        NULL},
{"http::@c:29",                         "http", "",    "",        "c",          29, NULL,      NULL,        NULL},
  // ... "]" in the password field isn't allowed, but we tolerate it here...
{"http://&a:foo(b]c@d:2/",              "http", "&a",  "foo(b]c", "d",           2, "/",       NULL,        NULL},
{"http://::@c@d:2",                     "http", "",    ":@c",     "d",           2, NULL,      NULL,        NULL},
{"http://foo.com:b@d/",                 "http", "foo.com", "b",   "d",          -1, "/",       NULL,        NULL},

{"http://foo.com/\\@",                  "http", NULL,  NULL,      "foo.com",    -1, "/\\@",    NULL,        NULL},
{"http:\\\\foo.com\\",                  "http", NULL,  NULL,      "foo.com",    -1, "\\",      NULL,        NULL},
{"http:\\\\a\\b:c\\d@foo.com\\",        "http", NULL,  NULL,      "a",          -1, "\\b:c\\d@foo.com\\", NULL,   NULL},

  // Tolerate different numbers of slashes.
{"foo:/",                               "foo",  NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{"foo:/bar.com/",                       "foo",  NULL,  NULL,      "bar.com",    -1, "/",       NULL,        NULL},
{"foo://///////",                       "foo",  NULL,  NULL,      "",           -1, NULL,      NULL,        NULL},
{"foo://///////bar.com/",               "foo",  NULL,  NULL,      "bar.com",    -1, "/",       NULL,        NULL},
{"foo:////://///",                      "foo",  NULL,  NULL,      "",           -1, "/////",   NULL,        NULL},

  // Raw file paths on Windows aren't handled by the parser.
{"c:/foo",                              "c",    NULL,  NULL,      "foo",        -1, NULL,      NULL,        NULL},
{"//foo/bar",                           NULL,   NULL,  NULL,      "foo",        -1, "/bar",    NULL,        NULL},

  // Use the first question mark for the query and the ref.
{"http://foo/path;a??e#f#g",            "http", NULL,  NULL,      "foo",        -1, "/path;a", "?e",      "f#g"},
{"http://foo/abcd?efgh?ijkl",           "http", NULL,  NULL,      "foo",        -1, "/abcd",   "efgh?ijkl", NULL},
{"http://foo/abcd#foo?bar",             "http", NULL,  NULL,      "foo",        -1, "/abcd",   NULL,        "foo?bar"},

  // IPV6, check also interesting uses of colons.
{"[61:24:74]:98",                       NULL,   NULL,  NULL,      "[61:24:74]", 98, NULL,      NULL,        NULL},
{"http://[61:27]:98",                   "http", NULL,  NULL,      "[61:27]",    98, NULL,      NULL,        NULL},
{"http:[61:27]/:foo",                   "http", NULL,  NULL,      "[61:27]",    -1, "/:foo",   NULL,        NULL},

};

TEST(URLParser, Standard) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the construtor.
  url_parse::Parsed parsed;
  for (int i = 0; i < arraysize(cases); i++) {
    const char* url = cases[i].input;
    url_parse::ParseStandardURL(url, static_cast<int>(strlen(url)), &parsed);
    int port = url_parse::ParsePort(url, parsed.port);

    EXPECT_TRUE(ComponentMatches(url, cases[i].scheme, parsed.scheme));
    EXPECT_TRUE(ComponentMatches(url, cases[i].username, parsed.username));
    EXPECT_TRUE(ComponentMatches(url, cases[i].password, parsed.password));
    EXPECT_TRUE(ComponentMatches(url, cases[i].host, parsed.host));
    EXPECT_EQ(cases[i].port, port);
    EXPECT_TRUE(ComponentMatches(url, cases[i].path, parsed.path));
    EXPECT_TRUE(ComponentMatches(url, cases[i].query, parsed.query));
    EXPECT_TRUE(ComponentMatches(url, cases[i].ref, parsed.ref));
  }
}

// PathURL --------------------------------------------------------------------

// Various incarnations of path URLs.
static PathURLParseCase path_cases[] = {
{"",                                        NULL,          NULL},
{":",                                       "",            NULL},
{":/",                                      "",            "/"},
{"/",                                       NULL,          "/"},
{" This is \\interesting// \t",             NULL,          "This is \\interesting//"},
{"about:",                                  "about",       NULL},
{"about:blank",                             "about",       "blank"},
{"  about: blank ",                         "about",       " blank"},
{"javascript :alert(\"He:/l\\l#o?foo\"); ", "javascript ", "alert(\"He:/l\\l#o?foo\");"},
};

TEST(URLParser, PathURL) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the construtor.
  url_parse::Parsed parsed;
  for (int i = 0; i < arraysize(path_cases); i++) {
    const char* url = path_cases[i].input;
    url_parse::ParsePathURL(url, static_cast<int>(strlen(url)), &parsed);

    EXPECT_TRUE(ComponentMatches(url, path_cases[i].scheme, parsed.scheme));
    EXPECT_TRUE(ComponentMatches(url, path_cases[i].path, parsed.path));

    EXPECT_EQ(0, parsed.username.begin);
    EXPECT_EQ(-1, parsed.username.len);

    EXPECT_EQ(0, parsed.password.begin);
    EXPECT_EQ(-1, parsed.password.len);

    // Hosts have 0, not -1 when not present.
    EXPECT_EQ(0, parsed.host.begin);
    EXPECT_EQ(0, parsed.host.len);

    EXPECT_EQ(0, parsed.port.begin);
    EXPECT_EQ(-1, parsed.port.len);

    EXPECT_EQ(0, parsed.query.begin);
    EXPECT_EQ(-1, parsed.query.len);

    EXPECT_EQ(0, parsed.ref.begin);
    EXPECT_EQ(-1, parsed.ref.len);
  }
}

#ifdef WIN32

// WindowsFile ----------------------------------------------------------------

// Various incarnations of file URLs. These are for Windows only.
static URLParseCase file_cases[] = {
{"file:server",              "file", NULL, NULL, "server", -1, NULL,          NULL, NULL},
{"  file: server  \t",       "file", NULL, NULL, " server",-1, NULL,          NULL, NULL},
{"FiLe:c|",                  "FiLe", NULL, NULL, "",       -1, "c|",          NULL, NULL},
{"FILE:/\\\\/server/file",   "FILE", NULL, NULL, "server", -1, "/file",       NULL, NULL},
{"file://server/",           "file", NULL, NULL, "server", -1, "/",           NULL, NULL},
{"file://localhost/c:/",     "file", NULL, NULL, "",       -1, "/c:/",        NULL, NULL},
{"file://127.0.0.1/c|\\",    "file", NULL, NULL, "",       -1, "/c|\\",       NULL, NULL},
{"file:/",                   "file", NULL, NULL, "",       -1, NULL,          NULL, NULL},
{"file:",                    "file", NULL, NULL, "",       -1, NULL,          NULL, NULL},
  // If there is a Windows drive letter, treat any number of slashes as the
  // path part.
{"file:c:\\fo\\b",           "file", NULL, NULL, "",       -1, "c:\\fo\\b",   NULL, NULL},
{"file:/c:\\foo/bar",        "file", NULL, NULL, "",       -1, "/c:\\foo/bar",NULL, NULL},
{"file://c:/f\\b",           "file", NULL, NULL, "",       -1, "/c:/f\\b",    NULL, NULL},
{"file:///C:/foo",           "file", NULL, NULL, "",       -1, "/C:/foo",     NULL, NULL},
{"file://///\\/\\/c:\\f\\b", "file", NULL, NULL, "",       -1, "/c:\\f\\b",   NULL, NULL},
  // If there is not a drive letter, we should treat is as UNC EXCEPT for
  // three slashes, which we treat as a Unix style path.
{"file:server/file",         "file", NULL, NULL, "server", -1, "/file",       NULL, NULL},
{"file:/server/file",        "file", NULL, NULL, "server", -1, "/file",       NULL, NULL},
{"file://server/file",       "file", NULL, NULL, "server", -1, "/file",       NULL, NULL},
{"file:///server/file",      "file", NULL, NULL, "",       -1, "/server/file",NULL, NULL},
{"file://\\server/file",     "file", NULL, NULL, "",       -1, "\\server/file",NULL, NULL},
{"file:////server/file",     "file", NULL, NULL, "server", -1, "/file",       NULL, NULL},
  // Queries and refs are valid for file URLs as well.
{"file:///C:/foo.html?#",   "file", NULL, NULL, "",       -1, "/C:/foo.html",  "",   ""},
{"file:///C:/foo.html?query=yes#ref", "file", NULL, NULL, "", -1, "/C:/foo.html", "query=yes", "ref"},
};

TEST(URLParser, WindowsFile) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the construtor.
  url_parse::Parsed parsed;
  for (int i = 0; i < arraysize(file_cases); i++) {
    const char* url = file_cases[i].input;
    url_parse::ParseFileURL(url, static_cast<int>(strlen(url)), &parsed);
    int port = url_parse::ParsePort(url, parsed.port);

    EXPECT_TRUE(ComponentMatches(url, file_cases[i].scheme, parsed.scheme));
    EXPECT_TRUE(ComponentMatches(url, file_cases[i].username, parsed.username));
    EXPECT_TRUE(ComponentMatches(url, file_cases[i].password, parsed.password));
    EXPECT_TRUE(ComponentMatches(url, file_cases[i].host, parsed.host));
    EXPECT_EQ(file_cases[i].port, port);
    EXPECT_TRUE(ComponentMatches(url, file_cases[i].path, parsed.path));
    EXPECT_TRUE(ComponentMatches(url, file_cases[i].query, parsed.query));
    EXPECT_TRUE(ComponentMatches(url, file_cases[i].ref, parsed.ref));
  }
}

#endif  // WIN32

TEST(URLParser, ExtractFileName) {
  struct FileCase {
    const char* input;
    const char* expected;
  } file_cases[] = {
    {"http://www.google.com", NULL},
    {"http://www.google.com/", ""},
    {"http://www.google.com/search", "search"},
    {"http://www.google.com/search/", ""},
    {"http://www.google.com/foo/bar.html?baz=22", "bar.html"},
    {"http://www.google.com/foo/bar.html#ref", "bar.html"},
    {"http://www.google.com/search/;param", ""},
    {"http://www.google.com/foo/bar.html;param#ref", "bar.html"},
    {"http://www.google.com/foo/bar.html;foo;param#ref", "bar.html;foo"},
    {"http://www.google.com/foo/bar.html?query#ref", "bar.html"},
  };

  for (int i = 0; i < ARRAYSIZE(file_cases); i++) {
    const char* url = file_cases[i].input;
    int len = static_cast<int>(strlen(url));

    url_parse::Parsed parsed;
    url_parse::ParseStandardURL(url, len, &parsed);

    url_parse::Component file_name;
    url_parse::ExtractFileName(url, parsed.path, &file_name);

    EXPECT_TRUE(ComponentMatches(url, file_cases[i].expected, file_name));
  }
}
