#pragma once

// Generic commands.
#define SI4735_CMD_POWER_UP			0x01
#define SI4735_CMD_GET_REV			0x10
#define SI4735_CMD_POWER_DOWN			0x11
#define SI4735_CMD_SET_PROPERTY			0x12
#define SI4735_CMD_GET_PROPERTY			0x13
#define SI4735_CMD_GET_INT_STATUS		0x14
#define SI4735_CMD_PATCH_ARGS			0x15
#define SI4735_CMD_PATCH_DATA			0x16

// Commands for the FM/RDS receiver.
#define SI4735_CMD_FM_TUNE_FREQ			0x20
#define SI4735_CMD_FM_SEEK_START		0x21
#define SI4735_CMD_FM_TUNE_STATUS		0x22
#define SI4735_CMD_FM_RSQ_STATUS		0x23
#define SI4735_CMD_FM_RDS_STATUS		0x24
#define SI4735_CMD_FM_AGC_STATUS		0x27
#define SI4735_CMD_FM_AGC_OVERRIDE		0x28

// Commands for the AM/SW/LW receiver.
#define SI4735_CMD_AM_TUNE_FREQ			0x40
#define SI4735_CMD_AM_SEEK_START		0x41
#define SI4735_CMD_AM_TUNE_STATUS		0x42
#define SI4735_CMD_AM_RSQ_STATUS		0x43
#define SI4735_CMD_AM_AGC_STATUS		0x47
#define SI4735_CMD_AM_AGC_OVERRIDE		0x48

// GPIO commands.
#define SI4735_CMD_GPIO_CTL			0x80
#define SI4735_CMD_GPIO_SET			0x81

// POWER_UP definitions.
#define SI4735_CMD_POWER_UP_FUNC_FM_RECV	0x00
#define SI4735_CMD_POWER_UP_FUNC_AM_RECV	0x01
#define SI4735_CMD_POWER_UP_OPMODE_ANALOG_OUT	0x05
#define SI4735_CMD_POWER_UP_OPMODE_DIGITAL_OUT	0x0B
#define SI4735_CMD_POWER_UP_OPMODE_DIGITAL_OUTS	0xB0
#define SI4735_CMD_POWER_UP_OPMODE_BOTH_OUTS	0xB5
