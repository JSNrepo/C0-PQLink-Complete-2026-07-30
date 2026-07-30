#include <avr/eeprom.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include <stdint.h>
#include <string.h>

#include "common.h"
#include "protocol.h"
#include "provisioning.h"
#ifdef NPQ_AUTH_LMS
#include "lms.h"
#else
#include "slhdsa.h"
#endif

#ifdef NPQ_AUTH_LMS
#ifdef NPQ_LMS_W8
#define NPQ_AUTH_ALGORITHM_ID 3u
#define NPQ_AUTH_SIGNATURE_BYTES NPQ_LMS_W8_SIGNATURE_BYTES
#define NPQ_AUTH_PUBLIC_KEY NPQ_LMS_W8_PUBLIC_KEY
#else
#define NPQ_AUTH_ALGORITHM_ID 2u
#define NPQ_AUTH_SIGNATURE_BYTES NPQ_LMS_W4_SIGNATURE_BYTES
#define NPQ_AUTH_PUBLIC_KEY NPQ_LMS_W4_PUBLIC_KEY
#endif
#define NPQ_AUTH_PUBLIC_KEY_BYTES NPQ_LMS_PUBLIC_KEY_BYTES
#else
#define NPQ_AUTH_ALGORITHM_ID 1u
#define NPQ_AUTH_PUBLIC_KEY_BYTES QP_SLHDSA_PUBLIC_KEY_BYTES
#define NPQ_AUTH_SIGNATURE_BYTES QP_SLHDSA_SIGNATURE_BYTES
#endif

#define NPQ_STATE_MAGIC 0x4e50u
#define NPQ_STATE_SCHEMA 1u
#define NPQ_UART_TIMEOUT_TICKS 31250u
#define NPQ_POLICY_MANIFEST_BYTES 48u
#define NPQ_POLICY_FRAME_BYTES 52u
#define NPQ_POLICY_CONTEXT_BYTES 14u

#define NPQ_PAYLOAD_SENSOR 0x01u
#define NPQ_PAYLOAD_STATUS 0x02u
#define NPQ_PAYLOAD_COMMAND 0x10u

#define NPQ_STATUS_REJECTED 0u
#define NPQ_STATUS_ACCEPTED 1u

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t schema;
    uint8_t authorized;
    uint32_t generation;
    uint32_t boot_epoch;
    uint32_t policy_sequence;
    uint16_t crc;
} npq_state_record;

static npq_state_record EEMEM npq_eeprom_slots[2];
static npq_state_record npq_state;
static npq_session npq_live_session;
static uint8_t npq_hello[NPQ_HELLO_BYTES];
static uint8_t npq_challenge[NPQ_CHALLENGE_BYTES];
static uint8_t npq_frame[NPQ_FRAME_MAX_BYTES];
static uint8_t npq_plaintext[NPQ_DATA_MAX_BYTES];
#ifndef NPQ_AUTH_LMS
static const uint8_t npq_policy_context[] = "Q-PUNCTURE-205";
#endif

static uint16_t crc16_ccitt(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xffffu;
    uint8_t bit;
    while (length-- != 0u) {
        crc ^= (uint16_t)(*data++) << 8u;
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) != 0u
                ? (uint16_t)((crc << 1u) ^ 0x1021u)
                : (uint16_t)(crc << 1u);
        }
    }
    return crc;
}

static uint8_t state_valid(const npq_state_record *record)
{
    const uint16_t expected = crc16_ccitt(
        (const uint8_t *)record,
        (uint8_t)(sizeof(*record) - sizeof(record->crc))
    );
    return record->magic == NPQ_STATE_MAGIC
        && record->schema == NPQ_STATE_SCHEMA
        && record->authorized <= 1u
        && record->crc == expected;
}

static void state_defaults(void)
{
    memset(&npq_state, 0, sizeof(npq_state));
    npq_state.magic = NPQ_STATE_MAGIC;
    npq_state.schema = NPQ_STATE_SCHEMA;
    npq_state.crc = crc16_ccitt(
        (const uint8_t *)&npq_state,
        (uint8_t)(sizeof(npq_state) - sizeof(npq_state.crc))
    );
}

static void state_load(void)
{
    npq_state_record first;
    npq_state_record second;
    const uint8_t first_valid = (
        eeprom_read_block(&first, &npq_eeprom_slots[0], sizeof(first)),
        state_valid(&first)
    );
    const uint8_t second_valid = (
        eeprom_read_block(&second, &npq_eeprom_slots[1], sizeof(second)),
        state_valid(&second)
    );
    if (first_valid == 0u && second_valid == 0u) {
        state_defaults();
    } else if (first_valid != 0u
        && (second_valid == 0u || first.generation >= second.generation)) {
        memcpy(&npq_state, &first, sizeof(npq_state));
    } else {
        memcpy(&npq_state, &second, sizeof(npq_state));
    }
    npq_secure_zero(&first, sizeof(first));
    npq_secure_zero(&second, sizeof(second));
}

