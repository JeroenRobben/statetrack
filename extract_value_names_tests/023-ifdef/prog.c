/* 023-ifdef: coverage is scoped to the build configuration. Built WITHOUT
 * -DENABLE_TURBO, so MODE_TURBO's case is preprocessed away and never seen.
 * The map reflects only the compiled config (MODE_OFF, MODE_ON). */
#include "statetrack_annotate.h"

#define MODE_OFF 0
#define MODE_ON  1
#ifdef ENABLE_TURBO
#define MODE_TURBO 2
#endif

struct S { int mode; };

int f(struct S *s)
{
    STATETRACK_REGISTER(s->mode);
    switch (s->mode) {
        case MODE_OFF: return 0;
        case MODE_ON:  return 1;
#ifdef ENABLE_TURBO
        case MODE_TURBO: return 2;
#endif
    }
    return -1;
}
