CC      ?= clang
CFLAGS  ?= -O2 -g -Wall -fPIC
AR      ?= ar

LIB  = libstatetrack.a
SO   = libstatetrack.so
OBJ  = statetrack.o

HEAP_SO   = libstatetrack_heap.so
HEAP_WOBJ = statetrack_heap_wrap.o

.PHONY: all clean check
all: $(LIB) $(SO) $(HEAP_SO) $(HEAP_WOBJ)

$(OBJ): statetrack.c statetrack.h
	$(CC) $(CFLAGS) -c statetrack.c -o $@

$(LIB): $(OBJ)
	$(AR) rcs $@ $(OBJ)

$(SO): $(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $(OBJ)

$(HEAP_SO): statetrack_heap.c statetrack.h $(LIB)
	$(CC) $(CFLAGS) -shared -o $@ statetrack_heap.c $(LIB) -ldl

# --wrap object: link the target with
#   -Wl,--wrap=free -Wl,--wrap=realloc $(HEAP_WOBJ)
$(HEAP_WOBJ): statetrack_heap.c statetrack.h
	$(CC) $(CFLAGS) -DSTATETRACK_HEAP_WRAP -c statetrack_heap.c -o $@

check: test/test_statetrack.c test/test_statetrack_macros.c test/test_statetrack_heap.c \
       statetrack.h statetrack_annotate.h $(LIB) $(HEAP_WOBJ)
	$(CC) $(CFLAGS) -o test_statetrack test/test_statetrack.c $(LIB)
	./test_statetrack
	$(CC) $(CFLAGS) -o test_statetrack_macros test/test_statetrack_macros.c $(LIB)
	./test_statetrack_macros
	$(CC) $(CFLAGS) -o test_statetrack_heap test/test_statetrack_heap.c \
	    -Wl,--wrap=free -Wl,--wrap=realloc $(HEAP_WOBJ) $(LIB)
	./test_statetrack_heap

clean:
	rm -f $(LIB) $(SO) $(OBJ) $(HEAP_SO) $(HEAP_WOBJ) \
	      test_statetrack test_statetrack_macros test_statetrack_heap
