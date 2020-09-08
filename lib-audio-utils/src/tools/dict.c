
#include "dict.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "avstring.h"
#include "error_def.h"
#if defined(__ANDROID__) || defined (__linux__)
#include "libavutil/mem.h"
#else
#include "mem.h"
#endif

struct AEDictionary {
    int count;
    AEDictionaryEntry *elems;
};

int ae_dict_count(const AEDictionary *m) { return m ? m->count : 0; }

AEDictionaryEntry *ae_dict_get(const AEDictionary *m, const char *key,
                               const AEDictionaryEntry *prev, int flags) {
    int i, j;

    if (!m) return NULL;

    if (prev)
        i = prev - m->elems + 1;
    else
        i = 0;

    for (; i < m->count; i++) {
        const char *s = m->elems[i].key;
        if (flags & AV_DICT_MATCH_CASE)
            for (j = 0; s[j] == key[j] && key[j]; j++)
                ;
        else
            for (j = 0; toupper(s[j]) == toupper(key[j]) && key[j]; j++)
                ;
        if (key[j]) continue;
        if (s[j] && !(flags & AV_DICT_IGNORE_SUFFIX)) continue;
        return &m->elems[i];
    }
    return NULL;
}

int ae_dict_set(AEDictionary **pm, const char *key, const char *value,
                int flags) {
    AEDictionary *m = *pm;
    AEDictionaryEntry *tag = NULL;
    char *oldval = NULL, *copy_key = NULL, *copy_value = NULL;

    if (!(flags & AV_DICT_MULTIKEY)) {
        tag = ae_dict_get(m, key, NULL, flags);
    }
    if (flags & AV_DICT_DONT_STRDUP_KEY)
        copy_key = (void *)key;
    else
        copy_key = av_strdup(key);
    if (flags & AV_DICT_DONT_STRDUP_VAL)
        copy_value = (void *)value;
    else if (copy_key)
        copy_value = av_strdup(value);
    if (!m) m = *pm = av_mallocz(sizeof(*m));
    if (!m || (key && !copy_key) || (value && !copy_value)) goto err_out;

    if (tag) {
        if (flags & AV_DICT_DONT_OVERWRITE) {
            av_free(copy_key);
            av_free(copy_value);
            return 0;
        }
        if (flags & AV_DICT_APPEND)
            oldval = tag->value;
        else
            av_free(tag->value);
        av_free(tag->key);
        *tag = m->elems[--m->count];
    } else if (copy_value) {
        AEDictionaryEntry *tmp =
            av_realloc(m->elems, (m->count + 1) * sizeof(*m->elems));
        if (!tmp) goto err_out;
        m->elems = tmp;
    }
    if (copy_value) {
        m->elems[m->count].key = copy_key;
        m->elems[m->count].value = copy_value;
        if (oldval && flags & AV_DICT_APPEND) {
            size_t len = strlen(oldval) + strlen(copy_value) + 1;
            char *newval = av_mallocz(len);
            if (!newval) goto err_out;
            av_strlcat(newval, oldval, len);
            av_freep(&oldval);
            av_strlcat(newval, copy_value, len);
            m->elems[m->count].value = newval;
            av_freep(&copy_value);
        }
        m->count++;
    } else {
        av_freep(&copy_key);
    }
    if (!m->count) {
        av_freep(&m->elems);
        av_freep(pm);
    }

    return 0;

err_out:
    if (m && !m->count) {
        av_freep(&m->elems);
        av_freep(pm);
    }
    av_free(copy_key);
    av_free(copy_value);
    return AEERROR_NOMEM;
}

int ae_dict_set_int(AEDictionary **pm, const char *key, int64_t value,
                    int flags) {
    char valuestr[22];
    snprintf(valuestr, sizeof(valuestr), "%" PRId64, value);
    flags &= ~AV_DICT_DONT_STRDUP_VAL;
    return ae_dict_set(pm, key, valuestr, flags);
}

__attribute__((unused)) static int parse_key_value_pair(AEDictionary **pm,
                                                        const char **buf,
                                                        const char *key_val_sep,
                                                        const char *pairs_sep,
                                                        int flags) {
    char *key = av_get_token(buf, key_val_sep);
    char *val = NULL;
    int ret;

    if (key && *key && strspn(*buf, key_val_sep)) {
        (*buf)++;
        val = av_get_token(buf, pairs_sep);
    }

    if (key && *key && val && *val)
        ret = ae_dict_set(pm, key, val, flags);
    else
        ret = AEERROR_INVAL;

    av_freep(&key);
    av_freep(&val);

    return ret;
}

void ae_dict_free(AEDictionary **pm) {
    AEDictionary *m = *pm;

    if (m) {
        while (m->count--) {
            av_freep(&m->elems[m->count].key);
            av_freep(&m->elems[m->count].value);
        }
        av_freep(&m->elems);
    }
    av_freep(pm);
}

int ae_dict_copy(AEDictionary **dst, const AEDictionary *src, int flags) {
    AEDictionaryEntry *t = NULL;

    while ((t = ae_dict_get(src, "", t, AV_DICT_IGNORE_SUFFIX))) {
        int ret = ae_dict_set(dst, t->key, t->value, flags);
        if (ret < 0) return ret;
    }

    return 0;
}

int aepriv_dict_set_timestamp(AEDictionary **dict, const char *key,
                              int64_t timestamp) {
    time_t seconds = timestamp / 1000000;
    struct tm *ptm, tmbuf;
    ptm = gmtime_r(&seconds, &tmbuf);
    if (ptm) {
        char buf[32];
        if (!strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", ptm))
            return AEERROR_EXTERNAL;
        av_strlcatf(buf, sizeof(buf), ".%06dZ", (int)(timestamp % 1000000));
        return ae_dict_set(dict, key, buf, 0);
    } else {
        return AEERROR_EXTERNAL;
    }
}
