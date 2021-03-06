/*
    $Id: file.c 1228 2016-07-09 18:28:35Z soci $

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/
#include "file.h"
#include <string.h>
#include "wchar.h"
#include <errno.h>
#ifdef _WIN32
#include <locale.h>
#endif
#include "64tass.h"
#include "unicode.h"
#include "error.h"
#include "strobj.h"
#include "arguments.h"

#define REPLACEMENT_CHARACTER 0xfffd

struct include_list_s {
    struct include_list_s *next;
#if __STDC_VERSION__ >= 199901L
    char path[];
#elif __GNUC__ >= 3
    char path[];
#else
    char path[1];
#endif
};

static struct include_list_s include_list;
struct include_list_s *include_list_last = &include_list;

static struct avltree file_tree;

void include_list_add(const char *path)
{
    size_t i, j, len;
    j = i = strlen(path);
    if (i == 0) return;
#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __MSDOS__ || defined __DOS__
    if (path[i - 1] != '/' && path[i-1] != '\\') j++;
#else
    if (path[i - 1] != '/') j++;
#endif
    len = j + 1 + sizeof(struct include_list_s);
    if (len < sizeof(struct include_list_s)) err_msg_out_of_memory();
    include_list_last->next = (struct include_list_s *)mallocx(len);
    include_list_last = include_list_last->next;
    include_list_last->next = NULL;
    memcpy(include_list_last->path, path, i + 1);
    if (i != j) memcpy(include_list_last->path + i, "/", 2);
}

char *get_path(const Str *v, const char *base) {
    char *path;
    size_t i, len;
#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __MSDOS__ || defined __DOS__
    size_t j;

    i = strlen(base);
    j = (((base[0] >= 'A' && base[0] <= 'Z') || (base[0] >= 'a' && base[0] <= 'z')) && base[1]==':') ? 2 : 0;
    while (i > j) {
        if (base[i - 1] == '/' || base[i - 1] == '\\') break;
        i--;
    }
#else
    const char *c;
    c = strrchr(base, '/');
    i = (c != NULL) ? (c - base + 1) : 0;
#endif

    if (v == NULL) {
        len = i + 1;
        if (len < 1) err_msg_out_of_memory(); /* overflow */
        path = (char *)mallocx(len);
        memcpy(path, base, i);
        path[i] = 0;
        return path;
    }

#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __MSDOS__ || defined __DOS__
    if (v->len != 0 && (v->data[0] == '/' || v->data[0] == '\\')) i = j;
    else if (v->len > 1 && ((v->data[0] >= 'A' && v->data[0] <= 'Z') || (v->data[0] >= 'a' && v->data[0] <= 'z')) && v->data[1]==':') i = 0;
#else
    if (v->len != 0 && v->data[0] == '/') i = 0;
#endif
    len = i + v->len;
    if (len < i) err_msg_out_of_memory(); /* overflow */
    len += 1;
    if (len < 1) err_msg_out_of_memory(); /* overflow */
    path = (char *)mallocx(len);
    memcpy(path, base, i);
    memcpy(path + i, v->data, v->len);
    path[i + v->len] = 0;
    return path;
}

FILE *file_open(const char *name, const char *mode)
{
    FILE *f;
#ifdef _WIN32
    wchar_t *wname, *c2, wmode[3];
    const uint8_t *c;
    uint32_t ch;
    size_t len = strlen(name) + 1;
    if (len > SIZE_MAX / sizeof *wname) err_msg_out_of_memory();
    wname = (wchar_t *)mallocx(len * sizeof *wname);
    c2 = wname; c = (uint8_t *)name;
    while (*c != 0) {
        ch = *c;
        if ((ch & 0x80) != 0) c += utf8in(c, &ch); else c++;
        if (ch < 0x10000) *c2++ = ch;
        else if (ch < 0x110000) {
            *c2++ = (ch >> 10) + 0xd7c0;
            *c2++ = (ch & 0x3ff) | 0xdc00;
        } else *c2++ = REPLACEMENT_CHARACTER;
    }
    *c2++ = 0;
    c2 = wmode; c = (uint8_t *)mode;
    while ((*c2++=(wchar_t)*c++) != 0);
    f = _wfopen(wname, wmode);
    free(wname);
#else
    size_t len = 0, max = strlen(name) + 1;
    char *newname = (char *)mallocx(max);
    const uint8_t *c = (uint8_t *)name;
    uint32_t ch;
    mbstate_t ps;
    memset(&ps, 0, sizeof ps);
    if (max < 1) err_msg_out_of_memory();
    do {
        char temp[64];
        int l;
        ch = *c;
        if ((ch & 0x80) != 0) c += utf8in(c, &ch); else c++;
        l = wcrtomb(temp, (wchar_t)ch, &ps);
        if (l <= 0) l = sprintf(temp, "{$%" PRIx32 "}", ch);
        len += l;
        if (len < (size_t)l) err_msg_out_of_memory();
        if (len > max) {
            max = len + 64;
            if (max < 64) err_msg_out_of_memory();
            newname = (char *)reallocx(newname, max);
        }
        memcpy(newname + len - l, temp, l);
    } while (ch != 0);
    f = fopen(newname, mode);
    free(newname);
#endif
    return f;
}

