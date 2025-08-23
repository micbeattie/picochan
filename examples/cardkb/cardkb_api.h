typedef struct cardkb_dev_config {
        uint16_t        timeout_cs; // timeout in 1/100 secs (ffff for forever)
        unsigned char   eol;        // end-of-line char or 0
        uint8_t         minread;    // only send when have >= minread bytes
} cardkb_dev_config_t;

//! CCW operations codes

//! PCH_CCW_CMD_READ (0x02) reads the current cardkb buffer,
//! blocking if necessary until either minread bytes have been
//! buffered, the eol character has been seen (if non-zero) or until
//! timeout_cs centiseconds have passed (if less than 0xffff).

//! CARDKB_CCW_CMD_GET_CONFIG reads the current cardkb_dev_config
//! configuration "register" of the device.
#define CARDKB_CCW_CMD_GET_CONFIG    0x04

//! CARDKB_CCW_CMD_SET_CONFIG sets the current cardkb_dev_config
//! configuration "register" of the device.
#define CARDKB_CCW_CMD_SET_CONFIG    0x03

//! CARDKB_TIMEOUT_NEVER is the constant 0xffff which, when set as
//! the timeout_cs field in a cardkb_dev_config configuration,
//! causes a PCH_CCW_CMD_READ timeout never to expire.
#define CARDKB_TIMEOUT_NEVER         0xffff
