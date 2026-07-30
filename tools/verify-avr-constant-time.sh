#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
audit_dir="$project_dir/build/avr-audit"
mkdir -p "$audit_dir"

if ! command -v avr-gcc >/dev/null 2>&1 \
    || ! command -v avr-objdump >/dev/null 2>&1; then
    echo "SKIP: avr-gcc and avr-objdump are required for the linked AVR audit" >&2
    exit 77
fi

avr-gcc -mmcu=atmega328p -Os -std=c99 -DC0PQLINK_CT_AUDIT \
    -I"$project_dir/include" -I"$project_dir/src" \
    -ffunction-sections -fdata-sections \
    -c "$project_dir/src/core/mlkem512_stream.c" \
    -o "$audit_dir/mlkem512_stream.o"

avr-objdump -dr "$audit_dir/mlkem512_stream.o" \
    > "$audit_dir/mlkem512_stream.disassembly.txt"

awk '
/<multiply_s16_constant_schedule>:/ { inside=1; found=1; next }
inside && /^[[:xdigit:]]+[[:space:]]+<.*>:/ { inside=0 }
inside { print }
END { if (!found) exit 2 }
' "$audit_dir/mlkem512_stream.disassembly.txt" \
    > "$audit_dir/multiply_s16_constant_schedule.txt"

if grep -Eq '\b(brlt|brge|brlo|brsh|breq|brne|brmi|brpl|sbrc|sbrs)\b' \
    "$audit_dir/multiply_s16_constant_schedule.txt"; then
    echo "FAIL: conditional instruction found in signed-product correction" >&2
    cat "$audit_dir/multiply_s16_constant_schedule.txt" >&2
    exit 1
fi

echo "PASS: AVR signed-product correction contains no conditional instruction"
echo "Inspect linked multiply helper before a release claim:"
echo "  $audit_dir/mlkem512_stream.disassembly.txt"
