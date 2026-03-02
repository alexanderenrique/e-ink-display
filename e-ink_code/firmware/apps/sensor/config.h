#ifndef SENSOR_APP_CONFIG_H
#define SENSOR_APP_CONFIG_H

#include "../../core/hardware_config.h"

// SHT31 I2C address (default)
#define SHT31_I2C_ADDR 0x44

// Default Nemo API endpoint
#define SENSOR_APP_DEFAULT_NEMO_URL "https://nemo.stanford.edu/api/sensors/sensor_data/"

// Default NTP time server and timezone for created_date (Pacific: PST/PDT)
#define SENSOR_APP_DEFAULT_TIME_SERVER "pool.ntp.org"
#define SENSOR_APP_DEFAULT_GMT_OFFSET_SEC  (-8 * 3600)   // PST
#define SENSOR_APP_DEFAULT_DAYLIGHT_OFFSET_SEC 3600      // 1 hour for PDT
#define SENSOR_APP_DEFAULT_TIMEZONE_OFFSET "-08:00"      // ISO suffix (PST)

#endif // SENSOR_APP_CONFIG_H
