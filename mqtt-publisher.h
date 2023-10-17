void update_root_topic(char* new_topic_root);
int init_mqtt();
int publish(char* topic, char* payload);
int publish_helloworld();
int publish_heartbeat(struct Si4707_Heartbeat *heartbeat);
int publish_SAME_status(struct Si4707_SAME_Status_FullResponse *status);