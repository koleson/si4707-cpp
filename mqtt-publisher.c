#include <stdio.h>
#include <string.h>

#include <sys/time.h>

#include "port_common.h"

#include "wizchip_conf.h"
#include "w5x00_spi.h"

#include "mqtt_interface.h"
#include "MQTTClient.h"
#include "dhcp.h"
#include "dns.h"

#include "timer.h"
#include "si4707.h"

#include "mqtt-publisher.h"

/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_DHCP 0
#define SOCKET_DNS 1
#define SOCKET_MQTT 2

/* Port */
#define PORT_MQTT 1883

/* Timeout */
#define DEFAULT_TIMEOUT 1000 // 1 second

/* MQTT */
#define MQTT_CLIENT_ID "rpi-pico-si4707"
#define MQTT_USERNAME "wiznet"
#define MQTT_PASSWORD "wizn3t"
#define MQTT_PUBLISH_TOPIC "si4707"
#define MQTT_PUBLISH_PAYLOAD "Hello, World!"
#define MQTT_KEEP_ALIVE 60              // 60 milliseconds

/* Socket */


/* Network */
static wiz_NetInfo g_net_info =
	{
		.mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x99}, // MAC address
		.ip = {0, 0, 0, 0},                     // IP address
		.sn = {255, 255, 255, 0},                    // Subnet Mask
		.gw = {0, 0, 0, 0},                     // Gateway
		.dns = {8, 8, 8, 8},                         // DNS server
		.dhcp = NETINFO_DHCP                       // DHCP enable/disable
};

/* MQTT */
static uint8_t g_mqtt_send_buf[ETHERNET_BUF_MAX_SIZE] = {
	0,
};
static uint8_t g_mqtt_recv_buf[ETHERNET_BUF_MAX_SIZE] = {
	0,
};

static uint8_t g_mqtt_broker_ip[4] = {10, 0, 1, 33};
static Network g_mqtt_network;
static MQTTClient g_mqtt_client;
static MQTTPacket_connectData g_mqtt_packet_connect_data = MQTTPacket_connectData_initializer;
static MQTTMessage g_mqtt_message;

static uint8_t g_ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {
	0,
}; // common buffer

char* root_topic = "si4707";
char* heartbeat_topic_suffix = "heartbeat";

/* DHCP */
static uint8_t g_dhcp_get_ip_flag = 0;

/* DNS */
static uint8_t g_dns_target_domain[] = "orangepi5.tworock.lan";
static uint8_t g_dns_target_ip[4] = {
	0,
};
static uint8_t g_dns_get_ip_flag = 0;

/* Retry count */
#define DHCP_RETRY_COUNT 5
#define DNS_RETRY_COUNT 5

/* Timer  */
static volatile uint32_t g_msec_cnt = 0;

/* Clock */
static void set_clock_khz(void);

/* Timer  */
static void repeating_timer_callback(void);
//static time_t millis(void);

/* DHCP */
static void wizchip_dhcp_init(void);
static void wizchip_dhcp_assign(void);
static void wizchip_dhcp_conflict(void);

uint8_t dhcp_retry = 0;
uint8_t dns_retry = 0;

int dhcp_run_wrapper() {
	return DHCP_run();
}

int dhcp_wait() {
	uint8_t retval;
	
	
	/* Infinite loop */
	while (1)
	{
		/* Assigned IP through DHCP */
		if (g_net_info.dhcp == NETINFO_DHCP)
		{
			retval = DHCP_run();
	
			if (retval == DHCP_IP_LEASED)
			{
				if (g_dhcp_get_ip_flag == 0)
				{
					printf(" DHCP success\n");
	
					g_dhcp_get_ip_flag = 1;
					return 0;
				}
			}
			else if (retval == DHCP_FAILED)
			{
				g_dhcp_get_ip_flag = 0;
				dhcp_retry++;
	
				if (dhcp_retry <= DHCP_RETRY_COUNT)
				{
					printf(" DHCP timeout occurred and retry %d\n", dhcp_retry);
				}
			}
			else if (retval == DHCP_RUNNING)
			{
				puts("DHCP client running, waiting...");
			}
	
			if (dhcp_retry > DHCP_RETRY_COUNT)
			{
				printf(" DHCP failed\n");
				
				DHCP_stop();
				
				// TODO:  extract constant
				return 99;
			}
			printf("dchp_wait: retval = %d", retval);

			wizchip_delay_ms(1000); // wait for 1 second
		}
	}
}