static int star_compare(const struct avltree_node *aa, const struct avltree_node *bb)
{
    const struct star_s *a = cavltree_container_of(aa, struct star_s, node);
    const struct star_s *b = cavltree_container_of(bb, struct star_s, node);

    return a->line - b->line;
}

static int file_compare(const struct avltree_node *aa, const struct avltree_node *bb)
{
    const struct file_s *a = cavltree_container_of(aa, struct file_s, node);
    const struct file_s *b = cavltree_container_of(bb, struct file_s, node);
    int c = strcmp(a->name, b->name);
    if (c != 0) return c;
    return strcmp(a->base, b->base);
}

static void star_free(struct avltree_node *aa)
{
    struct star_s *a = avltree_container_of(aa, struct star_s, node);

    avltree_destroy(&a->tree, star_free);
}

static void file_free(struct avltree_node *aa)
{
    struct file_s *a = avltree_container_of(aa, struct file_s, node);

    avltree_destroy(&a->star, star_free);
    free(a->data);
    free(a->line);
    free((char *)a->name);
    free((char *)a->realname);
    free((char *)a->base);
    free(a);
}

static uint8_t *flushubuff(struct ubuff_s *ubuff, uint8_t *p, struct file_s *tmp) {
    size_t i;
    if (ubuff->data == NULL) {
        ubuff->len = 16;
        ubuff->data = (uint32_t *)mallocx(16 * sizeof *ubuff->data);
        return p;
    }
    for (i = 0; i < ubuff->p; i++) {
        size_t o = p - tmp->data;
        uint32_t ch;
        if (o + 6*6 + 1 > tmp->len) {
            tmp->len += 4096;
            if (tmp->len < 4096) err_msg_out_of_memory(); /* overflow */
            tmp->data = (uint8_t *)reallocx(tmp->data, tmp->len);
            p = tmp->data + o;
        }
        ch = ubuff->data[i];
        if (ch != 0 && ch < 0x80) *p++ = ch; else p = utf8out(ch, p);
    }
    return p;
}

static uint32_t fromiso2(uint8_t c) {
    static mbstate_t ps;
    wchar_t w;
    int olderrno;
    ssize_t l;

    memset(&ps, 0, sizeof ps);
    olderrno = errno;
    l = mbrtowc(&w, (char *)&c, 1,  &ps);
    errno = olderrno;
    if (l < 0) w = c;
    return w;
}

static inline uint32_t fromiso(uint8_t c) {
    static uint32_t conv[128];
    if (conv[c - 0x80] == 0) conv[c - 0x80] = fromiso2(c);
    return conv[c - 0x80];
}

