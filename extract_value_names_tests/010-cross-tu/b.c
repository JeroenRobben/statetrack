/* ... but its value space is only visible in a *different* TU's switch.
 * Recovered by sweeping all TUs and merging by canonical USR. */
#include "state.h"

int conn_step(struct conn *c)
{
    switch (c->action) {
        case ACT_NONE:  return 0;
        case ACT_READ:  return 1;
        case ACT_WRITE: return 2;
    }
    return -1;
}
