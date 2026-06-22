/* registration TU: the field is annotated here ... */
#include "state.h"
#include "statetrack_annotate.h"

void conn_init(struct conn *c)
{
    STATETRACK_REGISTER(c->action);
}
