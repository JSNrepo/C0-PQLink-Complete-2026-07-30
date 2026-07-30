MCU ?= atmega328p
F_CPU ?= 16000000UL
HOST_CC ?= cc
AVRDUDE ?= avrdude
PORT ?=
UPLOAD_BAUD ?= 57600
HASH_SIGS ?= /tmp/hash-sigs

NPM_PLATFORM := $(shell node -p \
	"process.platform + '-' + (process.platform === 'win32' ? 'ia32' : process.arch)" \
	2>/dev/null)
LOCAL_AVR_INSTALLED := node_modules/quirkbot-avr-gcc/tools/avr/bin/avr-
LOCAL_AVR_PACKED := \
	node_modules/quirkbot-avr-gcc/$(NPM_PLATFORM)/tools/avr/bin/avr-
ifneq ($(wildcard $(LOCAL_AVR_INSTALLED)gcc),)
AVR_PREFIX ?= $(LOCAL_AVR_INSTALLED)
else ifneq ($(wildcard $(LOCAL_AVR_PACKED)gcc),)
AVR_PREFIX ?= $(LOCAL_AVR_PACKED)
else
AVR_PREFIX ?= avr-
endif

CC := $(AVR_PREFIX)gcc
OBJCOPY := $(AVR_PREFIX)objcopy
SIZE := $(AVR_PREFIX)size
OBJDUMP := $(AVR_PREFIX)objdump

FIRMWARE := firmware/nanopq_secure_link
SLH_BUILD := build/avr-slh
LMS_W4_BUILD := build/avr-lms-w4
LMS_W8_BUILD := build/avr-lms-w8
HOST_BUILD := build/host
EVIDENCE := evidence

COMMON_SOURCES := \
	$(FIRMWARE)/common.c \
	$(FIRMWARE)/sha256.c \
	$(FIRMWARE)/ascon_aead128.c \
	$(FIRMWARE)/protocol.c

SLH_AVR_SOURCES := \
	$(FIRMWARE)/main.c \
	$(COMMON_SOURCES) \
	$(FIRMWARE)/slhdsa.c
LMS_AVR_SOURCES := \
	$(FIRMWARE)/main.c \
	$(COMMON_SOURCES) \
	$(FIRMWARE)/lms.c
SLH_AVR_OBJECTS := \
	$(patsubst $(FIRMWARE)/%.c,$(SLH_BUILD)/%.o,$(SLH_AVR_SOURCES))
LMS_W4_AVR_OBJECTS := \
	$(patsubst $(FIRMWARE)/%.c,$(LMS_W4_BUILD)/%.o,$(LMS_AVR_SOURCES))
LMS_W8_AVR_OBJECTS := \
	$(patsubst $(FIRMWARE)/%.c,$(LMS_W8_BUILD)/%.o,$(LMS_AVR_SOURCES))

PROTOCOL_TEST_SOURCES := $(COMMON_SOURCES) tests/test_protocol.c
SLH_TEST_SOURCES := \
	$(FIRMWARE)/sha256.c \
	$(FIRMWARE)/slhdsa.c \
	tests/test_slhdsa.c
LMS_TEST_SOURCES := \
	$(FIRMWARE)/sha256.c \
	$(FIRMWARE)/lms.c \
	tests/test_lms.c
PEER_SOURCES := $(COMMON_SOURCES) host/nanopq_peer.c

AVR_CFLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=gnu11 \
	-Wall -Wextra -Werror -ffunction-sections -fdata-sections \
	-fstack-usage -I$(FIRMWARE)
AVR_LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections
HOST_CFLAGS := -O2 -std=c99 -Wall -Wextra -Werror -Wpedantic \
	-I$(FIRMWARE)

.PHONY: \
	all clean host-test sanitizer-test peer reference-lms benchmark release \
	avr avr-slh avr-lms avr-lms-w4 avr-lms-w8 evidence simulate verify \
	flash flash-lms flash-lms-w8 flash-slh factory-reset

all: verify

host-test: \
	$(HOST_BUILD)/test_protocol \
	$(HOST_BUILD)/test_slhdsa \
	$(HOST_BUILD)/test_lms
	$(HOST_BUILD)/test_protocol
	$(HOST_BUILD)/test_slhdsa
	$(HOST_BUILD)/test_lms

