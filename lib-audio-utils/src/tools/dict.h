//
// Created by layne on 19-4-27.
//

#ifndef AUDIO_EFFECT_DICT_H
#define AUDIO_EFFECT_DICT_H

#include <stdint.h>

#define AEERROR_EXTERNAL -6000

#define AV_DICT_MATCH_CASE                                               \
    1 /**< Only get an entry with exact-case key match. Only relevant in \
         ae_dict_get(). */
#define AV_DICT_IGNORE_SUFFIX                                                 \
    2 /**< Return first entry in a dictionary whose first part corresponds to \
         the search key, ignoring the suffix of the found key string. Only    \
         relevant in ae_dict_get(). */
#define AV_DICT_DONT_STRDUP_KEY                                              \
    4 /**< Take ownership of a key that's been                               \
           allocated with av_malloc() or another memory allocation function. \
       */
#define AV_DICT_DONT_STRDUP_VAL                                                                           \
    8                              /**< Take ownership of a value that's been                             \
                                        allocated with av_malloc() or another memory allocation function. \
                                    */
#define AV_DICT_DONT_OVERWRITE 16  ///< Don't overwrite existing entries.
#define AV_DICT_APPEND                                               \
    32 /**< If the entry already exists, append to it.  Note that no \
         delimiter is added, the strings are simply concatenated. */
#define AV_DICT_MULTIKEY \
    64 /**< Allow to store several equal keys in the dictionary */

typedef struct AEDictionaryEntry {
    char *key;
    char *value;
} AEDictionaryEntry;

typedef struct AEDictionary AEDictionary;

AEDictionaryEntry *ae_dict_get(const AEDictionary *m, const char *key,
                               const AEDictionaryEntry *prev, int flags);
int ae_dict_count(const AEDictionary *m);
int ae_dict_set(AEDictionary **pm, const char *key, const char *value,
                int flags);
int ae_dict_set_int(AEDictionary **pm, const char *key, int64_t value,
                    int flags);
int ae_dict_copy(AEDictionary **dst, const AEDictionary *src, int flags);
void ae_dict_free(AEDictionary **m);

#endif  // AUDIO_EFFECT_DICT_H
