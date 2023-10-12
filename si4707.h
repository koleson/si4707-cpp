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

struct Si4707_RSQ_Status {
	// unsigned int CTS: 		1;
	// unsigned int ERR: 		1;
	// unsigned int RSQINT: 	1;
	// unsigned int SAMEINT: 	1;
	// unsigned int ASQINT:	1;
	// unsigned int SNRHINT:	1;
	// unsigned int SNRLINT:	1;
	// unsigned int RSSIHINT:  1;
	// unsigned int RSSILINT:	1;
	// unsigned int AFCRL:		1;
	// unsigned int VALID:		1;
	uint8_t RSSI;
	uint8_t ASNR;
	uint8_t FREQOFF;
};

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

void get_si4707_rsq(struct Si4707_RSQ_Status *rsq_status);

// TODO:

