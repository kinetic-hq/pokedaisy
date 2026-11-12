/* GPL-3.0, see ../NOTICE. A plain HTTPS GET for the updater and the box art: WinHTTP on Windows, curl elsewhere. */
#ifndef POCKET_HTTP_H
#define POCKET_HTTP_H
#include <stdbool.h>
#include <stddef.h>

/* Only https URLs of these characters are fetched (anything else is percent-encoded first: http_encode). */
bool http_url_ok(const char *url);
/* GET url (redirects followed) into a malloc'd, NUL-ended buffer, *size without the NUL; NULL on any failure, a
 * non-200 answer or more than max bytes. header: one extra "Name: value" line, or NULL. */
char *http_get(const char *url, const char *header, size_t max, size_t *size);
/* Percent-encodes s for a URL's path: everything but letters, digits and -._~ . */
void http_encode(const char *s, char *out, size_t size);
#endif
