#include "system_state.h"

int dhcp_run_wrapper();
// int dhcp_wait();
void update_root_topic(char* new_topic_root);
int init_mqtt();
int publish(char* topic, char* payload);
int publish_hello_world();
int publish_heartbeat(struct Si4707_Heartbeat *heartbeat);
int publish_SAME_status(struct Si4707_SAME_Status_FullResponse *status);
int publish_system_state(System_State previous, System_State current);

void set_final_MAC_bytes(uint8_t byte5, uint8_t byte6);