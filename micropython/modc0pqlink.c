#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"

#include "c0pqlink/c0pqlink.h"

#include <string.h>

typedef struct {
    mp_obj_base_t base;
    c0pq_client client;
    c0_mlkem512_workspace workspace;
    mp_obj_t public_key_source;
    mp_obj_t random_function;
    mp_obj_t send_function;
    mp_obj_t receive_function;
    const uint8_t *public_key_buffer;
    size_t public_key_length;
    bool public_key_is_buffer;
} c0pq_mpy_client_obj;

static void raise_c0_error(int result)
{
    if (result != C0PQLINK_OK) {
        mp_raise_OSError(-result);
    }
}

static uint8_t mpy_read_public_key(void *context, uint16_t offset)
{
    c0pq_mpy_client_obj *self = (c0pq_mpy_client_obj *)context;
    if (self->public_key_is_buffer) {
        return self->public_key_buffer[offset];
    }
    {
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            mp_obj_t result = mp_call_function_1(
                self->public_key_source,
                mp_obj_new_int_from_uint(offset)
            );
            const uint8_t value = (uint8_t)mp_obj_get_int(result);
            nlr_pop();
            return value;
        }
    }
    return 0u;
}

static int mpy_random_bytes(
    void *context,
    uint8_t *output,
    size_t length
)
{
    c0pq_mpy_client_obj *self = (c0pq_mpy_client_obj *)context;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_buffer_info_t buffer;
        mp_obj_t result = mp_call_function_1(
            self->random_function,
            mp_obj_new_int_from_uint(length)
        );
        mp_get_buffer_raise(result, &buffer, MP_BUFFER_READ);
        if (buffer.len != length) {
            nlr_pop();
            return -1;
        }
        memcpy(output, buffer.buf, length);
        nlr_pop();
        return 0;
    }
    return -1;
}

static int mpy_send_frame(
    void *context,
    const uint8_t *frame,
    size_t frame_length
)
{
    c0pq_mpy_client_obj *self = (c0pq_mpy_client_obj *)context;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t result = mp_call_function_1(
            self->send_function,
            mp_obj_new_bytes(frame, frame_length)
        );
        const int status = result == mp_const_none
            ? 0 : mp_obj_get_int(result);
        nlr_pop();
        return status;
    }
    return -1;
}

static int mpy_receive_frame(
    void *context,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length,
    uint32_t timeout_ms
)
{
    c0pq_mpy_client_obj *self = (c0pq_mpy_client_obj *)context;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_buffer_info_t buffer;
        mp_obj_t result = mp_call_function_2(
            self->receive_function,
            mp_obj_new_int_from_uint(frame_capacity),
            mp_obj_new_int_from_uint(timeout_ms)
        );
        if (result == mp_const_none) {
            nlr_pop();
            return -1;
        }
        mp_get_buffer_raise(result, &buffer, MP_BUFFER_READ);
        if (buffer.len > frame_capacity) {
            nlr_pop();
            return -1;
        }
        memcpy(frame, buffer.buf, buffer.len);
        *frame_length = buffer.len;
        nlr_pop();
        return 0;
    }
    return -1;
}

static mp_obj_t c0pq_mpy_client_make_new(
    const mp_obj_type_t *type,
    size_t n_args,
    size_t n_kw,
    const mp_obj_t *args
)
{
    c0pq_mpy_client_obj *self;
    c0pq_client_config config;
    mp_buffer_info_t device_id;
    mp_buffer_info_t psk;
    mp_buffer_info_t key_id;
    mp_buffer_info_t public_key;
    int result;
    mp_arg_check_num(n_args, n_kw, 8u, 11u, false);
    self = mp_obj_malloc(c0pq_mpy_client_obj, type);
    memset((uint8_t *)self + sizeof(self->base), 0,
        sizeof(*self) - sizeof(self->base));
    memset(&config, 0, sizeof(config));
    mp_get_buffer_raise(args[0], &device_id, MP_BUFFER_READ);
    mp_get_buffer_raise(args[1], &psk, MP_BUFFER_READ);
    mp_get_buffer_raise(args[3], &key_id, MP_BUFFER_READ);
    if (device_id.len != C0PQ_DEVICE_ID_BYTES
        || psk.len != C0PQ_PSK_BYTES
        || key_id.len != C0PQ_KEY_ID_BYTES) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad provisioning length"));
    }
    memcpy(config.device_id, device_id.buf, C0PQ_DEVICE_ID_BYTES);
    memcpy(config.psk, psk.buf, C0PQ_PSK_BYTES);
    config.epoch = (uint64_t)mp_obj_get_int(args[2]);
    memcpy(config.public_key_id, key_id.buf, C0PQ_KEY_ID_BYTES);
    self->public_key_source = args[4];
    self->random_function = args[5];
    self->send_function = args[6];
    self->receive_function = args[7];
    self->public_key_is_buffer = mp_get_buffer(
        args[4],
        &public_key,
        MP_BUFFER_READ
    );
    if (self->public_key_is_buffer) {
        if (public_key.len != C0_MLKEM512_PUBLIC_KEY_BYTES) {
            mp_raise_ValueError(MP_ERROR_TEXT("public key must be 800 bytes"));
        }
        self->public_key_buffer = (const uint8_t *)public_key.buf;
        self->public_key_length = public_key.len;
    } else if (!mp_obj_is_callable(args[4])) {
        mp_raise_TypeError(MP_ERROR_TEXT("public key source is not callable"));
    }
    if (!mp_obj_is_callable(args[5])
        || !mp_obj_is_callable(args[6])
        || !mp_obj_is_callable(args[7])) {
        mp_raise_TypeError(MP_ERROR_TEXT("callback is not callable"));
    }
    config.read_public_key = mpy_read_public_key;
    config.public_key_context = self;
    config.random_bytes = mpy_random_bytes;
    config.rng_context = self;
    config.send_frame = mpy_send_frame;
    config.receive_frame = mpy_receive_frame;
    config.transport_context = self;
    config.mode = n_args > 8u
        ? (c0pq_session_mode)mp_obj_get_int(args[8])
        : C0PQ_PQ_BOOTSTRAP_RATCHET;
    config.timeout_ms = n_args > 9u
        ? (uint32_t)mp_obj_get_int(args[9]) : 3000u;
    config.maximum_retries = n_args > 10u
        ? (uint8_t)mp_obj_get_int(args[10]) : 3u;
    result = c0pq_client_init(&self->client, &config);
    raise_c0_error(result);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t c0pq_mpy_client_connect(mp_obj_t self_in)
{
    c0pq_mpy_client_obj *self = MP_OBJ_TO_PTR(self_in);
    raise_c0_error(c0pq_client_connect(&self->client, &self->workspace));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
    c0pq_mpy_client_connect_obj,
    c0pq_mpy_client_connect
);