static struct file_s *command_line = NULL;
static struct file_s *lastfi = NULL;
static uint16_t curfnum = 1;
struct file_s *openfile(const char* name, const char *base, int ftype, const Str *val, linepos_t epoint) {
    char *base2;
    struct avltree_node *b;
    struct file_s *tmp;
    char *s;
    if (lastfi == NULL) {
        lastfi = (struct file_s *)mallocx(sizeof *lastfi);
    }
    base2 = get_path(NULL, base);
    lastfi->base = base2;
    if (name != NULL) {
        lastfi->name = name;
        b = avltree_insert(&lastfi->node, &file_tree, file_compare);
    } else {
        b = (command_line != NULL) ? &command_line->node : NULL;
        if (command_line == NULL) command_line = lastfi;
    }
    if (b == NULL) { /* new file */
	enum filecoding_e type = E_UNKNOWN;
        FILE *f;
        uint32_t c = 0;
        size_t fp = 0;

	lastfi->line = NULL;
	lastfi->lines = 0;
	lastfi->data = NULL;
	lastfi->len = 0;
        lastfi->open = 0;
        lastfi->type = ftype;
        avltree_init(&lastfi->star);
        tmp = lastfi;
        lastfi = NULL;
        if (name != NULL) {
            int err;
            char *path = NULL;
            size_t namelen = strlen(name) + 1;
            s = (char *)mallocx(namelen);
            memcpy(s, name, namelen); tmp->name = s;
            if (val != NULL) {
                struct include_list_s *i = include_list.next;
                f = file_open(name, "rb");
                while (f == NULL && i != NULL) {
                    free(path);
                    path = get_path(val, i->path);
                    f = file_open(path, "rb");
                    i = i->next;
                }
            } else {
                f = dash_name(name) ? stdin : file_open(name, "rb");
            }
            if (path == NULL) {
                s = (char *)mallocx(namelen);
                memcpy(s, name, namelen);
                path = s;
            }
            tmp->realname = path;
            if (f == NULL) {
                path = (val != NULL) ? get_path(val, "") : NULL;
                err_msg_file(ERROR_CANT_FINDFILE, (val != NULL) ? path : name, epoint);
                free(path);
                return NULL;
            }
            if (ftype == 1) {
                if (arguments.quiet) {
                    printf("Reading file:      ");
                    argv_print(tmp->realname, stdout);
                    putchar('\n');
                }
                if (fseek(f, 0, SEEK_END) == 0) {
                    long len = ftell(f);
                    if (len >= 0) {
                        tmp->data = (uint8_t *)mallocx(len);
                        tmp->len = len;
                    }
                    rewind(f);
                }
                do {
                    if (fp + 4096 > tmp->len) {
                        tmp->len += 4096;
                        if (tmp->len < 4096) err_msg_out_of_memory(); /* overflow */
                        tmp->data = (uint8_t *)reallocx(tmp->data, tmp->len);
                    }
                    fp += fread(tmp->data + fp, 1, tmp->len - fp, f);
                } while (feof(f) == 0);
            } else {
                struct ubuff_s ubuff = {NULL, 0, 0};
                size_t max_lines = 0;
                line_t lines = 0;
                uint8_t buffer[BUFSIZ * 2];
                size_t bp = 0, bl, qr = 1;
                if (arguments.quiet) {
                    printf("Assembling file:   ");
                    argv_print(tmp->realname, stdout);
                    putchar('\n');
                }
                if (fseek(f, 0, SEEK_END) == 0) {
                    long len = ftell(f);
                    if (len >= 0) {
                        len += 4096;
                        if (len < 4096) err_msg_out_of_memory(); /* overflow */
                        tmp->data = (uint8_t *)mallocx(len);
                        tmp->len = len;
                        max_lines = (len / 20 + 1024) & ~1023;
                        if (max_lines > SIZE_MAX / sizeof *tmp->line) err_msg_out_of_memory(); /* overflow */
                        tmp->line = (size_t *)mallocx(max_lines * sizeof *tmp->line);
                    }
                    rewind(f);
                }
                bl = fread(buffer, 1, BUFSIZ, f);
                if (bl != 0 && buffer[0] == 0) type = E_UTF16BE; /* most likely */
#ifdef _WIN32
                setlocale(LC_CTYPE, "");
#endif
                do {
                    int i, j;
                    uint8_t *p;
                    uint32_t lastchar;
                    bool qc = true;
                    uint8_t cclass = 0;

                    if (lines >= max_lines) {
                        max_lines += 1024;
                        if (/*max_lines < 1024 ||*/ max_lines > SIZE_MAX / sizeof *tmp->line) err_msg_out_of_memory(); /* overflow */
                        tmp->line = (size_t *)reallocx(tmp->line, max_lines * sizeof *tmp->line);
                    }
                    tmp->line[lines++] = fp;
                    if (lines < 1) err_msg_out_of_memory(); /* overflow */
                    ubuff.p = 0;
                    p = tmp->data + fp;
                    for (;;) {
                        size_t o = p - tmp->data;
                        uint8_t ch2;
                        if (o + 6*6 + 1 > tmp->len) {
                            tmp->len += 4096;
                            if (tmp->len < 4096) err_msg_out_of_memory(); /* overflow */
                            tmp->data = (uint8_t *)reallocx(tmp->data, tmp->len);
                            p = tmp->data + o;
                        }
                        if (bp / (BUFSIZ / 2) == qr) {
                            if (qr == 1) {
                                qr = 3;
                                if (feof(f) == 0) bl = BUFSIZ + fread(buffer + BUFSIZ, 1, BUFSIZ, f);
                            } else {
                                qr = 1;
                                if (feof(f) == 0) bl = fread(buffer, 1, BUFSIZ, f);
                            }
                        }
                        if (bp == bl) break;
                        lastchar = c;
                        c = buffer[bp]; bp = (bp + 1) % (BUFSIZ * 2);
                        if (!arguments.toascii) {
                            if (c == 10) {
                                if (lastchar == 13) continue;
                                break;
                            } else if (c == 13) {
                                break;
                            }
                            if (c != 0 && c < 0x80) *p++ = c; else p = utf8out(c, p);
                            continue;
                        }
                        switch (type) {
                        case E_UNKNOWN:
                        case E_UTF8:
                            if (c < 0x80) goto done;
                            if (c < 0xc0) {
                            invalid:
                                if (type == E_UNKNOWN) {
                                    c = fromiso(c);
                                    type = E_ISO; break;
                                }
                                c = REPLACEMENT_CHARACTER; break;
                            } 
                            ch2 = (bp == bl) ? 0 : buffer[bp];
                            if (c < 0xe0) {
                                if (c < 0xc2) goto invalid;
                                c ^= 0xc0; i = 1;
                            } else if (c < 0xf0) {
                                if ((c ^ 0xe0) == 0 && (ch2 ^ 0xa0) >= 0x20) goto invalid;
                                c ^= 0xe0; i = 2;
                            } else if (c < 0xf8) {
                                if ((c ^ 0xf0) == 0 && (uint8_t)(ch2 - 0x90) >= 0x30) goto invalid;
                                c ^= 0xf0; i = 3;
                            } else if (c < 0xfc) {
                                if ((c ^ 0xf8) == 0 && (uint8_t)(ch2 - 0x88) >= 0x38) goto invalid;
                                c ^= 0xf8; i = 4;
                            } else if (c < 0xfe) {
                                if ((c ^ 0xfc) == 0 && (uint8_t)(ch2 - 0x84) >= 0x3c) goto invalid;
                                c ^= 0xfc; i = 5;
                            } else {
                                if (type != E_UNKNOWN) goto invalid;
                                if (c == 0xff && ch2 == 0xfe) type = E_UTF16LE;
                                else if (c == 0xfe && ch2 == 0xff) type = E_UTF16BE;
                                else goto invalid;
                                bp = (bp + 1) % (BUFSIZ * 2);
                                continue;
                            }

                            for (j = i; i != 0; i--) {
                                if (bp != bl) {
                                    ch2 = buffer[bp];
                                    if ((ch2 ^ 0x80) < 0x40) {
                                        c = (c << 6) ^ ch2 ^ 0x80;
                                        bp = (bp + 1) % (BUFSIZ * 2);
                                        continue;
                                    }
                                }
                                if (type != E_UNKNOWN) {
                                    c = REPLACEMENT_CHARACTER;break;
                                }
                                type = E_ISO;
                                i = (j - i) * 6;
                                qc = false;
                                if (ubuff.p >= ubuff.len) {
                                    ubuff.len += 16;
                                    if (/*ubuff.len < 16 ||*/ ubuff.len > SIZE_MAX / sizeof *ubuff.data) err_msg_out_of_memory(); /* overflow */
                                    ubuff.data = (uint32_t *)reallocx(ubuff.data, ubuff.len * sizeof *ubuff.data);
                                }
                                ubuff.data[ubuff.p++] = fromiso(((~0x7f >> j) & 0xff) | (c >> i));
                                for (;i != 0; i-= 6) {
                                    if (ubuff.p >= ubuff.len) {
                                        ubuff.len += 16;
                                        if (/*ubuff.len < 16 ||*/ ubuff.len > SIZE_MAX / sizeof *ubuff.data) err_msg_out_of_memory(); /* overflow */
                                        ubuff.data = (uint32_t *)reallocx(ubuff.data, ubuff.len * sizeof *ubuff.data);
                                    }
                                    ubuff.data[ubuff.p++] = fromiso(((c >> (i-6)) & 0x3f) | 0x80);
                                }
                                if (bp == bl) goto eof;
                                c = (ch2 >= 0x80) ? fromiso(ch2) : ch2; 
                                j = 0;
                                bp = (bp + 1) % (BUFSIZ * 2);
                                break;
                            }
                            if (j != 0) type = E_UTF8;
                            break;
                        case E_UTF16LE:
                            if (bp == bl) goto invalid;
                            c |= buffer[bp] << 8; bp = (bp + 1) % (BUFSIZ * 2);
                            if (c == 0xfffe) {
                                type = E_UTF16BE;
                                continue;
                            }
                            break;
                        case E_UTF16BE:
                            if (bp == bl) goto invalid;
                            c = (c << 8) | buffer[bp]; bp = (bp + 1) % (BUFSIZ * 2);
                            if (c == 0xfffe) {
                                type = E_UTF16LE;
                                continue;
                            }
                            break;
                        case E_ISO:
                            if (c >= 0x80) c = fromiso(c);
                            goto done;
                        }
                        if (c == 0xfeff) continue;
                        if (type != E_UTF8) {
                            if (c >= 0xd800 && c < 0xdc00) {
                                if (lastchar < 0xd800 || lastchar >= 0xdc00) continue;
                                c = REPLACEMENT_CHARACTER;
                            } else if (c >= 0xdc00 && c < 0xe000) {
                                if (lastchar >= 0xd800 && lastchar < 0xdc00) {
                                    c ^= 0x360dc00 ^ (lastchar << 10);
                                    c += 0x10000;
                                } else
                                    c = REPLACEMENT_CHARACTER;
                            } else if (lastchar >= 0xd800 && lastchar < 0xdc00) {
                                c = REPLACEMENT_CHARACTER;
                            }
                        }
                    done:
                        if (c < 0xc0) {
                            if (c == 10) {
                                if (lastchar == 13) continue;
                                break;
                            } else if (c == 13) {
                                break;
                            }
                            cclass = 0;
                            if (!qc) {
                                unfc(&ubuff);
                                qc = true;
                            }
                            if (ubuff.p == 1) {
                                if (ubuff.data[0] != 0 && ubuff.data[0] < 0x80) *p++ = ubuff.data[0]; else p = utf8out(ubuff.data[0], p);
                            } else {
                                p = flushubuff(&ubuff, p, tmp);
                                ubuff.p = 1;
                            }
                            ubuff.data[0] = c;
                        } else {
                            const struct properties_s *prop = uget_property(c);
                            uint8_t ncclass = prop->combclass;
                            if ((ncclass != 0 && cclass > ncclass) || (prop->property & (qc_N | qc_M)) != 0) {
                                qc = false;
                                if (ubuff.p >= ubuff.len) {
                                    ubuff.len += 16;
                                    if (/*ubuff.len < 16 ||*/ ubuff.len > SIZE_MAX / sizeof *ubuff.data) err_msg_out_of_memory(); /* overflow */
                                    ubuff.data = (uint32_t *)reallocx(ubuff.data, ubuff.len * sizeof *ubuff.data);
                                }
                                ubuff.data[ubuff.p++] = c;
                            } else {
                                if (!qc) {
                                    unfc(&ubuff);
                                    qc = true; 
                                }
                                if (ubuff.p == 1) {
                                    if (ubuff.data[0] != 0 && ubuff.data[0] < 0x80) *p++ = ubuff.data[0]; else p = utf8out(ubuff.data[0], p);
                                } else {
                                    p = flushubuff(&ubuff, p, tmp);
                                    ubuff.p = 1;
                                }
                                ubuff.data[0] = c;
                            }
                            cclass = ncclass;
                        }
                    }
                eof:
                    if (!qc) unfc(&ubuff);
                    p = flushubuff(&ubuff, p, tmp);
                    i = (p - tmp->data) - fp;
                    p = tmp->data + fp;
                    while (i != 0 && (p[i-1]==' ' || p[i-1]=='\t')) i--;
                    p[i++] = 0;
                    fp += i;
                } while (bp != bl);
#ifdef _WIN32
                setlocale(LC_CTYPE, "C");
#endif
                free(ubuff.data);
                tmp->lines = lines;
                if (lines != max_lines) {
                    tmp->line = (size_t *)reallocx(tmp->line, lines * sizeof *tmp->line);
                }
            }
            err = ferror(f);
            if (f != stdin) err |= fclose(f);
            if (err != 0 && errno != 0) err_msg_file(ERROR__READING_FILE, name, epoint);
            tmp->len = fp;
            tmp->data = (uint8_t *)reallocx(tmp->data, tmp->len);
            tmp->coding = type;
        } else {
            const char *cmd_name = "<command line>";
            size_t cmdlen = strlen(cmd_name) + 1;
            s = (char *)mallocx(1);
            s[0] = 0; tmp->name = s;
            s = (char *)mallocx(cmdlen);
            memcpy(s, cmd_name, cmdlen); tmp->realname = s;
            tmp->coding = E_UNKNOWN;
        }

        tmp->uid = (ftype != 1) ? curfnum++ : 0;
    } else {
        free(base2);
        tmp = avltree_container_of(b, struct file_s, node);
        if ((tmp->type == 1) != (ftype == 1)) err_msg_file(ERROR__READING_FILE, name, epoint);
    }
    tmp->open++;
    return tmp;
}

