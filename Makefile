CC ?= cc
AR ?= ar
CFLAGS ?= -O2
CPPFLAGS += -Iinclude -Isrc
WARNINGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes

CORE_SOURCES = \
	src/core/keccak.c \
	src/core/sha256.c \
	src/core/mlkem512_stream.c \
	src/core/ascon_aead128.c \
	src/session/crypto.c \
	src/session/preflight.c \
	src/session/fragment.c \
	src/session/ratchet.c \
	src/session/migration.c

CORE_OBJECTS = $(CORE_SOURCES:%.c=build/%.o)

.PHONY: all clean test oracle-test interop-test header-check arduino-check sanitizers size-report verify

all: build/libc0pqlink.a

build/libc0pqlink.a: $(CORE_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -std=c99 $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -c $< -o $@

build/c0pqlink_tests: $(CORE_SOURCES) tests/test_main.c tests/test_core.c tests/test_session.c
	@mkdir -p build
	$(CC) -std=c99 $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $^ -o $@

build/mlkem_oracle_cli: src/core/keccak.c src/core/mlkem512_stream.c tests/mlkem_oracle_cli.c
	@mkdir -p build
	$(CC) -std=c99 $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $^ -o $@

build/session_interop_cli: $(CORE_SOURCES) tests/session_interop_cli.c
	@mkdir -p build
	$(CC) -std=c99 $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $^ -o $@

test: build/c0pqlink_tests
	./build/c0pqlink_tests

oracle-test: build/mlkem_oracle_cli
	node tests/mlkem_oracle_test.mjs

interop-test: build/session_interop_cli
	node tests/session_interop_test.mjs
	node tests/session_full_mode_test.mjs

arduino-check: header-check
	@mkdir -p build/arduino
	@for source in $(CORE_SOURCES); do \
		object=build/arduino/$$(basename $$source .c).o; \
		$(CC) -std=c99 -Isrc $(CFLAGS) $(WARNINGS) -c $$source -o $$object; \
	done
	$(CXX) -std=c++11 -Isrc $(CFLAGS) -Wall -Wextra -Wpedantic \
		-c src/C0PQLink.cpp -o build/arduino/C0PQLink.o
	$(CXX) -std=c++11 -Isrc $(CFLAGS) -Wall -Wextra -Wpedantic \
		-c tests/arduino_api_compile.cpp -o build/arduino/api.o
	$(CXX) -std=c++11 -Isrc $(CFLAGS) -Wall -Wextra -Wpedantic \
		-c tests/arduino_example_compile.cpp -o build/arduino/example.o
	$(CXX) build/arduino/*.o -o build/arduino_api_check
	./build/arduino_api_check

header-check:
	sh tools/sync-arduino-headers.sh
	@for header in include/c0pqlink/*.h; do \
		cmp "$$header" "src/c0pqlink/$$(basename $$header)"; \
	done

sanitizers:
	@mkdir -p build
	$(CC) -std=c99 $(CPPFLAGS) -O1 -g $(WARNINGS) \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		$(CORE_SOURCES) tests/test_main.c tests/test_core.c tests/test_session.c \
		-o build/c0pqlink_tests_san
	ASAN_OPTIONS=detect_leaks=0 ./build/c0pqlink_tests_san

size-report:
	@mkdir -p build
	$(CC) -std=c99 $(CPPFLAGS) $(CFLAGS) $(WARNINGS) \
		$(CORE_SOURCES) tools/size_report.c -o build/size_report
	./build/size_report

verify: test oracle-test interop-test header-check arduino-check sanitizers

clean:
	rm -rf build
