/* 002-int-define-switch: int field whose value space is #define constants,
 * recovered from the switch case labels (the noise-c `action` shape). */
#include "statetrack_annotate.h"

#define ACT_NONE  0
#define ACT_READ  1
#define ACT_WRITE 2

struct S { int action; };

int step(struct S *s)
{
    STATETRACK_REGISTER(s->action);
    switch (s->action) {
        case ACT_NONE:  return 0;
        case ACT_READ:  return 1;
        case ACT_WRITE: return 2;
    }
    return -1;
}