void closefile(struct file_s *f) {
    if (f->open != 0) f->open--;
}

static struct stars_s {
    struct star_s stars[255];
    struct stars_s *next;
} *stars = NULL;

static struct star_s *lastst;
static int starsp;
struct star_s *new_star(line_t line, bool *exists) {
    struct avltree_node *b;
    struct star_s *tmp;
    lastst->line = line;
    b = avltree_insert(&lastst->node, star_tree, star_compare);
    if (b == NULL) { /* new label */
	*exists = false;
        avltree_init(&lastst->tree);
        if (starsp == 254) {
            struct stars_s *old = stars;
            stars = (struct stars_s *)mallocx(sizeof *stars);
            stars->next = old;
            starsp = 0;
        } else starsp++;
        tmp = lastst;
        lastst = &stars->stars[starsp];
	return tmp;
    }
    *exists = true;
    return avltree_container_of(b, struct star_s, node);            /* already exists */
}

void destroy_file(void) {
    struct stars_s *old;

    avltree_destroy(&file_tree, file_free);
    free(lastfi);
    if (command_line != NULL) file_free(&command_line->node);

    include_list_last = include_list.next;
    while (include_list_last != NULL) {
        struct include_list_s *tmp = include_list_last;
        include_list_last = tmp->next;
        free(tmp);
    }

    while (stars != NULL) {
        old = stars;
        stars = stars->next;
        free(old);
    }
}

