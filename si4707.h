// to remove
void setup_si4707_spi();


// some of this should be private
void reset_si4707();
void await_si4707_cts();
uint8_t read_status();
void read_resp(uint8_t* resp);
void power_up_si4707();
void get_si4707_rev();
void tune_si4707();
void print_si4707_rsq();
void print_si4707_same_status();