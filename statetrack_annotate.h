#ifndef STATETRACK_ANNOTATE_H
#define STATETRACK_ANNOTATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((weak)) void statetrack_register(void *ptr, size_t size, const char *name);


__attribute__((weak)) void statetrack_unregister(void *ptr);

static inline void statetrack__scoped_cleanup(void *guard) {
    if (statetrack_unregister)
        statetrack_unregister(*(void **)guard);
}

#define STATETRACK_REGISTER(var)                                             \
    do {                                                                     \
        if (statetrack_register)                                             \
            statetrack_register(&(var), sizeof(var), #var);                  \
    } while (0)

#define STATETRACK_REGISTER_AS(var, label)                                   \
    do {                                                                     \
        if (statetrack_register)                                             \
            statetrack_register(&(var), sizeof(var), (label));               \
    } while (0)

#define STATETRACK_UNREGISTER(var)                                           \
    do {                                                                     \
        if (statetrack_unregister)                                           \
            statetrack_unregister(&(var));                                   \
    } while (0)

#define STATETRACK__CAT_(a, b) a##b
#define STATETRACK__CAT(a, b)  STATETRACK__CAT_(a, b)

#define STATETRACK_REGISTER_SCOPED(var)                                      \
    void *STATETRACK__CAT(statetrack__guard_, __LINE__)                      \
        __attribute__((cleanup(statetrack__scoped_cleanup))) =               \
        ((statetrack_register                                                \
              ? (statetrack_register(&(var), sizeof(var), #var), 0)          \
              : 0),                                                          \
         &(var))

#define STATETRACK_REGISTER_SCOPED_AS(var, label)                            \
    void *STATETRACK__CAT(statetrack__guard_, __LINE__)                      \
        __attribute__((cleanup(statetrack__scoped_cleanup))) =               \
        ((statetrack_register                                                \
              ? (statetrack_register(&(var), sizeof(var), (label)), 0)       \
              : 0),                                                          \
         &(var))

#ifdef __cplusplus
}
#endif

#endif /* STATETRACK_ANNOTATE_H */
