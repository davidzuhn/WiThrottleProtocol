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

typedef enum Direction {
    Reverse = 0,
    Forward = 1
} Direction;

typedef enum TrackPower {
    PowerOff = 0,
    PowerOn = 1,
    PowerUnknown = 2
} TrackPower;



class WiThrottleDelegate
{
  public:
    virtual void receivedVersion(String version) {}

    virtual void fastTimeChanged(uint32_t time) { }
    virtual void fastTimeRateChanged(double rate) { }

    virtual void receivedFunctionState(uint8_t func, bool state) { }

    virtual void receivedSpeed(int speed) { }             // Vnnn
    virtual void receivedDirection(Direction dir) { }     // R{0,1}
    virtual void receivedSpeedSteps(int steps) { }        // snn

    virtual void receivedWebPort(int port) { }            // PWnnnnn
    virtual void receivedTrackPower(TrackPower state) { } // PPAn
};


class WiThrottle
{
  public:
    WiThrottle(bool server = false);

    void connect(Stream *stream);
    void disconnect();

    void setDeviceName(String deviceName);

    bool check();

    int fastTimeHours();
    int fastTimeMinutes();
    bool clockChanged;

    String protocolVersion;
    bool protocolVersionChanged;

    bool requireHeartbeat(bool needed);
    bool heartbeatChanged;

    bool addLocomotive(String address);  // address is [S|L]nnnn (where n is 0-10000)
    bool releaseLocomotive();
    bool locomotiveChanged;

    void setFunction(int funcnum, bool pressed);

    bool setSpeed(int speed);
    bool setDirection(Direction direction);

    WiThrottleDelegate *delegate = NULL;

  private:
    bool server;
    Stream *stream;

    bool processCommand(char *c, int len);
    bool processFastTime(char *c, int len);
    bool processHeartbeat(char *c, int len);
    void processProtocolVersion(char *c, int len);
    void processWebPort(char *c, int len);
    void processTrackPower(char *c, int len);
    void processFunctionState(char *c, int len);

    bool checkFastTime();
    bool checkHeartbeat();

    void sendCommand(String cmd);

    void setCurrentFastTime(const String& s);

    String deviceName;
    char inputbuffer[1024];
    ssize_t nextChar;  // where the next character to be read goes in the buffer

    Chrono heartbeatTimer;
    int heartbeatPeriod;

    Chrono fastTimeTimer;
    double currentFastTime;
    float fastTimeRate;

    void resetChangeFlags();

    void init();

    bool locomotiveSelected = false;
};



#endif // WITHROTTLE_H
