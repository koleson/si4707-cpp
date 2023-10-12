// to remove
void setup_si4707_spi();


// some of this should be private
void reset_si4707();
bool await_si4707_cts(int maxWait);
uint8_t read_status();
void read_resp(uint8_t* resp);
void power_up_si4707();
void get_si4707_rev();
void tune_si4707();
void print_si4707_rsq();
void print_si4707_same_status();

// TODO:

// make a struct for RSQ data
// make a struct for SAME data

struct Si4707_Heartbeat {
	// int i, bool si4707_started, uint8_t rssi, uint8_t snr, bool tune_valid
	unsigned int iteration;
	bool si4707_started;
	uint8_t rssi;
	uint8_t snr;
	bool tune_valid;
};