static void state_persist(void)
{
    npq_state_record record = npq_state;
    npq_state_record *target;
    const uint8_t slot = (uint8_t)((record.generation + 1u) & 1u);
    record.generation++;
    record.magic = NPQ_STATE_MAGIC;
    record.schema = NPQ_STATE_SCHEMA;
    record.crc = crc16_ccitt(
        (const uint8_t *)&record,
        (uint8_t)(sizeof(record) - sizeof(record.crc))
    );
    target = &npq_eeprom_slots[slot];
    eeprom_update_word(&target->magic, 0u);
    eeprom_update_block(
        ((const uint8_t *)&record) + 2u,
        ((uint8_t *)target) + 2u,
        sizeof(record) - 2u
    );
    eeprom_update_word(&target->magic, NPQ_STATE_MAGIC);
    memcpy(&npq_state, &record, sizeof(npq_state));
    npq_secure_zero(&record, sizeof(record));
}

static void uart_init(void)
{
    UCSR0A = _BV(U2X0);
    UBRR0H = 0u;
    UBRR0L = 16u;
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

static void timeout_timer_init(void)
{
    TCCR1A = 0u;
    TCCR1B = _BV(CS12) | _BV(CS10);
}

static void uart_putc(uint8_t value)
{
    while ((UCSR0A & _BV(UDRE0)) == 0u) {
    }
    UDR0 = value;
}

static uint8_t uart_getc_timeout(uint8_t *value)
{
    const uint16_t started = TCNT1;
    while ((UCSR0A & _BV(RXC0)) == 0u) {
        if ((uint16_t)(TCNT1 - started) >= NPQ_UART_TIMEOUT_TICKS) {
            return 0u;
        }
    }
    *value = UDR0;
    return 1u;
}

static uint8_t uart_read_exact(uint8_t *output, size_t length)
{
    while (length-- != 0u) {
        if (uart_getc_timeout(output++) == 0u) {
            return 0u;
        }
    }
    return 1u;
}

static void uart_send_frame(const uint8_t *frame, uint8_t length)
{
    uint8_t index;
    uart_putc(length);
    for (index = 0u; index < length; ++index) {
        uart_putc(frame[index]);
    }
}

static uint8_t uart_read_frame(uint8_t *frame, uint8_t *length)
{
    uint8_t incoming_length;
    if (uart_getc_timeout(&incoming_length) == 0u
        || incoming_length == 0u
        || incoming_length > NPQ_FRAME_MAX_BYTES
        || uart_read_exact(frame, incoming_length) == 0u) {
        *length = 0u;
        return 0u;
    }
    *length = incoming_length;
    return 1u;
}

static int signature_reader(void *context, uint8_t *output, size_t length)
{
    uint8_t ready[6];
    (void)context;
    if (length == 0u || length > UINT16_MAX) {
        return 0;
    }
    ready[0] = 0x4eu;
    ready[1] = 0x51u;
    ready[2] = 1u;
    ready[3] = NPQ_FRAME_ENROLL_CHUNK_READY;
    ready[4] = (uint8_t)(length >> 8u);
    ready[5] = (uint8_t)length;
    uart_send_frame(ready, sizeof(ready));
    return uart_read_exact(output, length) != 0u;
}

static uint8_t flash_equals(
    const uint8_t *bytes,
    const uint8_t *flash_bytes,
    uint8_t length
)
{
    uint8_t difference = 0u;
    uint8_t index;
    for (index = 0u; index < length; ++index) {
        difference |= (uint8_t)(
            bytes[index] ^ pgm_read_byte(flash_bytes + index)
        );
    }
    return difference == 0u;
}

static uint8_t manifest_valid(const uint8_t manifest[48])
{
    return manifest[0] == (uint8_t)'Q'
        && manifest[1] == (uint8_t)'P'
        && manifest[2] == (uint8_t)'C'
        && manifest[3] == (uint8_t)'1'
        && manifest[4] == 1u
        && manifest[5] == 1u
        && manifest[6] == 1u
        && manifest[7] == 2u
        && flash_equals(
            manifest + 8u,
            NPQ_DEVICE_CLASS_ID,
            16u
        ) != 0u
        && flash_equals(
            manifest + 24u,
            NPQ_LEGACY_POLICY_DIGEST,
            16u
        ) != 0u
        && npq_load_u32_be(manifest + 40u) > npq_state.policy_sequence;
}

static void make_control_frame(uint8_t type, uint8_t status)
{
    npq_frame[0] = 0x4eu;
    npq_frame[1] = 0x51u;
    npq_frame[2] = 1u;
    npq_frame[3] = type;
    npq_frame[4] = status;
}

static uint8_t authorize_post_quantum(void)
{
    uint8_t public_key[NPQ_AUTH_PUBLIC_KEY_BYTES];
    uint8_t incoming_length = 0u;
    uint8_t index;
    size_t consumed = 0u;
#ifdef NPQ_AUTH_LMS
    npq_lms_result verification;
#else
    qp_slhdsa_result verification;
#endif
    make_control_frame(NPQ_FRAME_ENROLL_REQUIRED, NPQ_AUTH_ALGORITHM_ID);
    uart_send_frame(npq_frame, 5u);
    if (uart_read_frame(npq_frame, &incoming_length) == 0u
        || incoming_length != NPQ_POLICY_FRAME_BYTES
        || npq_frame[0] != 0x4eu
        || npq_frame[1] != 0x51u
        || npq_frame[2] != 1u
        || npq_frame[3] != NPQ_FRAME_ENROLL_BEGIN
        || manifest_valid(npq_frame + 4u) == 0u) {
        make_control_frame(NPQ_FRAME_ENROLL_RESULT, 0u);
        uart_send_frame(npq_frame, 5u);
        return 0u;
    }
    for (index = 0u; index < sizeof(public_key); ++index) {
#ifdef NPQ_AUTH_LMS
        public_key[index] = pgm_read_byte(NPQ_AUTH_PUBLIC_KEY + index);
#else
        public_key[index] = pgm_read_byte(NPQ_SLHDSA_PUBLIC_KEY + index);
#endif
    }
#ifdef NPQ_AUTH_LMS
    verification = npq_lms_sha256_m32_h5_verify_stream(
        public_key,
        npq_frame + 4u,
        NPQ_POLICY_MANIFEST_BYTES,
        NPQ_AUTH_SIGNATURE_BYTES,
        signature_reader,
        NULL,
        &consumed
    );
#else
    verification = qp_slhdsa_sha2_128s_verify_stream(
        public_key,
        npq_frame + 4u,
        NPQ_POLICY_MANIFEST_BYTES,
        npq_policy_context,
        NPQ_POLICY_CONTEXT_BYTES,
        QP_SLHDSA_SIGNATURE_BYTES,
        signature_reader,
        NULL,
        &consumed
    );
#endif
    npq_secure_zero(public_key, sizeof(public_key));
#ifdef NPQ_AUTH_LMS
    if (verification != NPQ_LMS_OK
        || consumed != NPQ_AUTH_SIGNATURE_BYTES) {
#else
    if (verification != QP_SLHDSA_OK
        || consumed != NPQ_AUTH_SIGNATURE_BYTES) {
#endif
        make_control_frame(NPQ_FRAME_ENROLL_RESULT, 0u);
        uart_send_frame(npq_frame, 5u);
        return 0u;
    }
    npq_state.authorized = 1u;
    npq_state.policy_sequence = npq_load_u32_be(npq_frame + 44u);
    state_persist();
    make_control_frame(NPQ_FRAME_ENROLL_RESULT, 1u);
    uart_send_frame(npq_frame, 5u);
    return 1u;
}

static void load_flash_bytes(
    uint8_t *output,
    const uint8_t *source,
    uint8_t length
)
{
    uint8_t index;
    for (index = 0u; index < length; ++index) {
        output[index] = pgm_read_byte(source + index);
    }
}

static uint8_t establish_session(void)
{
    uint8_t root_key[NPQ_ROOT_KEY_BYTES];
    uint8_t device_id[NPQ_DEVICE_ID_BYTES];
    uint8_t frame_length = 0u;
    int result;
    load_flash_bytes(root_key, NPQ_DEVICE_ROOT, sizeof(root_key));
    load_flash_bytes(device_id, NPQ_DEVICE_ID, sizeof(device_id));
    result = npq_make_hello(
        npq_hello,
        root_key,
        device_id,
        npq_state.boot_epoch
    );
    if (result == NPQ_OK) {
        uart_send_frame(npq_hello, NPQ_HELLO_BYTES);
        if (uart_read_frame(npq_challenge, &frame_length) == 0u
            || frame_length != NPQ_CHALLENGE_BYTES) {
            result = NPQ_ERR_IO;
        }
    }
    if (result == NPQ_OK) {
        result = npq_verify_challenge(
            npq_challenge,
            root_key,
            npq_hello
        );
    }
    if (result == NPQ_OK) {
        result = npq_derive_session(
            &npq_live_session,
            NPQ_ROLE_DEVICE,
            root_key,
            npq_hello,
            npq_challenge
        );
    }
    if (result == NPQ_OK) {
        result = npq_make_finished(
            npq_frame,
            NPQ_FRAME_CLIENT_FINISHED,
            NPQ_ROLE_DEVICE,
            root_key,
            npq_hello,
            npq_challenge
        );
    }
    if (result == NPQ_OK) {
        uart_send_frame(npq_frame, NPQ_FINISHED_BYTES);
        if (uart_read_frame(npq_frame, &frame_length) == 0u
            || frame_length != NPQ_FINISHED_BYTES) {
            result = NPQ_ERR_IO;
        }
    }
    if (result == NPQ_OK) {
        result = npq_verify_finished(
            npq_frame,
            NPQ_FRAME_SERVER_FINISHED,
            NPQ_ROLE_DEVICE,
            root_key,
            npq_hello,
            npq_challenge
        );
    }
    npq_secure_zero(root_key, sizeof(root_key));
    npq_secure_zero(device_id, sizeof(device_id));
    npq_secure_zero(npq_hello, sizeof(npq_hello));
    npq_secure_zero(npq_challenge, sizeof(npq_challenge));
    return result == NPQ_OK;
}

static void adc_init(void)
{
    ADMUX = _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

static uint16_t adc_read_a0(void)
{
    ADMUX = (uint8_t)((ADMUX & 0xf0u) | 0u);
    ADCSRA |= _BV(ADSC);
    while ((ADCSRA & _BV(ADSC)) != 0u) {
    }
    return ADC;
}

static void led_set(uint8_t enabled)
{
    if (enabled != 0u) {
        PORTB |= _BV(PORTB5);
    } else {
        PORTB &= (uint8_t)~_BV(PORTB5);
    }
}

static void send_sensor(void)
{
    const uint16_t sample = adc_read_a0();
    uint8_t frame_length = 0u;
    npq_plaintext[0] = NPQ_PAYLOAD_SENSOR;
    npq_plaintext[1] = (uint8_t)(sample >> 8u);
    npq_plaintext[2] = (uint8_t)sample;
    npq_store_u32_be(npq_plaintext + 3u, npq_state.boot_epoch);
    if (npq_seal(
            &npq_live_session,
            npq_plaintext,
            7u,
            npq_frame,
            &frame_length
        ) == NPQ_OK) {
        uart_send_frame(npq_frame, frame_length);
    }
}

static void send_status(uint8_t accepted)
{
    uint8_t frame_length = 0u;
    npq_plaintext[0] = NPQ_PAYLOAD_STATUS;
    npq_plaintext[1] = accepted;
    npq_plaintext[2] = (PORTB & _BV(PORTB5)) != 0u ? 1u : 0u;
    if (npq_seal(
            &npq_live_session,
            npq_plaintext,
            3u,
            npq_frame,
            &frame_length
        ) == NPQ_OK) {
        uart_send_frame(npq_frame, frame_length);
    }
}

static void receive_command(void)
{
    uint8_t frame_length = 0u;
    uint8_t plaintext_length = 0u;
    uint8_t accepted = NPQ_STATUS_REJECTED;
    if (uart_read_frame(npq_frame, &frame_length) == 0u) {
        return;
    }
    if (npq_open(
            &npq_live_session,
            npq_frame,
            frame_length,
            npq_plaintext,
            &plaintext_length
        ) == NPQ_OK
        && plaintext_length == 2u
        && npq_plaintext[0] == NPQ_PAYLOAD_COMMAND
        && npq_plaintext[1] <= 1u) {
        led_set(npq_plaintext[1]);
        accepted = NPQ_STATUS_ACCEPTED;
    }
    send_status(accepted);
}

static void fatal_blink(void)
{
    for (;;) {
        led_set(1u);
        _delay_ms(150.0);
        led_set(0u);
        _delay_ms(150.0);
    }
}

int main(void)
{
    DDRB |= _BV(DDB5);
    led_set(0u);
    uart_init();
    timeout_timer_init();
    adc_init();
    state_load();
    while (npq_state.authorized == 0u) {
        (void)authorize_post_quantum();
    }
    if (npq_state.boot_epoch == UINT32_MAX) {
        fatal_blink();
    }
    ++npq_state.boot_epoch;
    state_persist();
    if (establish_session() == 0u) {
        fatal_blink();
    }
    for (;;) {
        send_sensor();
        receive_command();
        _delay_ms(500.0);
    }
}
