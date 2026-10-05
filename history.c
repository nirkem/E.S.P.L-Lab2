#include <stdio.h>
#include <string.h>
#include "history.h"

void historyInit(history *h)
{
    h->oldest = 0;
    h->newest = -1;
    h->count = 0;
}

void historyAdd(history *h, const char *line)
{
    h->newest = (h->newest + 1) % HISTLEN;
    if (h->count == HISTLEN)
        h->oldest = (h->oldest + 1) % HISTLEN; // the new line takes the oldest slot
    else
        h->count++;

    strncpy(h->lines[h->newest], line, MAX_BUF - 1);
    h->lines[h->newest][MAX_BUF - 1] = 0;
}

const char *historyGet(const history *h, int n)
{
    if (n < 1 || n > h->count)
        return NULL;
    return h->lines[(h->oldest + n - 1) % HISTLEN];
}

const char *historyLast(const history *h)
{
    return h->count ? h->lines[h->newest] : NULL;
}

void historyPrint(const history *h)
{
    for (int n = 1; n <= h->count; n++)
        printf("%3d  %s\n", n, historyGet(h, n));
}
