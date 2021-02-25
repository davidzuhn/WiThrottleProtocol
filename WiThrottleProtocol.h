/* -*- c++ -*-
 *
 * WiThrottleProtocolProtocol
 *
 * This package implements a WiThrottleProtocol protocol connection,
 * allow a device to communicate with a JMRI server or other
 * WiThrottleProtocol device (like the Digitrax LNWI).
 *
 * Copyright © 2018-2019, 2021 Blue Knobby Systems Inc.
 *
 * This work is licensed under the Creative Commons Attribution-ShareAlike
 * 4.0 International License. To view a copy of this license, visit
 * http://creativecommons.org/licenses/by-sa/4.0/ or send a letter to
 * Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 *
 * Attribution — You must give appropriate credit, provide a link to the
 * license, and indicate if changes were made. You may do so in any
 * reasonable manner, but not in any way that suggests the licensor
 * endorses you or your use.
 *
 * ShareAlike — If you remix, transform, or build upon the material, you
 * must distribute your contributions under the same license as the
 * original.
 *
 * All other rights reserved.
 *
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



class WiThrottleProtocolDelegate
{
  public:
    virtual void receivedVersion(String version) {}

    virtual void fastTimeChanged(uint32_t time) { }
    virtual void fastTimeRateChanged(double rate) { }

    virtual void heartbeatConfig(int seconds) { }

    virtual void receivedFunctionState(uint8_t func, bool state) { }

    virtual void receivedSpeed(int speed) { }             // Vnnn
    virtual void receivedDirection(Direction dir) { }     // R{0,1}
    virtual void receivedSpeedSteps(int steps) { }        // snn

    virtual void receivedWebPort(int port) { }            // PWnnnnn
    virtual void receivedTrackPower(TrackPower state) { } // PPAn

    virtual void addressAdded(String address, String entry) { }  // MT+addr<;>roster entry
    virtual void addressRemoved(String address, String command) { } // MT-addr<;>[dr]
    virtual void addressStealNeeded(String address, String entry) { } // MTSaddr<;>addr
};


class WiThrottleProtocol
{
  public:
    WiThrottleProtocol(bool server = false);

    void begin(Stream *console);

    void connect(Stream *stream);
    void disconnect();

    void setDeviceName(String deviceName);
    void setDeviceID(String deviceId);

    bool check();

    int fastTimeHours();
    int fastTimeMinutes();
    float fastTimeRate();
    bool clockChanged;

    void requireHeartbeat(bool needed=true);
    bool heartbeatChanged;

    bool addLocomotive(String address);  // address is [S|L]nnnn (where n is 0-10000)
    bool stealLocomotive(String address);   // address is [S|L]nnnn (where n is 0-10000)
    bool releaseLocomotive(String address = "*");

    void setFunction(int funcnum, bool pressed);

    bool setSpeed(int speed);
    int getSpeed();
    bool setDirection(Direction direction);
    Direction getDirection();

    void emergencyStop();

    WiThrottleProtocolDelegate *delegate = NULL;

  private:
    bool server;
    Stream *stream;
    Stream *console;

    bool processCommand(char *c, int len);
    bool processLocomotiveAction(char *c, int len);
    bool processFastTime(char *c, int len);
    bool processHeartbeat(char *c, int len);
    void processProtocolVersion(char *c, int len);
    void processWebPort(char *c, int len);
    void processTrackPower(char *c, int len);
    void processFunctionState(const String& functionData);
    void processSpeedSteps(const String& speedStepData);
    void processDirection(const String& speedStepData);
    void processSpeed(const String& speedData);
    void processAddRemove(char *c, int len);
    void processStealNeeded(char *c, int len);

    bool checkFastTime();
    bool checkHeartbeat();

    void sendCommand(String cmd);

    void setCurrentFastTime(const String& s);

    char inputbuffer[1024];
    ssize_t nextChar;  // where the next character to be read goes in the buffer

    Chrono heartbeatTimer;
    int heartbeatPeriod;

    Chrono fastTimeTimer;
    double currentFastTime;
    float currentFastTimeRate;

    void resetChangeFlags();

    void init();

    bool locomotiveSelected = false;

    String currentAddress;

    int currentSpeed;
    int speedSteps;  // 1=128, 2=28, 4=27, 8=14, 16=28Mot
    Direction currentDirection;
};



#endif // WITHROTTLE_H
