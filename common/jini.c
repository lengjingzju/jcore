/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include "jlist.h"
#include "jfs.h"
#include "jfcache.h"
#include "jlog.h"
#include "jini.h"
#include "jheap.h"

#define JINI_ERROR(fmt, ...) jlog_print(JLOG_LEVEL_ERROR, &g_jcore_mod, NULL, "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

static const char *jini_none = "";

typedef struct {
    struct jslist list;
    char *key;
    char *val;
    uint32_t escaped:1;
    uint32_t special:1;
    uint32_t len:30;
} jini_node_t;

typedef struct {
    struct jslist list;
    struct jslist_head head;
    char *key;
} jini_section_t;

typedef struct {
    struct jslist_head head;
    char *fname;
    int flen;
    int changed;
} jini_mgr_t;

typedef enum {
    PARSE_IDLE = 0,
    PARSE_NOTE,
    PARSE_SECTION,
    PARSE_KEY,
    PARSE_START_VAL,
    PARSE_NORMAL_VAL,
    PARSE_SPECIAL_VAL
} jini_parse_t;

static int _parse_value(char *s, char *e, jini_node_t *pn, int offset)
{
    char *p = NULL, *ptr = NULL, *last = NULL;
    int len = (int)(e - s);
    int size = 0;
    char c = '\0';

    if (!len)
        return 0;

    pn->val = (char *)jheap_malloc(len + 1);
    if (!pn->val) {
        JINI_ERROR("malloc failed!");
        return -1;
    }

    if (!pn->escaped) {
        memcpy(pn->val, s, len);
        pn->val[len] = '\0';
        pn->len = len;
        return 0;
    }

    p = s;
    last = s;
    ptr = pn->val;
    while (p != e) {
        switch ((*p)) {
        case '\\':
            ++p;
            switch ((*p)) {
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            case 'v':  c = '\v'; break;
            case '\"': c = '\"'; break;
            case '\'': c = '\''; break;
            case '\\': c = '\\'; break;
            case '/' : c = '/' ; break;
            default :
                JINI_ERROR("invalid escape character(\\%c), offset = %d!", *p, (int)(offset - (e - p)));
                jheap_free(pn->val);
                pn->val = (char *)jini_none;
                return -1;
            }

            size = (int)(p - last - 1);
            memcpy(ptr, last, size);
            ptr += size;
            *ptr++ = c;
            last = p + 1;
            break;
        default:
            break;
        }
        ++p;
    }

    size = (int)(p - last);
    if (size) {
        memcpy(ptr, last, size);
        ptr += size;
    }
    *ptr = '\0';
    pn->len = (uint32_t)(ptr - pn->val);

    return 0;
}

