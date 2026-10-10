/* Strict stable release tags; encoded components compare numerically. */
#ifndef GULCHCE_RELEASE_VERSION_H
#define GULCHCE_RELEASE_VERSION_H
static long gulchce_version_code(const char *tag)
{
    long value = 0;
    int part;
    if (!tag || *tag++ != 'v') return -1;
    for (part = 0; part < 3; ++part)
    {
        long component = 0;
        int digits = 0;
        const char *start = tag;
        while (*tag >= '0' && *tag <= '9')
        {
            if (++digits > 3) return -1;
            component = component * 10 + (*tag++ - '0');
        }
        if (!digits || (digits > 1 && *start == '0')) return -1;
        value = value * 1000 + component;
        if (!*tag) return part == 1 ? value * 1000 : (part == 2 ? value : -1);
        if (*tag++ != '.') return -1;
    }
    return -1;
}
#endif
