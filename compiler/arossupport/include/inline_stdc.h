/*
    Copyright © 1995-2020, The AROS Development Team. All rights reserved.

    some inline functions as replacement for stdc functions
*/

static inline size_t Strlen(const char * ptr)
{
    const char * start = ptr;

    while (*ptr) ptr ++;

    return (((long)ptr) - ((long)start));
}


static inline int Strcmp(const char * str1, const char * str2)
{
    int diff;

    while (!(diff = *(unsigned char*) str1 - *(unsigned char*) str2) && *str1)
    {
        str1 ++;
        str2 ++;
    }

    return diff;
}

static inline char * Strchr(const char * str, int c)
{
    do
    {
        if ((unsigned char)*str == (unsigned char)c)
            return ((char *)str);
    } while (*(str++));

    return NULL;
}
