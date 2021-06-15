CC ?= gcc
OUTPUT ?= libplum.so
CFLAGS = -O3 -fomit-frame-pointer -Wl,-x -Wl,--gc-sections

all: build/libplum.c build/libplum.h
	$(CC) -shared $(CFLAGS) $^ -o build/$(OUTPUT)

clean:
	rm -rf build

build/libplum.c: $(wildcard src/*.c) $(wildcard src/*.h) merge.sh
	mkdir -p build
	./merge.sh $(wildcard src/*.c) > $@

debug: build/libplum-debug.so build/libplum.h

build/libplum-debug.so: $(wildcard src/*.c) $(wildcard src/*.h)
	mkdir -p build
	$(CC) -shared -ggdb -fPIC -DPLUM_DEBUG $(wildcard src/*.c) -o $@

build/libplum.h: src/libplum.h merge.sh
	mkdir -p build
	./merge.sh $< > $@