void *jini_init(const char *fname)
{
    jini_mgr_t *mgr = NULL;
    char *buf = NULL;
    size_t count = 0;

    mgr = (jini_mgr_t *)jheap_calloc(1, sizeof(jini_mgr_t));
    if (!mgr) {
        JINI_ERROR("calloc failed!");
        return NULL;
    }
    jslist_init_head(&mgr->head);

    if (fname && fname[0]) {
        mgr->flen = (int)strlen(fname);
        mgr->fname = (char *)jheap_malloc(mgr->flen + 1);
        if (!mgr->fname) {
            jheap_free(mgr);
            JINI_ERROR("malloc failed!");
            return NULL;
        }
        memcpy(mgr->fname, fname, mgr->flen);
        mgr->fname[mgr->flen] = '\0';

        if (jfs_existed(fname)) {
            jini_section_t *ps = NULL;
            jini_node_t *pn = NULL;
            char *p = NULL, *s = NULL;
            char c;
            int len = 0, space = 0;
            jini_parse_t choice = PARSE_IDLE;

            if (jfs_readall(fname, &buf, &count) < 0) {
                jheap_free(mgr->fname);
                jheap_free(mgr);
                return NULL;
            }

            p = buf;
            while (1) {
                c = *p;
                switch (choice) {
                case PARSE_IDLE:
                    switch (c) {
                    case '\n': case '\r': case '\t': case '\v': case ' ':
                        break;
                    case '#': case ';':
                        choice = PARSE_NOTE;
                        break;
                    case '[':
                        s = p + 1;
                        choice = PARSE_SECTION;
                        break;
                    case '\0':
                        goto next;
                    default:
                        if (!ps) {
                            JINI_ERROR("Node is not below section, offset = %d!", (int)(p - buf));
                            goto err;
                        } else {
                            s = p;
                            choice = PARSE_KEY;
                        }
                        break;
                    }
                    break;

                case PARSE_NOTE:
                    switch (c) {
                    case '\n': case '\r':
                        choice = PARSE_IDLE;
                        break;
                    case '\0':
                        goto next;
                    default:
                        break;
                    }
                    break;

                case PARSE_SECTION:
                    switch (c) {
                    case '\n': case '\r':
                        JINI_ERROR("section is not ended with ']', offset = %d!", (int)(p - buf));
                        goto err;
                    case '\t': case '\v': case ' ':
                        ++space;
                        break;
                    case ']':
                        while (1) {
                            switch (*s) {
                            case '\t': case '\v': case ' ':
                                ++s;
                                break;
                            default:
                                goto section;
                            }
                        }

section:
                        len = (int)((p - s) - space);
                        space = 0;
                        if (!len) {
                            JINI_ERROR("section is space, offset = %d!", (int)(p - buf));
                            goto err;
                        }

                        ps = (jini_section_t *)jheap_malloc(sizeof(jini_section_t));
                        if (!ps) {
                            JINI_ERROR("malloc failed!");
                            goto err;
                        }
                        ps->key = (char *)jheap_malloc(len + 1);
                        if (!ps->key) {
                            JINI_ERROR("malloc failed!");
                            jheap_free(ps);
                            goto err;
                        }
                        memcpy(ps->key, s, len);
                        ps->key[len] = '\0';
                        jslist_init_head(&ps->head);
                        jslist_add_head_tail(&ps->list, &mgr->head);
                        choice = PARSE_IDLE;
                        break;
                    case '\0':
                        JINI_ERROR("section is not ended with ']', offset = %d!", (int)(p - buf));
                        goto err;
                    default:
                        space = 0;
                        break;
                    }
                    break;

                case PARSE_KEY:
                    switch (c) {
                    case '\n': case '\r':
                        JINI_ERROR("'=' is not after key, offset = %d!", (int)(p - buf));
                        goto err;
                    case '\t': case '\v': case ' ':
                        ++space;
                        break;
                    case '=':
                        len = (int)((p - s) - space);
                        space = 0;
                        if (!len) {
                            JINI_ERROR("key is space, offset = %d!", (int)(p - buf));
                            goto err;
                        }

                        pn = (jini_node_t *)jheap_calloc(1, sizeof(jini_node_t));
                        if (!pn) {
                            JINI_ERROR("malloc failed!");
                            goto err;
                        }
                        pn->key = (char *)jheap_malloc(len + 1);
                        if (!pn->key) {
                            JINI_ERROR("malloc failed!");
                            jheap_free(pn);
                            goto err;
                        }
                        memcpy(pn->key, s, len);
                        pn->key[len] = '\0';
                        pn->val = (char *)jini_none;
                        pn->special = 0;
                        jslist_add_head_tail(&pn->list, &ps->head);
                        choice = PARSE_START_VAL;
                        break;
                    case '\0':
                        JINI_ERROR("section not end with ']'!");
                        JINI_ERROR("'=' is not after key, offset = %d!", (int)(p - buf));
                        goto err;
                    default:
                        space = 0;
                        break;
                    }
                    break;

                case PARSE_START_VAL:
                    switch (c) {
                    case '\t': case '\v': case ' ':
                        break;
                    case '\n': case '\r':
                        choice = PARSE_IDLE;
                        break;
                    case '#': case ';':
                        choice = PARSE_NOTE;
                        break;
                    case '\"': case '\'':
                        s = p + 1;
                        choice = PARSE_SPECIAL_VAL;
                        break;
                    case '\0':
                        goto next;
                    default:
                        s = p;
                        choice = PARSE_NORMAL_VAL;
                        break;
                    }
                    break;

                case PARSE_NORMAL_VAL:
                    switch (c) {
                    case '\\':
                        ++p;
                        pn->escaped = 1;
                        space = 0;
                        break;
                    case '\"': case '\'':
                        pn->escaped = 1;
                        space = 0;
                        break;
                    case '\t': case '\v': case ' ':
                        ++space;
                        break;
                    case '\n': case '\r':
                        if (_parse_value(s, p - space, pn, (int)(p - buf) - space) < 0)
                            goto err;
                        choice = PARSE_IDLE;
                        break;
                    case '#': case ';':
                        if (_parse_value(s, p - space, pn, (int)(p - buf) - space) < 0)
                            goto err;
                        choice = PARSE_NOTE;
                        break;
                    case '\0':
                        if (_parse_value(s, p - space, pn, (int)(p - buf) - space) < 0)
                            goto err;
                        goto next;
                    default:
                        space = 0;
                        break;
                    }
                    break;

                case PARSE_SPECIAL_VAL:
                    switch (c) {
                    case '\\':
                        ++p;
                        pn->escaped = 1;
                        break;
                    case '\n': case '\r':
                        JINI_ERROR("value is not end with quote, offset = %d!", (int)(p - buf));
                        goto err;
                    case '#': case ';':
                        pn->special = 1;
                        break;
                    case '\"': case '\'':
                        if (*(s-1) != c) {
                            pn->escaped = 1;
                            break;
                        }

                        switch (*s) {
                        case '\t': case '\v': case ' ':
                            pn->special = 1;
                            break;
                        default:
                            break;
                        }
                        switch (*(p-1)) {
                        case '\t': case '\v': case ' ':
                            pn->special = 1;
                            break;
                        default:
                            break;
                        }

                        if (_parse_value(s, p, pn, (int)(p - buf)) < 0)
                            goto err;
                        choice = PARSE_IDLE;
                        break;
                    case '\0':
                        JINI_ERROR("value is not end with quote, offset = %d!", (int)(p - buf));
                        goto err;
                    default:
                        break;
                    }
                    break;

                default:
                    break;
                }
                ++p;
            }
next:
            jfs_readfree(&buf, &count);
        }
    }

    return mgr;
err:
    JINI_ERROR("write failed!");
    jfs_readfree(&buf, &count);
    jini_uninit(mgr);
    return NULL;
}

