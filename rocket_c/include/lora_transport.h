#ifndef LORA_TRANSPORT_H
#define LORA_TRANSPORT_H

#include <stdbool.h>

bool lora_init_tx(void);
bool lora_is_ready(void);
bool lora_send_json(const char *json_payload);

#endif /* LORA_TRANSPORT_H */
