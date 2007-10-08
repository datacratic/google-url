// Copyright 2007 Google Inc. All Rights Reserved.
// Author: brettw@google.com (Brett Wilson)

#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "testing/base/gunit.h"

namespace {

template<typename CHAR>
void SetupReplacement(void (url_canon::Replacements<CHAR>::*func)(const CHAR*,
                          const url_parse::Component&),
                      url_canon::Replacements<CHAR>* replacements,
                      const CHAR* str) {
  if (str) {
    url_parse::Component comp;
    if (str[0])
      comp.len = static_cast<int>(strlen(str));
    (replacements->*func)(str, comp);
  }
}

}  // namespace

// Test the basic creation and querying of components in a GURL. We assume
// the parser is already tested and works, so we are mostly interested if the
// object does the right thing with the results.
TEST(GURLTest, Components) {
  GURL url(L"http://user:pass@google.com:99/foo;bar?q=a#ref");
  EXPECT_TRUE(url.is_valid());
  EXPECT_TRUE(url.SchemeIs("http"));
  EXPECT_FALSE(url.SchemeIsFile());

  // This is the narrow version of the URL, which should match the wide input.
  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url.spec());

  EXPECT_EQ("http", url.scheme());
  EXPECT_EQ("user", url.username());
  EXPECT_EQ("pass", url.password());
  EXPECT_EQ("google.com", url.host());
  EXPECT_EQ("99", url.port());
  EXPECT_EQ(99, url.IntPort());
  EXPECT_EQ("/foo;bar", url.path());
  EXPECT_EQ("q=a", url.query());
  EXPECT_EQ("ref", url.ref());
}

TEST(GURLTest, Empty) {
  GURL url;
  EXPECT_FALSE(url.is_valid());
  EXPECT_EQ("", url.spec());

  EXPECT_EQ("", url.scheme());
  EXPECT_EQ("", url.username());
  EXPECT_EQ("", url.password());
  EXPECT_EQ("", url.host());
  EXPECT_EQ("", url.port());
  EXPECT_EQ(url_parse::PORT_UNSPECIFIED, url.IntPort());
  EXPECT_EQ("", url.path());
  EXPECT_EQ("", url.query());
  EXPECT_EQ("", url.ref());
}

TEST(GURLTest, Copy) {
  GURL url(L"http://user:pass@google.com:99/foo;bar?q=a#ref");

  GURL url2(url);
  EXPECT_TRUE(url2.is_valid());

  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url2.spec());
  EXPECT_EQ("http", url2.scheme());
  EXPECT_EQ("user", url2.username());
  EXPECT_EQ("pass", url2.password());
  EXPECT_EQ("google.com", url2.host());
  EXPECT_EQ("99", url2.port());
  EXPECT_EQ(99, url2.IntPort());
  EXPECT_EQ("/foo;bar", url2.path());
  EXPECT_EQ("q=a", url2.query());
  EXPECT_EQ("ref", url2.ref());

  // Copying of invalid URL should be invalid
  GURL invalid;
  GURL invalid2(invalid);
  EXPECT_FALSE(invalid2.is_valid());
  EXPECT_EQ("", invalid2.spec());
  EXPECT_EQ("", invalid2.scheme());
  EXPECT_EQ("", invalid2.username());
  EXPECT_EQ("", invalid2.password());
  EXPECT_EQ("", invalid2.host());
  EXPECT_EQ("", invalid2.port());
  EXPECT_EQ(url_parse::PORT_UNSPECIFIED, invalid2.IntPort());
  EXPECT_EQ("", invalid2.path());
  EXPECT_EQ("", invalid2.query());
  EXPECT_EQ("", invalid2.ref());
}

// Given an invalid URL, we should still get most of the components.
TEST(GURLTest, Invalid) {
  GURL url("http:google.com:foo");
  EXPECT_FALSE(url.is_valid());
  EXPECT_EQ("http://google.com:foo/", url.possibly_invalid_spec());

  EXPECT_EQ("http", url.scheme());
  EXPECT_EQ("", url.username());
  EXPECT_EQ("", url.password());
  EXPECT_EQ("google.com", url.host());
  EXPECT_EQ("foo", url.port());
  EXPECT_EQ(url_parse::PORT_INVALID, url.IntPort());
  EXPECT_EQ("/", url.path());
  EXPECT_EQ("", url.query());
  EXPECT_EQ("", url.ref());
}

TEST(GURLTest, Resolve) {
  // The tricky cases for relative URL resolving are tested in the
  // canonicalizer unit test. Here, we just test that the GURL integration
  // works properly.
  struct ResolveCase {
    const char* base;
    const char* relative;
    bool expected_valid;
    const char* expected;
  } resolve_cases[] = {
    {"http://www.google.com/", "foo.html", true, "http://www.google.com/foo.html"},
    {"http://www.google.com/", "http://images.google.com/foo.html", true, "http://images.google.com/foo.html"},
    {"http://www.google.com/blah/bloo?c#d", "../../../hello/./world.html?a#b", true, "http://www.google.com/hello/world.html?a#b"},
    {"http://www.google.com/foo#bar", "#com", true, "http://www.google.com/foo#com"},
    {"http://www.google.com/", "Https:images.google.com", true, "https://images.google.com/"},
    {"data:blahblah", "http://google.com/", true, "http://google.com/"},
    {"data:blahblah", "http:google.com", true, "http://google.com/"},
    {"data:blahblah", "file.html", false, ""},
  };

  for (int i = 0; i < arraysize(resolve_cases); i++) {
    // 8-bit code path.
    GURL input(resolve_cases[i].base);
    GURL output = input.Resolve(resolve_cases[i].relative);
    EXPECT_EQ(resolve_cases[i].expected_valid, output.is_valid());
    EXPECT_EQ(resolve_cases[i].expected, output.spec());

    // Wide code path.
    GURL inputw(UTF8ToWide(resolve_cases[i].base));
    GURL outputw = input.Resolve(UTF8ToWide(resolve_cases[i].relative));
    EXPECT_EQ(resolve_cases[i].expected_valid, outputw.is_valid());
    EXPECT_EQ(resolve_cases[i].expected, outputw.spec());
  }
}