int init_mqtt() {
	puts("mqtt-publisher: init_mqtt()");
	/* Initialize */
	int32_t retval;
	
	set_clock_khz();
	
	stdio_init_all();
	
	puts("initializing wiznet SPI");
	
	wizchip_spi_initialize();

	puts("initializing wiznet CRIS");
	wizchip_cris_initialize();
	
	puts("resetting wiznet chip");
	wizchip_reset();
	puts("initializing wiznet chip");
	wizchip_initialize();
	puts("checking wiznet chip");
	wizchip_check();
	
	wizchip_1ms_timer_initialize(repeating_timer_callback);
	
	puts("initializing network");
	
	if (g_net_info.dhcp == NETINFO_DHCP) // DHCP
	{
		puts("DHCP init....");
		wizchip_dhcp_init();
		puts("DHCP init done.");
	}
	else // static
	{
		network_initialize(g_net_info);
	
		/* Get network information */
		print_network_information(g_net_info);
	}
	
	puts("init_mqtt calling dhcp_wait");
	dhcp_wait();
	puts("init_mqtt dhcp_wait finished");
	
	NewNetwork(&g_mqtt_network, SOCKET_MQTT);
	
	puts("connecting to mqtt server");
	retval = ConnectNetwork(&g_mqtt_network, g_mqtt_broker_ip, PORT_MQTT);
	
	if (retval != 1)
	{
		printf("MQTT Network connect failed\n");
	
		// TODO:  extract constant
		return 98;
	}
	
	/* Initialize MQTT client */
	MQTTClientInit(&g_mqtt_client, &g_mqtt_network, DEFAULT_TIMEOUT, g_mqtt_send_buf, ETHERNET_BUF_MAX_SIZE, g_mqtt_recv_buf, ETHERNET_BUF_MAX_SIZE);
	
	/* Connect to the MQTT broker */
	g_mqtt_packet_connect_data.MQTTVersion = 3;
	g_mqtt_packet_connect_data.cleansession = 1;
	g_mqtt_packet_connect_data.willFlag = 0;
	g_mqtt_packet_connect_data.keepAliveInterval = MQTT_KEEP_ALIVE;
	g_mqtt_packet_connect_data.clientID.cstring = MQTT_CLIENT_ID;
	g_mqtt_packet_connect_data.username.cstring = MQTT_USERNAME;
	g_mqtt_packet_connect_data.password.cstring = MQTT_PASSWORD;
	
	enum returnCode mqtt_connect_retval;
	
	mqtt_connect_retval = MQTTConnect(&g_mqtt_client, &g_mqtt_packet_connect_data);
	
	if (mqtt_connect_retval < 0)
	{
		printf(" MQTT connect failed : %d\n", mqtt_connect_retval);
	
		// TODO:  extract constant
		return 97;
	}
	
	if (mqtt_connect_retval == FAILURE) {
		printf("MQTT Connect Failed: %d", mqtt_connect_retval);
	}
	
	printf(" MQTT connected (mqtt_connect_retval: %d)\n", mqtt_connect_retval);

    publish_hello_world();
	
	puts("mqtt-publisher:  init_mqtt() complete!");
	return 0;
}

int publish_hello_world()
{
	puts("publish_hello_world()");
	int mqtt_retval = publish(MQTT_PUBLISH_TOPIC, MQTT_PUBLISH_PAYLOAD);
	
	return mqtt_retval;
}



void update_root_topic(char* new_topic_root) 
{
	root_topic = new_topic_root;
}

int publish_SAME_status(struct Si4707_SAME_Status_FullResponse *status)
{
	char payload[512] = { 0x00 };
	char* format = "{ "
					"\"CTS\": %d, "
					"\"ERR\": %d, "
					"\"RSQINT\": %d, "
					"\"SAMEINT\": %d, "
					"\"ASQINT\": %d, "
					"\"STCINT\": %d, "
					"\"EOMDET\": %d, "
					"\"SOMDET\": %d, "
					"\"PREDET\": %d, "
					"\"HDRRDY\": %d, "
					"\"STATE\": %d, "
					"\"MSGLEN\": %d, "
					"\"DATA\": \"%s\""
					" }";
	
	sprintf(payload, format,
					status->CTS,
					status->ERR,
					status->RSQINT,
					status->SAMEINT,
					status->ASQINT,
					status->STCINT,
					status->EOMDET,
					status->SOMDET,
					status->PREDET,
					status->HDRRDY,
					status->STATE,
					status->MSGLEN,
					status->DATA
					);

	char topic[32];
	sprintf(topic, "%s/same_status", root_topic);

	int mqtt_retval = publish(topic, payload);

	return mqtt_retval;
}

int publish_heartbeat(struct Si4707_Heartbeat *heartbeat) 
{
	char payload[256] = { 0x00 };
	char* format =  "{ \"iteration\": %d, "
					"\"si4707_booted\": %s, "
					"\"rssi\": %d, "
					"\"snr\": %d }";

	sprintf(payload, format, 
	 					heartbeat->iteration, 
						(heartbeat->si4707_started ? "true" : "false"),
						heartbeat->rssi, 
						heartbeat->snr);
	
	char topic[32];
	sprintf(topic, "%s/%s", root_topic, heartbeat_topic_suffix);
	
	int mqtt_retval = publish(topic, payload);
	
	return mqtt_retval;
}

int publish(char* topic, char* payload) {
	int mqtt_retval = 0;

	/* Configure publish message */
	g_mqtt_message.qos = QOS0;
	g_mqtt_message.retained = 0;
	g_mqtt_message.dup = 0;
	g_mqtt_message.payload = payload;
	g_mqtt_message.payloadlen = strlen(g_mqtt_message.payload);

	mqtt_retval = MQTTPublish(&g_mqtt_client, topic, &g_mqtt_message);

	if (mqtt_retval < 0)
	{
		printf(" Publish failed : %d\n", mqtt_retval);
		return mqtt_retval;
	}
	
	printf("%s Published (%d)\n", topic, mqtt_retval);
	
	if ((mqtt_retval = MQTTYield(&g_mqtt_client, g_mqtt_packet_connect_data.keepAliveInterval)) < 0)
	{
		printf(" Yield error : %d\n", mqtt_retval);
	
		return mqtt_retval;
	}
	
	return mqtt_retval;
}

/* Clock */
static void set_clock_khz(void)
{
	// set a system clock frequency in khz
	set_sys_clock_khz(PLL_SYS_KHZ, true);

	// configure the specified clock
	clock_configure(
		clk_peri,
		0,                                                // No glitchless mux
		CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
		PLL_SYS_KHZ * 1000,                               // Input frequency
		PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
	);
}

/* Timer */
static void repeating_timer_callback(void)
{
	g_msec_cnt++;

	MilliTimer_Handler();
}

//static time_t millis(void)
//{
//	return g_msec_cnt;
//}

/* DHCP */
static void wizchip_dhcp_init(void)
{
	printf(" DHCP client running\n");

	DHCP_init(SOCKET_DHCP, g_ethernet_buf);

	reg_dhcp_cbfunc(wizchip_dhcp_assign, wizchip_dhcp_assign, wizchip_dhcp_conflict);
}

static void wizchip_dhcp_assign(void)
{
	getIPfromDHCP(g_net_info.ip);
	getGWfromDHCP(g_net_info.gw);
	getSNfromDHCP(g_net_info.sn);
	getDNSfromDHCP(g_net_info.dns);

	g_net_info.dhcp = NETINFO_DHCP;

	/* Network initialize */
	network_initialize(g_net_info); // apply from DHCP

	print_network_information(g_net_info);
	printf(" DHCP leased time : %ld seconds\n", getDHCPLeasetime());
}

static void wizchip_dhcp_conflict(void)
{
	printf(" Conflict IP from DHCP\n");

	// halt or reset or any...
	while (1)
		; // this example is halt.
}
