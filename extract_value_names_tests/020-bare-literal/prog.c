/* 020-bare-literal: switch cases are raw integers with no symbolic name.
 * There is nothing to name -> the field stays numeric (no invented names). */
#include "statetrack_annotate.h"

struct S { int code; };

int f(struct S *s)
{
    STATETRACK_REGISTER(s->code);
    switch (s->code) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
    }
    return -1;
}
