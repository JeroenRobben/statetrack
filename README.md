# statetrack

Tiny C library for tracking state variables of protocol implementations, e.g., during fuzzing.

Annotate variables of interest in the target with the macros in `statetrack_annotate.h`,
then inspect or fingerprint them at runtime with the functions in `statetrack.h`.

## Usage

```c
#include "statetrack_annotate.h"

int proto_state;
STATETRACK_REGISTER(proto_state);   /* tracks &proto_state, sizeof, "proto_state" */
/* ... */
STATETRACK_UNREGISTER(proto_state);
```

```c
#include "statetrack.h"

const StateVar *v = statetrack_find("proto_state");
uint64_t fp = statetrack_fingerprint(v);
```

## Build

```sh
make            # libstatetrack.a, libstatetrack.so, heap interposers
make check      # build and run the tests in test/
```

## Lifecycle monitoring

For global / heap variables you use `STATETRACK_REGISTER()`. Optionally, link in
`statetrack_heap`, which hooks `free` / `realloc` to unregister freed variables and
re-point (or drop) them when a block moves or shrinks.

For state variables that live on the stack (usually not the case), use
`STATETRACK_REGISTER_SCOPED`. It attaches a `cleanup()` compiler attribute that
unregisters the variable when it goes out of scope.