TEST(GURLTest, GetWithEmptyPath) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    {"http://www.google.com", "http://www.google.com/"},
    {"javascript:window.alert(\"hello, world\");", ""},
    {"http://www.google.com/foo/bar.html?baz=22", "http://www.google.com/"},
  };

  for (int i = 0; i < arraysize(cases); i++) {
    GURL url(cases[i].input);
    GURL empty_path = url.GetWithEmptyPath();
    EXPECT_EQ(cases[i].expected, empty_path.spec());
  }
}

TEST(GURLTest, Replacements) {
  // The url canonicalizer replacement test will handle most of these case.
  // The most important thing to do here is to check that the proper
  // canonicalizer gets called based on the scheme of the input.
  struct ReplaceCase {
    const char* base;
    const char* scheme;
    const char* username;
    const char* password;
    const char* host;
    const char* port;
    const char* path;
    const char* query;
    const char* ref;
    const char* expected;
  } replace_cases[] = {
    {"http://www.google.com/foo/bar.html?foo#bar", NULL, NULL, NULL, NULL, NULL, "/", "", "", "http://www.google.com/"},
    {"http://www.google.com/foo/bar.html?foo#bar", "javascript", "", "", "", "", "window.open('foo');", "", "", "javascript:window.open('foo');"},
    {"file:///C:/foo/bar.txt", "http", NULL, NULL, "www.google.com", "99", "/foo","search", "ref", "http://www.google.com:99/foo?search#ref"},
    {"http://www.google.com/foo/bar.html?foo#bar", "file", "", "", "", "", "c:\\", "", "", "file:///C:/"},
  };

  for (int i = 0; i < arraysize(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    GURL url(cur.base);
    GURL::Replacements repl;
    SetupReplacement(&GURL::Replacements::SetScheme, &repl, cur.scheme);
    SetupReplacement(&GURL::Replacements::SetUsername, &repl, cur.username);
    SetupReplacement(&GURL::Replacements::SetPassword, &repl, cur.password);
    SetupReplacement(&GURL::Replacements::SetHost, &repl, cur.host);
    SetupReplacement(&GURL::Replacements::SetPort, &repl, cur.port);
    SetupReplacement(&GURL::Replacements::SetPath, &repl, cur.path);
    SetupReplacement(&GURL::Replacements::SetQuery, &repl, cur.query);
    SetupReplacement(&GURL::Replacements::SetRef, &repl, cur.ref);
    GURL output = url.ReplaceComponents(repl);

    EXPECT_EQ(replace_cases[i].expected, output.spec());
  }
}

TEST(GURLTest, PathForRequest) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    {"http://www.google.com", "/"},
    {"http://www.google.com/", "/"},
    {"http://www.google.com/foo/bar.html?baz=22", "/foo/bar.html?baz=22"},
    {"http://www.google.com/foo/bar.html#ref", "/foo/bar.html"},
    {"http://www.google.com/foo/bar.html?query#ref", "/foo/bar.html?query"},
  };

  for (int i = 0; i < arraysize(cases); i++) {
    GURL url(cases[i].input);
    std::string path_request = url.PathForRequest();
    EXPECT_EQ(cases[i].expected, path_request);
  }
}

TEST(GURLTest, ExtractQuery) {
  GURL::QueryMap map;
  GURL::QueryMap::iterator i;

  // empty URL
  GURL a("http://www.google.com");
  a.ExtractQuery(&map);
  i = map.find("foo");
  EXPECT_TRUE(i == map.end());

  // simple case
  GURL b("http://www.google.com?arg1=1&arg2=2&bar");
  b.ExtractQuery(&map);
  EXPECT_EQ(map["arg1"], "1");
  EXPECT_EQ(map["arg2"], "2");
  EXPECT_EQ(map["bar"], "");

  // Various terminations
  const char* urls[] = {
    "http://www.google.com?foo=bar",
    "http://www.google.com?foo=bar&",
    "http://www.google.com?&foo=bar",
    "http://www.google.com?blaz&foo=bar",
    "http://www.google.com?blaz=&foo=bar"
  };

  for (int i = 0; i < arraysize(urls); ++i) {
    GURL c(urls[i]);
    c.ExtractQuery(&map);
    EXPECT_EQ(map["foo"], "bar");
  }

  const char* stress[] = {
    "http://www.google.com?&=",
    "http://www.google.com?&&=&",
    "http://www.google.com?=",
    "http://www.google.com?==",
    "http://www.google.com?==&&&="
  };
  for (int i = 0; i < arraysize(urls); ++i) {
    GURL d(urls[i]);
    d.ExtractQuery(&map);
  }
}

TEST(GURLTest, IPAddress) {
  struct IPTest {
    const char* spec;
    bool expected_ip;
  } ip_tests[] = {
    {"http://www.google.com/", false},
    {"http://192.168.9.1/", true},
    {"http://192.168.9.1.2/", false},
    {"http://192.168.m.1/", false},
    {"", false},
    {"some random input!", false},
  };

  for (int i = 0; i < arraysize(ip_tests); i++) {
    GURL url(ip_tests[i].spec);
    EXPECT_EQ(ip_tests[i].expected_ip, url.HostIsIPAddress());
  }
}
