// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
// NB:  these are default pins and the pins used by the Wiznet W5500 pico.  kmo 9 oct 2023 17h42
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

// I2C defines - not defaults, specific to Wiznet W5500 setup
#define I2C_PORT i2c1
#define I2C_SDA 26
#define I2C_SCL 27


// Si4707 defines
#define SI4707_RESET 28

// default 0x63, can be set to 0x11 by pulling SEN low during reset
#define SI4707_ADDR 0x63
