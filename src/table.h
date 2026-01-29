#include "rax.h"
#include "listpack.h"

typedef struct table {
    rax *rax;                       /* The radix tree holding the stream. */
    uint64_t length;                /* Current number of elements inside this stream. */
    uint64_t last_id;               /* Zero if there are yet no items. */
    uint64_t first_id;              /* The first non-tombstone entry, zero if empty. */
    uint64_t max_deleted_entry_id;  /* The maximal ID that was deleted. */
    uint64_t entries_added;         /* All time count of elements added. */
    unsigned char *fields_lp;
} table;


/* Prototypes of exported APIs. */
struct client;

table *tableNew(void);
void freeTable(table *t);
