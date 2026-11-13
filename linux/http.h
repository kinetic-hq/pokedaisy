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
/* RetroAchievements' calls: url (http_url_ok) with post (NULL: a GET) of content type (NULL: a form), agent as the
 * User-Agent (printable ASCII; NULL: "PokeDaisy"). Any answer is returned, its status in *status, the body malloc'd
 * and NUL-ended (*size without the NUL); NULL when nothing came back or it was over max bytes. The post never
 * appears on a command line (curl reads it from a private temp file). */
char *http_post(const char *url, const char *post, const char *type, const char *agent, size_t max, size_t *size,
                int *status);
#endif