void jini_uninit(void *hd)
{
    jini_mgr_t *mgr = (jini_mgr_t *)hd;

    if (!mgr)
        return;

    jini_del_all(hd);
    if (mgr->fname)
        jheap_free(mgr->fname);
    jheap_free(mgr);
}

int jini_flush(void* hd, const char *fname)
{
    jini_mgr_t *mgr = (jini_mgr_t *)hd;
    jini_section_t *ps = NULL;
    jini_node_t *pn = NULL;
    void *fd = NULL;
    const char *path = NULL;
    char *fbak = NULL;
    int plen = 0;
    int ret = 0;

    if (fname && fname[0]) {
        plen = (int)strlen(fname);
        path = fname;
    } else {
        if (!fname && !mgr->changed && jfs_existed(mgr->fname))
            return 0;
        plen = mgr->flen;
        path = mgr->fname;
    }

    if (!path) {
        JINI_ERROR("path is null!");
        return -1;
    }

    fbak = (char *)jheap_malloc(plen + 5);
    if (!fbak) {
        JINI_ERROR("malloc failed!");
        return -1;
    }
    memcpy(fbak, path, plen);
    memcpy(fbak + plen, ".bak", 4);
    fbak[plen + 4] = '\0';

    fd = jfcache_open(fbak, "w", 0);
    if (!fd) {
        jheap_free(fbak);
        return -1;
    }

    jslist_for_each_entry(ps, &mgr->head, list, jini_section_t) {
        if (!jslist_empty(&ps->head)) {
            if (jfcache_write(fd, "[", 1) < 0)
                goto err;
            if (jfcache_write(fd, ps->key, strlen(ps->key)) < 0)
                goto err;
            if (jfcache_write(fd, "]\n", 2) < 0)
                goto err;

            jslist_for_each_entry(pn, &ps->head, list, jini_node_t) {
                if (jfcache_write(fd, pn->key, strlen(pn->key)) < 0)
                    goto err;
                if (jfcache_write(fd, " = ", 3) < 0)
                    goto err;

                if (pn->special) {
                    if (jfcache_write(fd, "\"", 1) < 0)
                        goto err;
                }

                if (pn->escaped) {
                    char *p = pn->val, *b = pn->val;
                    const char *c = NULL;

                    while (1) {
                        switch (*p) {
                        case '\"': c = "\""; break;
                        case '\'': c = "\'"; break;
                        case '\\': c = "\\"; break;
                        case '\b': c = "b" ; break;
                        case '\f': c = "f" ; break;
                        case '\n': c = "n" ; break;
                        case '\r': c = "r" ; break;
                        case '\t': c = "t" ; break;
                        case '\v': c = "v" ; break;
                        case '\0':
                            if (jfcache_write(fd, b, p - b) < 0)
                                goto err;
                            b = p;
                            goto next;
                        default:
                            ++p;
                            continue;
                        }

                        if (jfcache_write(fd, b, p - b) < 0)
                            goto err;
                        ++p;
                        b = p;
                        if (jfcache_write(fd, "\\", 1) < 0)
                            goto err;
                        if (jfcache_write(fd, c, 1) < 0)
                            goto err;
                    }
                } else {
                    if (jfcache_write(fd, pn->val, pn->len) < 0)
                        goto err;
                }
next:

                if (pn->special) {
                    if (jfcache_write(fd, "\"\n", 2) < 0)
                        goto err;
                } else {
                    if (jfcache_write(fd, "\n", 1) < 0)
                        goto err;
                }
            }
        }
    }

    if (jfcache_close(fd) < 0) {
        jfs_rmfile(fbak);
        jheap_free(fbak);
        return -1;
    }

    jfs_rmfile(path);
    ret = jfs_rename(fbak, path);
    jheap_free(fbak);
    mgr->changed = 0;
    return ret;
err:
    jfcache_close(fd);
    jfs_rmfile(fbak);
    jheap_free(fbak);
    return -1;
}