static mp_obj_t c0pq_mpy_client_seal(mp_obj_t self_in, mp_obj_t data_in)
{
    c0pq_mpy_client_obj *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t data;
    uint8_t frame[C0PQ_FRAME_MAX_BYTES];
    size_t frame_length = 0u;
    mp_get_buffer_raise(data_in, &data, MP_BUFFER_READ);
    raise_c0_error(c0pq_client_seal_record(
        &self->client,
        data.buf,
        data.len,
        frame,
        sizeof(frame),
        &frame_length
    ));
    return mp_obj_new_bytes(frame, frame_length);
}
static MP_DEFINE_CONST_FUN_OBJ_2(
    c0pq_mpy_client_seal_obj,
    c0pq_mpy_client_seal
);

static mp_obj_t c0pq_mpy_client_open(mp_obj_t self_in, mp_obj_t frame_in)
{
    c0pq_mpy_client_obj *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t frame;
    uint8_t plaintext[C0PQ_RECORD_PLAINTEXT_MAX];
    size_t plaintext_length = 0u;
    mp_get_buffer_raise(frame_in, &frame, MP_BUFFER_READ);
    raise_c0_error(c0pq_client_open_record(
        &self->client,
        frame.buf,
        frame.len,
        plaintext,
        sizeof(plaintext),
        &plaintext_length
    ));
    return mp_obj_new_bytes(plaintext, plaintext_length);
}
static MP_DEFINE_CONST_FUN_OBJ_2(
    c0pq_mpy_client_open_obj,
    c0pq_mpy_client_open
);

static mp_obj_t c0pq_mpy_client_close(mp_obj_t self_in)
{
    c0pq_mpy_client_obj *self = MP_OBJ_TO_PTR(self_in);
    c0pq_client_close(&self->client);
    c0pqlink_secure_zero(&self->workspace, sizeof(self->workspace));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
    c0pq_mpy_client_close_obj,
    c0pq_mpy_client_close
);

static mp_obj_t c0pq_mpy_client_state(mp_obj_t self_in)
{
    c0pq_mpy_client_obj *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int((mp_int_t)self->client.state);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
    c0pq_mpy_client_state_obj,
    c0pq_mpy_client_state
);

static const mp_rom_map_elem_t c0pq_mpy_client_locals_table[] = {
    { MP_ROM_QSTR(MP_QSTR_connect),
      MP_ROM_PTR(&c0pq_mpy_client_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_seal), MP_ROM_PTR(&c0pq_mpy_client_seal_obj) },
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&c0pq_mpy_client_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&c0pq_mpy_client_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_state), MP_ROM_PTR(&c0pq_mpy_client_state_obj) },
};
static MP_DEFINE_CONST_DICT(
    c0pq_mpy_client_locals,
    c0pq_mpy_client_locals_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    c0pq_mpy_type_Client,
    MP_QSTR__Client,
    MP_TYPE_FLAG_NONE,
    make_new, c0pq_mpy_client_make_new,
    locals_dict, &c0pq_mpy_client_locals
);

static const mp_rom_map_elem_t c0pq_mpy_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__c0pqlink) },
    { MP_ROM_QSTR(MP_QSTR__Client), MP_ROM_PTR(&c0pq_mpy_type_Client) },
    { MP_ROM_QSTR(MP_QSTR_FULL_PQ_EACH_SESSION),
      MP_ROM_INT(C0PQ_FULL_PQ_EACH_SESSION) },
    { MP_ROM_QSTR(MP_QSTR_PQ_BOOTSTRAP_RATCHET),
      MP_ROM_INT(C0PQ_PQ_BOOTSTRAP_RATCHET) },
    { MP_ROM_QSTR(MP_QSTR_ESTABLISHED),
      MP_ROM_INT(C0PQ_CLIENT_ESTABLISHED) },
    { MP_ROM_QSTR(MP_QSTR_FRAME_MAX), MP_ROM_INT(C0PQ_FRAME_MAX_BYTES) },
    { MP_ROM_QSTR(MP_QSTR_RECORD_MAX),
      MP_ROM_INT(C0PQ_RECORD_PLAINTEXT_MAX) },
};
static MP_DEFINE_CONST_DICT(
    c0pq_mpy_module_globals,
    c0pq_mpy_module_globals_table
);

const mp_obj_module_t c0pq_mpy_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&c0pq_mpy_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__c0pqlink, c0pq_mpy_module);
