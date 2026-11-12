/* GPL-3.0. The update check's pure parts: versions, the release JSON, SHA256SUMS.txt and SHA-256. */
#include "updater.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  assert(updater_compare("1.0.1", "1.0.0") > 0);
  assert(updater_compare("1.10.0", "1.9.9") > 0);
  assert(updater_compare("2.0.0", "2.0.0") == 0);
  assert(updater_compare("0.9.0", "1.0.0") < 0);
  assert(updater_compare("dev", "1.0.0") == 0); // not a version: never offered
  assert(updater_compare("1.0", "0.1.0") == 0);

  const char *json =
      "{\"url\":\"https://api.github.com/repos/kinetic-hq/pokedaisy/releases/1\","
      "\"html_url\":\"https://github.com/kinetic-hq/pokedaisy/releases/tag/desktop-v1.2.3\","
      "\"author\":{\"html_url\":\"https://github.com/someone\"},"
      "\"tag_name\": \"desktop-v1.2.3\",\"assets\":["
      "{\"name\":\"SHA256SUMS.txt\",\"browser_download_url\":"
      "\"https://github.com/kinetic-hq/pokedaisy/releases/download/desktop-v1.2.3/SHA256SUMS.txt\"},"
      "{\"name\":\"pokedaisy-linux-x86_64.zip\",\"browser_download_url\":"
      "\"https://github.com/kinetic-hq/pokedaisy/releases/download/desktop-v1.2.3/pokedaisy-linux-x86_64.zip\"},"
      "{\"name\":\"pokedaisy-windows-x86_64.zip\",\"browser_download_url\":"
      "\"https://github.com/kinetic-hq/pokedaisy/releases/download/desktop-v1.2.3/pokedaisy-windows-x86_64.zip\"}]}";
  char version[32], zip[512], sums[512], page[512];
  assert(updater_parse(json, version, sizeof version, zip, sizeof zip, sums, sizeof sums, page, sizeof page));
  assert(!strcmp(version, "1.2.3"));
  assert(!strcmp(page, "https://github.com/kinetic-hq/pokedaisy/releases/tag/desktop-v1.2.3"));
  assert(strstr(zip, "/pokedaisy-windows-x86_64.zip"));
  assert(strstr(sums, "/SHA256SUMS.txt"));
  // The Android app's tags (v*) and anything off GitHub are never taken.
  assert(!updater_parse("{\"tag_name\":\"v1.5.0\",\"html_url\":\"https://github.com/x\"}", version, sizeof version,
                        zip, sizeof zip, sums, sizeof sums, page, sizeof page));
  assert(!updater_parse("{\"tag_name\":\"desktop-v1.0.0\",\"html_url\":\"https://evil.example/x\"}", version,
                        sizeof version, zip, sizeof zip, sums, sizeof sums, page, sizeof page));
  assert(!updater_parse("{\"tag_name\":\"desktop-v1.0.0\",\"html_url\":\"https://github.com/a b\"}", version,
                        sizeof version, zip, sizeof zip, sums, sizeof sums, page, sizeof page));

  // The releases list: the highest desktop-v* wins, pre-releases included, out of order; the Android app's v* tags,
  // drafts and a body with braces in it don't get in the way.
  const char *releases =
      "[{\"html_url\":\"https://github.com/kinetic-hq/pokedaisy/releases/tag/v9.9.9\",\"tag_name\":\"v9.9.9\","
      "\"prerelease\":false,\"assets\":[]},"
      "{\"html_url\":\"https://github.com/kinetic-hq/pokedaisy/releases/tag/desktop-v1.9.0\","
      "\"tag_name\":\"desktop-v1.9.0\",\"draft\":false,\"prerelease\":true,\"body\":\"a } and a { \\\" in it\","
      "\"assets\":[{\"browser_download_url\":"
      "\"https://github.com/kinetic-hq/pokedaisy/releases/download/desktop-v1.9.0/pokedaisy-windows-x86_64.zip\"}]},"
      "{\"html_url\":\"https://github.com/kinetic-hq/pokedaisy/releases/tag/desktop-v1.10.0\","
      "\"tag_name\":\"desktop-v1.10.0\",\"draft\":true,\"prerelease\":true,\"assets\":[]},"
      "{\"html_url\":\"https://github.com/kinetic-hq/pokedaisy/releases/tag/desktop-v1.8.0\","
      "\"tag_name\":\"desktop-v1.8.0\",\"prerelease\":false,\"assets\":[]}]";
  assert(updater_parse_list(releases, version, sizeof version, zip, sizeof zip, sums, sizeof sums, page, sizeof page));
  assert(!strcmp(version, "1.9.0"));
  assert(strstr(zip, "desktop-v1.9.0/pokedaisy-windows-x86_64.zip") && !sums[0]);
  assert(!strcmp(page, "https://github.com/kinetic-hq/pokedaisy/releases/tag/desktop-v1.9.0"));
  assert(!updater_parse_list("[]", version, sizeof version, zip, sizeof zip, sums, sizeof sums, page, sizeof page));
  assert(!updater_parse_list("[{\"tag_name\":\"v1.0.0\",\"html_url\":\"https://github.com/x\"}]", version,
                             sizeof version, zip, sizeof zip, sums, sizeof sums, page, sizeof page));
  // A single release object (the old /releases/latest answer) still reads.
  assert(updater_parse_list(json, version, sizeof version, zip, sizeof zip, sums, sizeof sums, page, sizeof page) &&
         !strcmp(version, "1.2.3"));

  char hex[65];
  const char *list = "0000000000000000000000000000000000000000000000000000000000000000  pokedaisy-linux-x86_64.zip\n"
                     "ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef0123456789  pokedaisy-windows-x86_64.zip\n";
  assert(updater_sum_for(list, "pokedaisy-windows-x86_64.zip", hex));
  assert(!strcmp(hex, "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));
  assert(!updater_sum_for(list, "pokedaisy-windows", hex));

  updater_sha256((const unsigned char *)"", 0, hex);
  assert(!strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  updater_sha256((const unsigned char *)"abc", 3, hex);
  assert(!strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"; // 56 bytes: the padding's second block
  updater_sha256((const unsigned char *)two, strlen(two), hex);
  assert(!strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
  puts("PASS updater versions, release JSON, checksums");
  return 0;
}
