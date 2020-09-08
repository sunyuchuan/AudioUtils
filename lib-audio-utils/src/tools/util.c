#include "util.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

int ae_strcasecmp(const char* s1, const char* s2) {
#if defined(HAVE_STRCASECMP)
    return strcasecmp(s1, s2);
#elif defined(_MSC_VER)
    return _stricmp(s1, s2);
#else
    while (*s1 && (toupper(*s1) == toupper(*s2))) s1++, s2++;
    return toupper(*s1) - toupper(*s2);
#endif
}

int ae_strncasecmp(char const* s1, char const* s2, size_t n) {
#if defined(HAVE_STRCASECMP)
    return strncasecmp(s1, s2, n);
#elif defined(_MSC_VER)
    return _strnicmp(s1, s2, n);
#else
    while (--n && *s1 && (toupper(*s1) == toupper(*s2))) s1++, s2++;
    return toupper(*s1) - toupper(*s2);
#endif
}

char *ae_read_file_to_string(const char *filename) {
    FILE *file = NULL;
    long length = 0;
    char *content = NULL;
    size_t read_chars = 0;

    /* open in read binary mode */
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        goto cleanup;
    }

    /* get the length */
    if (fseek(file, 0, SEEK_END) != 0)
    {
        goto cleanup;
    }
    length = ftell(file);
    if (length < 0)
    {
        goto cleanup;
    }
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        goto cleanup;
    }

    /* allocate content buffer */
    content = (char*)malloc((size_t)length + sizeof(""));
    if (content == NULL)
    {
        goto cleanup;
    }

    /* read the file into memory */
    read_chars = fread(content, sizeof(char), (size_t)length, file);
    if ((long)read_chars != length)
    {
        free(content);
        content = NULL;
        goto cleanup;
    }
    content[read_chars] = '\0';

cleanup:
    if (file != NULL)
    {
        fclose(file);
    }

    return content;
}

int ae_open_file(FILE **fp, const char *file_name, const int is_write) {
    int ret = 0;
    if (*fp) {
        fclose(*fp);
        *fp = NULL;
    }

    if (is_write)
        *fp = fopen(file_name, "wb");
    else
        *fp = fopen(file_name, "rb");

    if (!*fp) {
        ret = -1;
    }

    return ret;
}

