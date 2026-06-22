/* 003-computed-macro: constants are computed function-like macros (the
 * noise-c NOISE_ID('A',1) shape). Tests that values are *folded*, not parsed:
 *   MK('M',1) = ('M'<<8)|1 = 19712|1 = 19713 ;  MK('M',2) = 19714. */
#include "statetrack_annotate.h"

#define MK(a, b)  (((a) << 8) | (b))
#define MSG_HELLO MK('M', 1)
#define MSG_BYE   MK('M', 2)

struct S { int msg; };

int step(struct S *s)
{
    STATETRACK_REGISTER(s->msg);
    switch (s->msg) {
        case MSG_HELLO: return 1;
        case MSG_BYE:   return 2;
    }
    return 0;
}
