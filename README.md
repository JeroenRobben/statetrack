# statetrack

Tiny C library for tracking state variables of protocol implementations, e.g., during fuzzing.
Same concept as [IJON](https://github.com/RUB-SysSec/ijon) but simpler and not tied to AFL.

You annotate variables of interest in the target with the macros in [`statetrack_annotate.h`](statetrack_annotate.h),
then inspect them at runtime with the functions in [`statetrack.h`](statetrack.c).

## Registering variables

You first register the state variables you want to track using the macros/functions in `statetrack_annotate.h`. See [profuzzbench-statetrack](https://github.com/JeroenRobben/profuzzbench-statetrack) and [hostap-statetrack](https://github.com/JeroenRobben/hostap-statetrack) for examples.

```c
#include "statetrack_annotate.h"

enum proto_state my_state = INIT;
uint64_t nonce = 0;
STATETRACK_REGISTER(my_state); // Registered with name "my_state"
STATETRACK_REGISTER_AS(nonce, "nonce_with_custom_name"); 
```

Then you build your target and link it with libstatetrack.a or libstatetrack.so. The macros just resolve to no-ops otherwise.

## Accessing tracked variables

Then later, in your own code that lives in the same process as the target (like a [netfuzzlib](https://github.com/JeroenRobben/netfuzzlib) module), you can look up state variables by name and get a pointer to their location in memory.

```c
#include "statetrack.h"

const StateVar *v = statetrack_find("my_state");
if (v) {
    enum proto_state s = *(enum proto_state *)v->ptr;
    printf("%s now has value %d\n", v->name, s);
}

// or iterate over everything registered:
for (int i = 0; i < statetrack_count(); i++) {
    const StateVar *v = statetrack_at(i);
    printf("%s @ %p (%zu bytes)\n", v->name, v->ptr, v->size);
}
```

## Finding state variables automatically

In my experience, asking Claude to <i>find and instrument all the state variables, make no mistakes</i> works pretty well.

## Lifecycle monitoring

Tracked variables might go out of scope, causing the registry to contain stale pointers.

For global / heap variables you can link in or `LD_PRELOAD`
[`statetrack_heap`](statetrack_heap.c), which interposes `free` / `realloc` to unregister freed variables and
re-point (or drop) them when a block moves or shrinks.

For state variables that live on the stack (this usually not the case), you can use
`STATETRACK_REGISTER_SCOPED`. It attaches a `cleanup()` compiler attribute that
calls statetrack_unregister() when it goes out of scope.

You can always unregister a state variable explicitly with `STATETRACK_UNREGISTER()`.

## Register / unregister callbacks

You can register callbacks that fire whenever a state variable enters or leaves the registry.
`on_register` is called after the variable is added, `on_unregister` before it is removed (the
pointer is still valid inside the callback). Either may be NULL.

```c
#include "statetrack.h"

void on_register(const StateVar *v, void *user) {
    printf("registered %s @ %p\n", v->name, v->ptr);
}

statetrack_add_observer(on_register, NULL, NULL);
```

## Extracting state variable value names

[`extract_value_names.py`](extract_value_names.py) is a script Claude made that outputs a json file with the enum/macro constant names that
are associated with instrumented state variables, using only static analysis. See `extract_value_names_tests/` for examples.

### Usage

1. Annotate the target's state variables with `STATETRACK_REGISTER(...)`.
2. Generate a [compile DB](https://clang.llvm.org/docs/JSONCompilationDatabase.html). Some options:
   - Prefix the target's build with [Bear](https://github.com/rizsotto/Bear): `bear -- make`.
   - When using CMake, set `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.
3. Run:

```sh
extract_value_names.py --compile-db compile_commands.json [-o out.json]
```

Input:

```c
#define ACT_NONE  0
#define ACT_READ  1
#define ACT_WRITE 2

typedef enum { ST_INIT = 0, ST_RUN = 1, ST_DONE = 2 } phase_t;

struct S { int action; phase_t phase; };

int step(struct S *s)
{
    STATETRACK_REGISTER(s->action);  // int, recovered from the switch labels
    STATETRACK_REGISTER(s->phase);   // enum-typed, decoded from the type alone
    switch (s->action) {
        case ACT_NONE:  return 0;
        case ACT_READ:  return 1;
        case ACT_WRITE: return 2;
    }
    return -1;
}
```

Output:

```json
    { "s->action": { "0": "ACT_NONE", "1": "ACT_READ", "2": "ACT_WRITE" },
      "s->phase":  { "0": "ST_INIT",  "1": "ST_RUN",   "2": "ST_DONE"  } }
```