$(HOST_BUILD)/test_protocol: $(PROTOCOL_TEST_SOURCES)
	mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(PROTOCOL_TEST_SOURCES) -o $@

$(HOST_BUILD)/test_slhdsa: $(SLH_TEST_SOURCES)
	mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(SLH_TEST_SOURCES) -o $@

$(HOST_BUILD)/test_lms: $(LMS_TEST_SOURCES)
	mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(LMS_TEST_SOURCES) -o $@

$(HOST_BUILD)/nanopq-peer: $(PEER_SOURCES)
	mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(PEER_SOURCES) -o $@

peer: $(HOST_BUILD)/nanopq-peer

reference-lms:
	mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -I$(HASH_SIGS) \
		tests/cisco_lms_reference_check.c \
		$(HASH_SIGS)/hss_verify.a -lcrypto \
		-o $(HOST_BUILD)/cisco-lms-reference-check
	$(HOST_BUILD)/cisco-lms-reference-check

sanitizer-test:
	mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		$(PROTOCOL_TEST_SOURCES) -o $(HOST_BUILD)/test-protocol-sanitize
	$(HOST_CC) $(HOST_CFLAGS) -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		$(SLH_TEST_SOURCES) -o $(HOST_BUILD)/test-slhdsa-sanitize
	$(HOST_CC) $(HOST_CFLAGS) -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		$(LMS_TEST_SOURCES) -o $(HOST_BUILD)/test-lms-sanitize
	ASAN_OPTIONS=detect_leaks=0 $(HOST_BUILD)/test-protocol-sanitize
	ASAN_OPTIONS=detect_leaks=0 $(HOST_BUILD)/test-slhdsa-sanitize
	ASAN_OPTIONS=detect_leaks=0 $(HOST_BUILD)/test-lms-sanitize

$(SLH_BUILD) $(LMS_W4_BUILD) $(LMS_W8_BUILD):
	mkdir -p $@

$(SLH_BUILD)/%.o: $(FIRMWARE)/%.c | $(SLH_BUILD)
	$(CC) $(AVR_CFLAGS) -c $< -o $@

$(LMS_W4_BUILD)/%.o: $(FIRMWARE)/%.c | $(LMS_W4_BUILD)
	$(CC) $(AVR_CFLAGS) -DNPQ_AUTH_LMS=1 -c $< -o $@

$(LMS_W8_BUILD)/%.o: $(FIRMWARE)/%.c | $(LMS_W8_BUILD)
	$(CC) $(AVR_CFLAGS) -DNPQ_AUTH_LMS=1 -DNPQ_LMS_W8=1 -c $< -o $@

$(SLH_BUILD)/nanopq.elf: $(SLH_AVR_OBJECTS)
	$(CC) $(AVR_LDFLAGS),-Map,$(SLH_BUILD)/nanopq.map $^ -o $@

$(LMS_W4_BUILD)/nanopq.elf: $(LMS_W4_AVR_OBJECTS)
	$(CC) $(AVR_LDFLAGS),-Map,$(LMS_W4_BUILD)/nanopq.map $^ -o $@

$(LMS_W8_BUILD)/nanopq.elf: $(LMS_W8_AVR_OBJECTS)
	$(CC) $(AVR_LDFLAGS),-Map,$(LMS_W8_BUILD)/nanopq.map $^ -o $@

$(SLH_BUILD)/nanopq.hex: $(SLH_BUILD)/nanopq.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(LMS_W4_BUILD)/nanopq.hex: $(LMS_W4_BUILD)/nanopq.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(LMS_W8_BUILD)/nanopq.hex: $(LMS_W8_BUILD)/nanopq.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(LMS_W4_BUILD)/factory-reset.eep: $(LMS_W4_BUILD)/nanopq.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 \
		--no-change-warnings -O ihex $< $@

avr-slh: $(SLH_BUILD)/nanopq.hex
	$(SIZE) -C --mcu=$(MCU) $(SLH_BUILD)/nanopq.elf

avr-lms-w4: $(LMS_W4_BUILD)/nanopq.hex
	$(SIZE) -C --mcu=$(MCU) $(LMS_W4_BUILD)/nanopq.elf

avr-lms-w8: $(LMS_W8_BUILD)/nanopq.hex
	$(SIZE) -C --mcu=$(MCU) $(LMS_W8_BUILD)/nanopq.elf

avr-lms: avr-lms-w4

