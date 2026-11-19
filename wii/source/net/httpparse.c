#include "net/httpparse.h"

#include <string.h>
#include <strings.h>

#define PREFIX "HTTP/1."
#define PREFIX_LEN 7
#define STATUS_LINE_MIN 12
#define DIGITS_MAX 18

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_blank(char c)
{
    return c == ' ' || c == '\t';
}

static const char *find_crlf(const char *p, const char *stop)
{
    while (p + 1 < stop && !(p[0] == '\r' && p[1] == '\n')) p++;
    return p + 1 < stop ? p : NULL;
}

static bool head_end(const char *buf, size_t len, size_t *end)
{
    for (size_t i = 0; i + 4 <= len; i++) {
        if (memcmp(buf + i, "\r\n\r\n", 4) == 0) {
            *end = i + 4;
            return true;
        }
    }
    return false;
}

static bool equals(const char *s, size_t n, const char *word)
{
    return strlen(word) == n && strncasecmp(s, word, n) == 0;
}

static bool parse_number(const char *s, size_t n, int64_t *out)
{
    if (n == 0 || n > DIGITS_MAX) return false;
    int64_t v = 0;
    for (size_t i = 0; i < n; i++) {
        if (!is_digit(s[i])) return false;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return true;
}

static bool parse_range(const char *v, size_t n, hp_response *out)
{
    if (n < 6 || strncasecmp(v, "bytes ", 6) != 0) return false;
    v += 6;
    n -= 6;
    const char *slash = memchr(v, '/', n);
    if (slash == NULL) return false;
    size_t left = (size_t)(slash - v);
    size_t right = n - left - 1;
    if (!(right == 1 && slash[1] == '*') && !parse_number(slash + 1, right, &out->total)) return false;
    if (left == 1 && v[0] == '*') return true;
    const char *dash = memchr(v, '-', left);
    if (dash == NULL) return false;
    size_t first = (size_t)(dash - v);
    if (!parse_number(v, first, &out->range_start) || !parse_number(dash + 1, left - first - 1, &out->range_end)) return false;
    return out->range_start <= out->range_end && (out->total < 0 || out->range_end < out->total);
}

static void parse_connection(const char *v, size_t n, bool *saw_close, bool *saw_keep)
{
    size_t i = 0;
    while (i < n) {
        size_t j = i;
        while (j < n && v[j] != ',') j++;
        size_t a = i;
        size_t b = j;
        while (a < b && is_blank(v[a])) a++;
        while (b > a && is_blank(v[b - 1])) b--;
        if (equals(v + a, b - a, "close")) *saw_close = true;
        if (equals(v + a, b - a, "keep-alive")) *saw_keep = true;
        i = j + 1;
    }
}

static bool parse_header(const char *line, size_t n, hp_response *out, bool *saw_close, bool *saw_keep)
{
    const char *colon = memchr(line, ':', n);
    if (colon == NULL || colon == line) return false;
    size_t name_len = (size_t)(colon - line);
    for (size_t i = 0; i < name_len; i++) {
        if (is_blank(line[i])) return false;
    }
    const char *v = colon + 1;
    size_t v_len = n - name_len - 1;
    while (v_len > 0 && is_blank(v[0])) {
        v++;
        v_len--;
    }
    while (v_len > 0 && is_blank(v[v_len - 1])) v_len--;

    if (equals(line, name_len, "content-length")) return parse_number(v, v_len, &out->content_length);
    if (equals(line, name_len, "content-range")) return parse_range(v, v_len, out);
    if (equals(line, name_len, "transfer-encoding")) return false;
    if (equals(line, name_len, "content-encoding")) out->gzip = equals(v, v_len, "gzip");
    if (equals(line, name_len, "connection")) parse_connection(v, v_len, saw_close, saw_keep);
    return true;
}

long hp_parse_head(const char *buf, size_t len, hp_response *out)
{
    if (memcmp(buf, PREFIX, len < PREFIX_LEN ? len : PREFIX_LEN) != 0) return -1;
    size_t end;
    if (!head_end(buf, len, &end)) return 0;

    const char *stop = buf + end - 2;
    const char *eol = find_crlf(buf, buf + end);
    size_t n = (size_t)(eol - buf);
    if (n < STATUS_LINE_MIN || !is_digit(buf[7]) || buf[8] != ' ' ||
        !is_digit(buf[9]) || !is_digit(buf[10]) || !is_digit(buf[11]) || (n > STATUS_LINE_MIN && buf[12] != ' ')) {
        return -1;
    }

    out->status = (buf[9] - '0') * 100 + (buf[10] - '0') * 10 + (buf[11] - '0');
    out->content_length = -1;
    out->range_start = -1;
    out->range_end = -1;
    out->total = -1;
    out->gzip = false;
    bool saw_close = false;
    bool saw_keep = buf[7] != '0';
    for (const char *line = eol + 2; line < stop; line = eol + 2) {
        eol = find_crlf(line, buf + end);
        if (!parse_header(line, (size_t)(eol - line), out, &saw_close, &saw_keep)) return -1;
    }
    out->keep_alive = saw_keep && !saw_close;
    return (long)end;
}
