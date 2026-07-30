#include "c0pqlink/c0pqlink.h"

#include <stdio.h>

int main(void)
{
    printf(
        "mlkem_workspace_internal=%lu\n"
        "mlkem_workspace_public_bound=%u\n"
        "client_context_this_abi=%lu\n"
        "frame_max=%u\n"
        "record_plaintext_max=%u\n",
        (unsigned long)c0_mlkem512_workspace_bytes(),
        (unsigned int)C0_MLKEM512_WORKSPACE_BYTES,
        (unsigned long)sizeof(c0pq_client),
        (unsigned int)C0PQ_FRAME_MAX_BYTES,
        (unsigned int)C0PQ_RECORD_PLAINTEXT_MAX
    );
    return 0;
}