static jini_section_t *jini_get_section(void *hd, const char *section, jini_section_t **prev)
{
    jini_mgr_t *mgr = (jini_mgr_t *)hd;
    jini_section_t *p = NULL, *pos = NULL, *n = NULL;

    jslist_for_each_entry_safe(p, pos, n, &mgr->head, list, jini_section_t) {
        if (strcmp(pos->key, section) == 0) {
            if (prev)
                *prev = p;
            return pos;
        }
    }

    return NULL;
}

static jini_node_t *jini_get_node(jini_section_t *section, const char *key, jini_node_t **prev)
{
    jini_node_t *p = NULL, *pos = NULL, *n = NULL;

    jslist_for_each_entry_safe(p, pos, n, &section->head, list, jini_node_t) {
        if (strcmp(pos->key, key) == 0) {
            if (prev)
                *prev = p;
            return pos;
        }
    }

    return NULL;
}

void jini_del_node(void *hd, const char *section, const char *key)
{
    jini_mgr_t *mgr = (jini_mgr_t *)hd;
    jini_section_t *ps = NULL;
    jini_node_t *pn = NULL, *prev_pn = NULL;

    ps = jini_get_section(hd, section, NULL);
    if (ps)
        pn = jini_get_node(ps, key, &prev_pn);
    if (!pn)
        return;

    mgr->changed = 1;
    jslist_del(&pn->list, &prev_pn->list, &ps->head);
    if (pn->len)
        jheap_free(pn->val);
    jheap_free(pn->key);
    jheap_free(pn);
}

void jini_del_section(void *hd, const char *section)
{
    jini_mgr_t *mgr = (jini_mgr_t *)hd;
    jini_section_t *ps = NULL, *prev_pg = NULL;
    jini_node_t *p = NULL, *pos = NULL, *n = NULL;

    ps = jini_get_section(hd, section, &prev_pg);
    if (!ps)
        return;

    jslist_del(&ps->list, &prev_pg->list, &mgr->head);
    if (!jslist_empty(&ps->head)) {
        mgr->changed = 1;
        jslist_for_each_entry_safe(p, pos, n, &ps->head, list, jini_node_t) {
            jslist_del(&pos->list, &p->list, &ps->head);
            if (pos->len)
                jheap_free(pos->val);
            jheap_free(pos->key);
            jheap_free(pos);
            pos = p;
        }
    }
    jheap_free(ps->key);
    jheap_free(ps);
}

void jini_del_all(void *hd)
{
    jini_mgr_t *mgr = (jini_mgr_t *)hd;
    jini_section_t *sp = NULL, *spos = NULL, *sn = NULL;
    jini_node_t *p = NULL, *pos = NULL, *n = NULL;

    jslist_for_each_entry_safe(sp, spos, sn, &mgr->head, list, jini_section_t) {
        jslist_del(&spos->list, &sp->list, &mgr->head);
        if (!jslist_empty(&spos->head)) {
            mgr->changed = 1;
            jslist_for_each_entry_safe(p, pos, n, &spos->head, list, jini_node_t) {
                jslist_del(&pos->list, &p->list, &spos->head);
                if (pos->len)
                    jheap_free(pos->val);
                jheap_free(pos->key);
                jheap_free(pos);
                pos = p;
            }
        }
        jheap_free(spos->key);
        jheap_free(spos);
        spos = sp;
    }
}

const char *jini_get(void *hd, const char *section, const char *key, const char *dval)
{
    jini_section_t *ps = NULL;
    jini_node_t *pn = NULL;

    ps = jini_get_section(hd, section, NULL);
    if (ps)
        pn = jini_get_node(ps, key, NULL);

    return pn ? pn->val : dval;
}

int32_t jini_get_int(void *hd, const char *section, const char *key, int32_t dval)
{
    const char *str = jini_get(hd, section, key, NULL);
    return str ? atoi(str) : dval;
}

