C0PQLINK_ROOT := $(USERMOD_DIR)/..

SRC_USERMOD += \
	$(USERMOD_DIR)/modc0pqlink.c \
	$(C0PQLINK_ROOT)/src/core/keccak.c \
	$(C0PQLINK_ROOT)/src/core/sha256.c \
	$(C0PQLINK_ROOT)/src/core/mlkem512_stream.c \
	$(C0PQLINK_ROOT)/src/core/ascon_aead128.c \
	$(C0PQLINK_ROOT)/src/session/crypto.c \
	$(C0PQLINK_ROOT)/src/session/preflight.c \
	$(C0PQLINK_ROOT)/src/session/fragment.c \
	$(C0PQLINK_ROOT)/src/session/ratchet.c \
	$(C0PQLINK_ROOT)/src/session/migration.c

CFLAGS_USERMOD += -I$(C0PQLINK_ROOT)/include -I$(C0PQLINK_ROOT)/src
