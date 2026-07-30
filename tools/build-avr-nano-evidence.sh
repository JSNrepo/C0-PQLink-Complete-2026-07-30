#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-"$project_dir/build/avr-nano-current"}

: "${AVR_TOOLCHAIN_BIN:?Set AVR_TOOLCHAIN_BIN to the directory containing avr-gcc, avr-g++, avr-size, and avr-objdump}"
: "${ARDUINO_CORE_ASSETS:?Set ARDUINO_CORE_ASSETS to the avr-gcc-wasm assets directory containing fs/arduino and objects}"

avr_gcc="$AVR_TOOLCHAIN_BIN/avr-gcc"
avr_gxx="$AVR_TOOLCHAIN_BIN/avr-g++"
avr_size="$AVR_TOOLCHAIN_BIN/avr-size"
avr_objdump="$AVR_TOOLCHAIN_BIN/avr-objdump"
core_include="$ARDUINO_CORE_ASSETS/fs/arduino/core"
variant_include="$ARDUINO_CORE_ASSETS/fs/arduino/variant"
core_objects="$ARDUINO_CORE_ASSETS/objects"

for required in \
    "$avr_gcc" "$avr_gxx" "$avr_size" "$avr_objdump" \
    "$core_include/Arduino.h" "$variant_include/pins_arduino.h" \
    "$core_objects/core_main.o" "$core_objects/core_wiring.o" \
    "$core_objects/core_new.o"; do
    if [ ! -e "$required" ]; then
        echo "Missing required Nano build input: $required" >&2
        exit 1
    fi
done

mkdir -p "$build_dir"

common_flags="-mmcu=atmega328p -DF_CPU=16000000UL -DARDUINO=10819 -DARDUINO_AVR_NANO"
common_flags="$common_flags -Os -ffunction-sections -fdata-sections -fstack-usage"
include_flags="-I$project_dir/include -I$project_dir/src -I$core_include -I$variant_include"

compile_c()
{
    source_file=$1
    object_file=$2
    # shellcheck disable=SC2086
    "$avr_gcc" $common_flags $include_flags -std=gnu99 -Wall -Wextra \
        -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes \
        -c "$source_file" -o "$object_file"
}

compile_cxx()
{
    source_file=$1
    object_file=$2
    shift 2
    # shellcheck disable=SC2086
    "$avr_gxx" $common_flags $include_flags -std=gnu++11 -Wall -Wextra \
        -Wpedantic -fno-exceptions -fno-threadsafe-statics "$@" \
        -c "$source_file" -o "$object_file"
}

compile_c "$project_dir/src/core/keccak.c" "$build_dir/keccak.o"
compile_c "$project_dir/src/core/sha256.c" "$build_dir/sha256.o"
compile_c "$project_dir/src/core/mlkem512_stream.c" "$build_dir/mlkem512_stream.o"
compile_c "$project_dir/src/core/ascon_aead128.c" "$build_dir/ascon_aead128.o"
compile_c "$project_dir/src/session/crypto.c" "$build_dir/crypto.o"
compile_c "$project_dir/src/session/preflight.c" "$build_dir/preflight.o"
compile_c "$project_dir/src/session/fragment.c" "$build_dir/fragment.o"
compile_c "$project_dir/src/session/ratchet.c" "$build_dir/ratchet.o"
compile_c "$project_dir/src/session/migration.c" "$build_dir/migration.o"

compile_cxx "$project_dir/src/C0PQLink.cpp" "$build_dir/C0PQLink.o"
compile_cxx \
    "$project_dir/examples/LiveSensorClient/LiveSensorClient.ino" \
    "$build_dir/LiveSensorClient.o" \
    -x c++ -include Arduino.h

elf="$build_dir/C0-PQLink-Nano.elf"
map="$build_dir/C0-PQLink-Nano.map"

# shellcheck disable=SC2086
"$avr_gxx" -mmcu=atmega328p \
    -Wl,--gc-sections "-Wl,-Map,$map" \
    "$build_dir"/keccak.o \
    "$build_dir"/sha256.o \
    "$build_dir"/mlkem512_stream.o \
    "$build_dir"/ascon_aead128.o \
    "$build_dir"/crypto.o \
    "$build_dir"/preflight.o \
    "$build_dir"/fragment.o \
    "$build_dir"/ratchet.o \
    "$build_dir"/migration.o \
    "$build_dir"/C0PQLink.o \
    "$build_dir"/LiveSensorClient.o \
    "$core_objects/core_main.o" \
    "$core_objects/core_wiring.o" \
    "$core_objects/core_new.o" \
    -o "$elf"

"$avr_size" -C --mcu=atmega328p "$elf" \
    | tee "$build_dir/avr-size.txt"
"$avr_size" -A "$elf" > "$build_dir/sections.txt"
"$avr_objdump" -dr "$elf" > "$build_dir/C0-PQLink-Nano.disassembly.txt"

awk '
BEGIN { maximum = 0; maximum_line = "" }
{
    split($0, fields, "\t")
    if (fields[2] + 0 > maximum) {
        maximum = fields[2] + 0
        maximum_line = $0
    }
}
END {
    print "largest_single_compiler_frame_bytes=" maximum
    print "largest_single_compiler_frame_record=" maximum_line
}
' "$build_dir"/*.su | tee "$build_dir/stack-frame-summary.txt"
