/* WiThrottle
 *
 * This package implements a WiThrottle protocol connection,
 * allow a device to communicate with a JMRI server or other
 * WiThrottle device (like the Digitrax LNWI).
 *
 * Copyright 2018 by david d zuhn <zoo@blueknobby.com>
 *
 * This work is licensed under the Creative Commons Attribution-ShareAlike
 * 4.0 International License. To view a copy of this license, visit
 * http://creativecommons.org/licenses/by-sa/4.0/deed.en_US.
 *
 * You may use this work for any purposes, provided that you make your
 * version available to anyone else.
 */

#ifndef WITHROTTLE_H
#define WITHROTTLE_H

#include "Arduino.h"
#include "Chrono.h"

class WiThrottle
{
  public:
    WiThrottle();

    void connect(Stream *stream);
    void disconnect();

    void setDeviceName(String deviceName);

    bool check();

    int fastTimeHours();
    int fastTimeMinutes();

    bool clockChanged;

  private:
    Stream *stream;

    bool processCommand(char *c, int len);
    bool processFastTime(char *c, int len);
    bool processHeartbeat(char *c, int len);

    bool checkFastTime();
    bool checkHeartbeat();

    void sendCommand(String cmd);

    void setCurrentFastTime(const String& s);

    String deviceName;
    char buffer[1024];
    ssize_t nextChar;

    Chrono heartbeatTimer;
    int heartbeatPeriod;

    Chrono fastTimeTimer;
    double currentFastTime;
    float fastTimeRate;

    void resetChangeFlags();
};



#endif // WITHROTTLE_H
