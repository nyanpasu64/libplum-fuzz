CC = clang
OUTPUT ?= plum-fuzz
OPTFLAGS =
DEBUGFLAGS =

CFLAGS = -O1 -g -fsanitize=fuzzer $(OPTFLAGS)

all: basefiles
	$(CC) -fPIC $(CFLAGS) build/libplum.c -o build/$(OUTPUT)

clean:
	rm -rf build

basefiles: build/libplum.c build/libplum.h

debug: build/libplum-debug.so build/libplum.h

build/libplum.c: $(wildcard src/*.c) $(wildcard src/*.h) $(wildcard header/*.h) merge.sh
	mkdir -p build
	./merge.sh $(sort $(wildcard src/*.c)) > $@

build/libplum-debug.so: $(wildcard src/*.c) $(wildcard src/*.h) $(wildcard header/*.h)
	mkdir -p build
	$(CC) -shared -ggdb -fPIC -DPLUM_DEBUG $(DEBUGFLAGS) $(wildcard src/*.c) -o $@

build/libplum.h: $(wildcard header/*.h) merge.sh
	mkdir -p build
	./merge.sh header/libplum.h > $@