void init_file(void) {
    avltree_init(&file_tree);
    stars = (struct stars_s *)mallocx(sizeof *stars);
    stars->next = NULL;
    starsp = 0;
    lastst = &stars->stars[starsp];
}

void makefile(int argc, char *argv[]) {
    FILE *f;
    char *path;
    struct linepos_s nopoint = {0, 0};
    struct avltree_node *n;
    size_t len;
    int i, err;

    f = dash_name(arguments.make) ? stdout : file_open(arguments.make, "wt");
    if (f == NULL) {
        err_msg_file(ERROR_CANT_WRTE_MAK, arguments.make, &nopoint);
        return;
    }
    clearerr(f);
    path = get_path(NULL, arguments.output);
    len = argv_print(arguments.output + strlen(path), f) + 1;
    free(path);
    putc(':', f);

    for (i = 0; i < argc; i++) {
        if (len > 64) {
            fputs(" \\\n", f);
            len = 0;
        }
        putc(' ', f);
        len += argv_print(argv[i], f) + 1;
    }

    for (n = avltree_first(&file_tree); n != NULL; n = avltree_next(n)) {
        const struct file_s *a = cavltree_container_of(n, struct file_s, node);
        if (a->type != 0) {
            if (len > 64) {
                fputs(" \\\n", f);
                len = 0;
            }
            putc(' ', f);
            len += argv_print(a->realname, f) + 1;
        }
    }
    putc('\n', f);

    err = ferror(f);
    err |= (f != stdout) ? fclose(f) : fflush(f);
    if (err != 0 && errno != 0) err_msg_file(ERROR_CANT_WRTE_MAK, arguments.make, &nopoint);
}
