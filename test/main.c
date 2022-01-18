#include <stdio.h>

int av_strstart(const char *str, const char *pfx, const char **ptr) {
    while (*pfx && *pfx == *str) {
        pfx++;
        str++;
    }
    if (!*pfx && ptr)
        *ptr = str;
    return !*pfx;
}

int main() {
    char *str1 = "world0";
    char *str2 = "world";
    const char *end;
    printf("%d\n", av_strstart(str1, str2, &end));
    // printf("%s\n", end);
    return 0;
}