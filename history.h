#ifndef HISTORY_H
#define HISTORY_H

#define HISTLEN 20
#define MAX_BUF 200

/* The last HISTLEN unparsed command lines, kept as a circular queue */
typedef struct history
{
    char lines[HISTLEN][MAX_BUF];
    int oldest; /* slot of the oldest entry */
    int newest; /* slot of the newest entry */
    int count;  /* number of valid entries */
} history;

/* Empties the queue */
void historyInit(history *h);

/* Stores a copy of line, overwriting the oldest entry once the queue is full */
void historyAdd(history *h, const char *line);

/* Prints every entry, numbered from 1 (oldest) */
void historyPrint(const history *h);

/* Entry number n (1 is the oldest), or NULL if there is no such entry */
const char *historyGet(const history *h, int n);

/* The newest entry, or NULL if the queue is empty */
const char *historyLast(const history *h);

#endif