int64_t jini_get_lint(void *hd, const char *section, const char *key, int64_t dval)
{
    const char *str = jini_get(hd, section, key, NULL);
    return str ? atoll(str) : dval;
}

double jini_get_double(void *hd, const char *section, const char *key, double dval)
{
    const char *str = jini_get(hd, section, key, NULL);
    return str ? atof(str) : dval;
}

static int _jini_set(void *hd, const char *section, const char *key, const char *val, int check)
{
    jini_mgr_t *mgr = (jini_mgr_t *)hd;
    jini_section_t *ps = NULL;
    jini_node_t *pn = NULL, *prev_pn = NULL;
    int len = 0, i = 0;

    ps = jini_get_section(hd, section, NULL);
    if (ps) {
        pn = jini_get_node(ps, key, &prev_pn);
    } else {
        ps = (jini_section_t *)jheap_malloc(sizeof(jini_section_t));
        if (!ps) {
            JINI_ERROR("malloc failed!");
            return -1;
        }
        ps->key = jheap_strdup(section);
        if (!ps->key) {
            JINI_ERROR("strdup failed!");
            jheap_free(ps);
            return -1;
        }
        jslist_init_head(&ps->head);
        jslist_add_head_tail(&ps->list, &mgr->head);
    }

    if (val)
        len = (int)strlen(val);

    if (pn) {
        if (len == pn->len && strcmp(val, pn->val) == 0)
            return 0;
        mgr->changed = 1;

        if (len) {
            if (pn->len)
                jheap_free(pn->val);
            pn->val = (char *)jheap_malloc(len + 1);
            if (!pn->val) {
                jslist_del(&pn->list, &prev_pn->list, &ps->head);
                jheap_free(pn->key);
                jheap_free(pn);
                JINI_ERROR("malloc failed!");
                return -1;
            }
            memcpy(pn->val, val, len);
            pn->val[len] = '\0';
            pn->len = len;
            pn->escaped = 0;
            pn->special = 0;
        } else {
            if (pn->len) {
                jheap_free(pn->val);
                pn->val = (char *)jini_none;
                pn->len = 0;
                pn->escaped = 0;
                pn->special = 0;
            }
        }
    } else {
        pn = (jini_node_t *)jheap_calloc(1, sizeof(jini_node_t));
        if (!pn) {
            JINI_ERROR("malloc failed!");
            return -1;
        }
        pn->key = jheap_strdup(key);
        if (!pn->key) {
            JINI_ERROR("strdup failed!");
            jheap_free(pn);
            return -1;
        }

        if (len) {
            pn->val = (char *)jheap_malloc(len + 1);
            if (!pn->val) {
                jheap_free(pn->key);
                jheap_free(pn);
                JINI_ERROR("malloc failed!");
                return -1;
            }
            memcpy(pn->val, val, len);
            pn->val[len] = '\0';
            pn->len = len;
        } else {
            pn->val = (char *)jini_none;
            pn->special = 0;
        }
        jslist_add_head_tail(&pn->list, &ps->head);
        mgr->changed = 1;
    }

    if (check) {
        for (i = 0; i < len; ++i) {
            switch(val[i]) {
            case '\"': case '\'':
                pn->escaped = 1;
                break;
            case '\\':
                pn->escaped = 1;
                break;
            case '\n': case '\r': case '\b': case '\f':
                pn->escaped = 1;
                break;
            case '\t':
            case '\v':
                pn->escaped = 1;
                if (i == 0 || i == len - 1)
                    pn->special = 1;
                break;
            case ' ':
                if (i == 0 || i == len - 1)
                    pn->special = 1;
                break;
            case ';': case '#':
                pn->special = 1;
                break;
            default:
                break;
            }
            if (pn->special && pn->escaped)
                break;
        }
    }

    return 0;
}

int jini_set(void *hd, const char *section, const char *key, const char *val)
{
    return _jini_set(hd, section, key, val, 1);
}

int jini_set_int(void *hd, const char *section, const char *key, int32_t val)
{
    char buf[32];
    sprintf(buf, "%d", val);
    return _jini_set(hd, section, key, buf, 0);
}

int jini_set_lint(void *hd, const char *section, const char *key, int64_t val)
{
    char buf[32];
    sprintf(buf, "%lld", (long long)val);
    return _jini_set(hd, section, key, buf, 0);
}

int jini_set_double(void *hd, const char *section, const char *key, double val)
{
    char buf[32];
    sprintf(buf, "%1.16g", val);
    return _jini_set(hd, section, key, buf, 0);
}