avr: avr-lms-w4 avr-lms-w8 avr-slh

evidence: avr
	mkdir -p $(EVIDENCE)
	$(SIZE) -C --mcu=$(MCU) $(SLH_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-slh-size.txt
	$(SIZE) -C --mcu=$(MCU) $(LMS_W4_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-lms-w4-size.txt
	$(SIZE) -C --mcu=$(MCU) $(LMS_W8_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-lms-w8-size.txt
	$(SIZE) -A $(SLH_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-slh-sections.txt
	$(SIZE) -A $(LMS_W4_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-lms-w4-sections.txt
	$(SIZE) -A $(LMS_W8_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-lms-w8-sections.txt
	$(OBJDUMP) -dr $(SLH_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-slh-disassembly.txt
	$(OBJDUMP) -dr $(LMS_W4_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-lms-w4-disassembly.txt
	$(OBJDUMP) -dr $(LMS_W8_BUILD)/nanopq.elf \
		> $(EVIDENCE)/avr-lms-w8-disassembly.txt
	awk 'BEGIN {m=0;l=""} {split($$0,f,"\\t"); if (f[2]+0>m) {m=f[2]+0;l=$$0}} \
		END {print "largest_single_compiler_frame_bytes=" m; \
		print "largest_single_compiler_frame_record=" l}' \
		$(SLH_BUILD)/*.su > $(EVIDENCE)/avr-slh-stack-frame-summary.txt
	awk 'BEGIN {m=0;l=""} {split($$0,f,"\\t"); if (f[2]+0>m) {m=f[2]+0;l=$$0}} \
		END {print "largest_single_compiler_frame_bytes=" m; \
		print "largest_single_compiler_frame_record=" l}' \
		$(LMS_W4_BUILD)/*.su \
		> $(EVIDENCE)/avr-lms-w4-stack-frame-summary.txt
	awk 'BEGIN {m=0;l=""} {split($$0,f,"\\t"); if (f[2]+0>m) {m=f[2]+0;l=$$0}} \
		END {print "largest_single_compiler_frame_bytes=" m; \
		print "largest_single_compiler_frame_record=" l}' \
		$(LMS_W8_BUILD)/*.su \
		> $(EVIDENCE)/avr-lms-w8-stack-frame-summary.txt

simulate: evidence
	node simulator/avr_e2e.cjs \
		$(LMS_W4_BUILD)/nanopq.hex \
		$(LMS_W4_BUILD)/nanopq.map \
		$(EVIDENCE)/avr-lms-w4-e2e.json
	node simulator/avr_e2e.cjs \
		$(LMS_W8_BUILD)/nanopq.hex \
		$(LMS_W8_BUILD)/nanopq.map \
		$(EVIDENCE)/avr-lms-w8-e2e.json
	node simulator/avr_e2e.cjs \
		$(SLH_BUILD)/nanopq.hex \
		$(SLH_BUILD)/nanopq.map \
		$(EVIDENCE)/avr-slh-e2e.json

benchmark: simulate
	python3 tools/build_benchmark.py

release: benchmark sanitizer-test $(LMS_W4_BUILD)/factory-reset.eep peer
	python3 tools/check_release.py
	python3 tools/build_release.py

verify: host-test peer evidence

flash: flash-lms

flash-lms: $(LMS_W4_BUILD)/nanopq.hex
	test -n "$(PORT)" || { echo "set PORT=/dev/ttyUSB0"; exit 2; }
	$(AVRDUDE) -p atmega328p -c arduino -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U flash:w:$<:i

flash-lms-w8: $(LMS_W8_BUILD)/nanopq.hex
	test -n "$(PORT)" || { echo "set PORT=/dev/ttyUSB0"; exit 2; }
	$(AVRDUDE) -p atmega328p -c arduino -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U flash:w:$<:i

flash-slh: $(SLH_BUILD)/nanopq.hex
	test -n "$(PORT)" || { echo "set PORT=/dev/ttyUSB0"; exit 2; }
	$(AVRDUDE) -p atmega328p -c arduino -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U flash:w:$<:i

factory-reset: $(LMS_W4_BUILD)/factory-reset.eep
	test -n "$(PORT)" || { echo "set PORT=/dev/ttyUSB0"; exit 2; }
	$(AVRDUDE) -p atmega328p -c arduino -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U eeprom:w:$<:i

clean:
	rm -rf build
