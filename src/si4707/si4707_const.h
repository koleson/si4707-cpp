// Si4707 i2c addresses - ADDR1 is default
#define SI4707_I2C_ADDR0			0x11
#define SI4707_I2C_ADDR1			0x63

// Si4707 useful constants
#define SI4707_STATUS_CTS			0x80
#define SI4707_SPI_SEND_CMD			0x48
#define SI4707_SPI_READ1_GPO1		0xA0
#define SI4707_SPI_READ16_GPO1		0xE0

// Si4707 Commands

#define SI4707_CMD_POWER_UP 		0x01
#define SI4707_CMD_GET_REV 			0x10
#define SI4707_CMD_POWER_DOWN 		0x11
#define SI4707_CMD_SET_PROPERTY		0x12
#define SI4707_CMD_GET_PROPERTY		0x13
#define SI4707_CMD_GET_INT_STATUS  	0x14
#define SI4707_CMD_PATCH_ARGS		0x15
#define SI4707_CMD_PATCH_DATA		0x16

#define SI4707_CMD_WB_TUNE_FREQ		0x50
#define SI4707_CMD_WB_TUNE_STATUS	0x52
#define SI4707_CMD_WB_RSQ_STATUS 	0x53
#define SI4707_CMD_WB_SAME_STATUS	0x54
#define SI4707_CMD_WB_ASQ_STATUS	0x55
#define SI4707_CMD_WB_AGC_STATUS	0x57
#define SI4707_CMD_WB_AGC_OVERRIDE	0x58

#define SI4707_CMD_GPO_CTL			0x80
#define SI4707_CMD_GPO_SET			0x81

// Si4707 Properties

// defaults to 0x0000
#define SI4707_PROP_GPO_IEN 					0x0001

// defaults to 0x8000
#define SI4707_PROP_REFCLK_FREQ 				0x0201

// defaults to 0x0001
#define SI4707_PROP_REFCLK_PRESCALE				0x0202

// defaults to 0x003f
#define SI4707_PROP_RX_VOLUME					0x4000

// defaults to 0x0000
#define SI4707_PROP_RX_HARD_MUTE				0x4001

// defaults to 0x000a
#define SI4707_PROP_WB_MAX_TUNE_ERROR			0x5108

// defaults to 0x0000
#define SI4707_PROP_WB_RSQ_INT_SOURCE			0x5200

// defaults to 0x007f
#define SI4707_PROP_WB_RSQ_SNR_HI_THRESH		0x5201

// defaults to 0x0000
#define SI4707_PROP_WB_RSQ_SNR_LO_THRESH		0x5202

// defaults to 0x007f
#define SI4707_PROP_WB_RSQ_RSSI_HI_THRESH		0x5203

// defaults to 0x0000
#define SI4707_PROP_WB_RSQ_RSSI_LO_THRESH		0x5204

// defaults to 0x0003
#define SI4707_PROP_WB_VALID_SNR_THRESH			0x5403

// defaults to 0x0014
#define SI4707_PROP_WB_VALID_RSSI_THRESH		0x5404

// defaults to 0x0000
#define SI4707_PROP_WB_SAME_INTERRUPT_SOURCE	0x5500

// Si4707 SAME machine states
// state when device has booted but not received anything; state after EOM is received
#define SI4707_SAME_STATE_END_OF_MESSAGE 		0

// state when device is receiving the calibration sequence for AFSK modem
#define SI4707_SAME_STATE_PREAMBLE_DETECTED 	1

// state when device is receiving SAME header (i.e. after ZCZC attention signal)
// see also SOMDET (start of message detected)
#define SI4707_SAME_STATE_RECEIVING_HEADER		2

// state when device has received an entire SAME header
// see also HDRRDY (header ready)
#define SI4707_SAME_STATE_HEADER_COMPLETE		3

// TODO:  this list of available properties is probably not complete.  check AN332.  kmo 9 oct 2023
#define CTS_WAIT 250

