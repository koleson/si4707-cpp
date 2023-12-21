#define VERSION "2023.12.6"

// ETHERNET
#define MAC     {0x00, 0x08, 0xDC, 0x12, 0x00, 0x00}

/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

// MQTT
#define MQTT_BROKER_IP {10, 0, 1, 33}
#define MQTT_BROKER_HOSTNAME "orangepi5.tworock.lan"    // not yet in use.
#define MQTT_PORT 1883
#define MQTT_USERNAME "wiznet"
#define MQTT_PASSWORD "wizn3t"

#define MQTT_CLIENT_ID "rpi-pico-si4707"

#define MQTT_ROOT_TOPIC "si4707"
#define MQTT_PUBLISH_PAYLOAD "Hello, World!"
#define MQTT_KEEP_ALIVE 60              // 60 milliseconds
