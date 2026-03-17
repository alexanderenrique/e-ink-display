#ifndef SENSOR_APP_CONFIG_H
#define SENSOR_APP_CONFIG_H

#include "../../core/hardware_config.h"

// SHT31 I2C address (default)
#define SHT31_I2C_ADDR 0x44

// Default Nemo API endpoint
#define SENSOR_APP_DEFAULT_NEMO_URL "https://nemo.stanford.edu/api/sensors/sensor_data/"

// Default NTP time server (UTC) and timezone rules (DST handled locally via TZ rule)
#define SENSOR_APP_DEFAULT_TIME_SERVER "pool.ntp.org"
// When using TZ rules, keep NTP sync in UTC (offsets 0); localtime() uses TZ rules for DST.
#define SENSOR_APP_DEFAULT_GMT_OFFSET_SEC  0
#define SENSOR_APP_DEFAULT_DAYLIGHT_OFFSET_SEC 0

// Timezone selection values expected from configuration UI
#define SENSOR_APP_TIMEZONE_PACIFIC   "pacific"
#define SENSOR_APP_TIMEZONE_MOUNTAIN  "mountain"
#define SENSOR_APP_TIMEZONE_CENTRAL   "central"
#define SENSOR_APP_TIMEZONE_EASTERN   "eastern"
#define SENSOR_APP_TIMEZONE_ARIZONA   "arizona"
#define SENSOR_APP_TIMEZONE_UTC       "utc"

// POSIX TZ rule strings (US DST rules where applicable)
#define SENSOR_APP_TZ_RULE_PACIFIC   "PST8PDT,M3.2.0/2,M11.1.0/2"
#define SENSOR_APP_TZ_RULE_MOUNTAIN  "MST7MDT,M3.2.0/2,M11.1.0/2"
#define SENSOR_APP_TZ_RULE_CENTRAL   "CST6CDT,M3.2.0/2,M11.1.0/2"
#define SENSOR_APP_TZ_RULE_EASTERN   "EST5EDT,M3.2.0/2,M11.1.0/2"
#define SENSOR_APP_TZ_RULE_ARIZONA   "MST7"
#define SENSOR_APP_TZ_RULE_UTC       "UTC0"

#define SENSOR_APP_DEFAULT_TIMEZONE SENSOR_APP_TIMEZONE_PACIFIC
#define SENSOR_APP_DEFAULT_TZ_RULE SENSOR_APP_TZ_RULE_PACIFIC

#endif // SENSOR_APP_CONFIG_H
