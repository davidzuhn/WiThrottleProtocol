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

#include <ArduinoTime.h>
#include <TimeLib.h>

#include "WiThrottle.h"


#define NEWLINE '\n'
#define PROPERTY_SEPARATOR "<;>"

static const int MIN_SPEED = 0;
static const int MAX_SPEED = 126;


WiThrottle::WiThrottle(bool server):
    server(server),
    heartbeatTimer(Chrono::SECONDS),
    fastTimeTimer(Chrono::SECONDS),
    currentSpeed(0),
    speedSteps(0),
    currentDirection(Forward)
{
    init();
}

void
WiThrottle::init()
{
    stream = NULL;
    memset(inputbuffer, 0, sizeof(inputbuffer));
    nextChar = 0;
    heartbeatPeriod = 0;
    currentFastTime = 0.0;
    fastTimeRate = 0.0;
    locomotiveSelected = false;
    resetChangeFlags();
}

void
WiThrottle::resetChangeFlags()
{
    clockChanged = false;
    heartbeatChanged = false;
    locomotiveChanged = false;
}

void
WiThrottle::connect(Stream *stream)
{
    init();
    this->stream = stream;
}

void
WiThrottle::disconnect()
{
    this->stream = NULL;
}

void
WiThrottle::setDeviceName(String deviceName)
{
    this->deviceName = deviceName;

    String command = "N" + deviceName;
    sendCommand(command);
}

bool
WiThrottle::check()
{
    bool changed = false;
    resetChangeFlags();

    if (stream) {
        // update the fast clock first
        changed |= checkFastTime();
        changed |= checkHeartbeat();

        while(stream->available()) {
            char b = stream->read();
            if (b == NEWLINE) {
                // server sends TWO newlines after each command, we trigger on the
                // first, and this skips the second one
                if (nextChar != 0) {
                    inputbuffer[nextChar] = 0;
                    changed |= processCommand(inputbuffer, nextChar);
                }
                nextChar = 0;
            }
            else {
                inputbuffer[nextChar] = b;
                nextChar += 1;
                if (nextChar == 1023) {
                    inputbuffer[1023] = 0;
                    Serial.print("ERROR LINE TOO LONG: ");
                    Serial.println(inputbuffer);
                    nextChar = 0;
                }
            }
        }

        return changed;

    }
    else {
        return false;
    }
}

void
WiThrottle::sendCommand(String cmd)
{
    if (stream) {
        // TODO: what happens when the write fails?
        stream->println(cmd);
        if (server) {
            stream->println("");
        }
        Serial.print("==> "); Serial.println(cmd);
    }
}


bool
WiThrottle::checkFastTime()
{
    bool changed = true;
    if (fastTimeTimer.hasPassed(1)) { // one real second
        fastTimeTimer.restart();
        if (fastTimeRate == 0.0) {
            clockChanged = false;
        }
        else {
            currentFastTime += fastTimeRate;
            clockChanged = true;
        }
    }

    return changed;
}


int
WiThrottle::fastTimeHours()
{
    time_t now = (time_t) currentFastTime;
    return hour(now);
}


int
WiThrottle::fastTimeMinutes()
{
    time_t now = (time_t) currentFastTime;
    return minute(now);
}



bool
WiThrottle::processLocomotiveAction(char *c, int len)
{
    String remainder(c);  // the leading "MTA" was not passed to this method

    String addrCheck = currentAddress + PROPERTY_SEPARATOR;
    String allCheck = "*";
    allCheck.concat(PROPERTY_SEPARATOR);
    if (remainder.startsWith(addrCheck)) {
        remainder.remove(0, addrCheck.length());
    }
    else if (remainder.startsWith(allCheck)) {
        remainder.remove(0, allCheck.length());
    }

    if (remainder.length() > 0) {
        char action = remainder[0];

        switch (action) {
            case 'F':
                //Serial.printf("processing function state\n");
                processFunctionState(remainder);
                break;
            case 'V':
                processSpeed(remainder);
                break;
            case 's':
                processSpeedSteps(remainder);
                break;
            case 'R':
                processDirection(remainder);
                break;
            default:
                Serial.printf("unrecognized action\n");
                // no processing on unrecognized actions
                break;
        }
        return true;
    }
    else {
        Serial.printf("insufficient action to process\n");
        return false;
    }
}



bool
WiThrottle::processCommand(char *c, int len)
{
    Serial.print("<== ");
    Serial.println(c);

    if (len > 3 && c[0]=='P' && c[1]=='F' && c[2]=='T') {
        return processFastTime(c+3, len-3);
    }
    else if (len > 3 && c[0]=='P' && c[1]=='P' && c[2]=='A') {
        processTrackPower(c+3, len-3);
        return true;
    }
    else if (len > 1 && c[0]=='*') {
        return processHeartbeat(c+1, len-1);
    }
    else if (len > 2 && c[0]=='V' && c[1]=='N') {
        processProtocolVersion(c+2, len-2);
        return true;
    }
    else if (len > 2 && c[0]=='P' && c[1]=='W') {
        processWebPort(c+2, len-2);
        return true;
    }
    else if (len > 8 && c[0]=='M' && c[1]=='T' && c[2]=='A') {
        return processLocomotiveAction(c+3, len-3);
    }
    else {
        // all other commands are explicitly ignored
    }
}



void
WiThrottle::setCurrentFastTime(const String& s)
{
    int t = s.toInt();
    if (currentFastTime == 0.0) {
        Serial.print("set fast time to "); Serial.println(t);
    }
    else {
        Serial.print("updating fast time (should be "); Serial.print(t);
        Serial.print(" is "); Serial.print(currentFastTime);  Serial.println(")");
    }
    currentFastTime = t;
}


bool
WiThrottle::processFastTime(char *c, int len)
{
    // keep this style -- I don't validate the settings and syntax
    // as well as I could, so someday we might return false

    bool changed = false;

    String s(c);

    int p = s.indexOf(PROPERTY_SEPARATOR);
    if (p > 0) {
        String timeval = s.substring(0, p);
        String rate    = s.substring(p+3);

        setCurrentFastTime(timeval);
        fastTimeRate = rate.toFloat();
        Serial.print("set clock rate to "); Serial.println(fastTimeRate);
        changed = true;
    }
    else {
        setCurrentFastTime(s);
        changed = true;
    }

    return changed;
}


bool
WiThrottle::processHeartbeat(char *c, int len)
{
    bool changed = false;
    String s(c);

    heartbeatPeriod = s.toInt();
    if (heartbeatPeriod > 0) {
        heartbeatChanged = true;
        changed = true;
    }
    return changed;
}


void
WiThrottle::processProtocolVersion(char *c, int len)
{
    if (delegate && len > 0) {
        protocolVersion = String(c);
        delegate->receivedVersion(protocolVersion);
    }
}

void
WiThrottle::processWebPort(char *c, int len)
{
    if (delegate && len > 0) {
        String port_string = String(c);
        int port = port_string.toInt();

        delegate->receivedWebPort(port);
    }
}



// the string passed in will look 'F03' (meaning turn off Function 3) or
// 'F112' (turn on function 12)
void
WiThrottle::processFunctionState(const String& functionData)
{
    // F[0|1]nn - where nn is 0-28
    if (delegate && functionData.length() >= 3) {
        bool state = functionData[1]=='1' ? true : false;

        String funcNumStr = functionData.substring(2);
        uint8_t funcNum = funcNumStr.toInt();

        if (funcNum == 0 && funcNumStr != "0") {
            // error in parsing
        }
        else {
            delegate->receivedFunctionState(funcNum, state);
        }
    }
}


void
WiThrottle::processSpeed(const String& speedData)
{
    if (delegate && speedData.length() >= 2) {
        String speedStr = speedData.substring(1);
        int speed = speedStr.toInt();

        if ((speed < MIN_SPEED) || (speed > MAX_SPEED)) {
            speed = 0;
        }

        delegate->receivedSpeed(speed);
    }
}


void
WiThrottle::processSpeedSteps(const String& speedStepData)
{
    if (delegate && speedStepData.length() >= 2) {
        String speedStepStr = speedStepData.substring(1);
        int steps = speedStepStr.toInt();

        if (steps != 1 && steps != 2 && steps != 4 && steps != 8 && steps !=16) {
            // error, not one of the known values
        }
        else {
            delegate->receivedSpeedSteps(steps);
        }
    }
}


void
WiThrottle::processDirection(const String& directionStr)
{
    // R[0|1]
    if (delegate && directionStr.length() == 2) {
        if (directionStr.charAt(1) == '0') {
            currentDirection = Reverse;
        }
        else {
            currentDirection = Forward;
        }

        delegate->receivedDirection(currentDirection);

    }

}



void
WiThrottle::processTrackPower(char *c, int len)
{
    if (delegate) {
        if (len > 0) {
            TrackPower state = PowerUnknown;
            if (c[0]=='0') {
                state = PowerOff;
            }
            else if (c[0]=='1') {
                state = PowerOn;
            }

            delegate->receivedTrackPower(state);
        }
    }
}




bool
WiThrottle::checkHeartbeat()
{
    if (heartbeatPeriod > 0 && heartbeatTimer.hasPassed(0.8 * heartbeatPeriod)) {
        heartbeatTimer.restart();

        sendCommand("*");
        return true;
    }
    else {
        return false;
    }
}


bool
WiThrottle::requireHeartbeat(bool needed)
{
    if (needed) {
        sendCommand("*+");
    }
    else {
        sendCommand("*-");
    }
}

bool
WiThrottle::addLocomotive(String address)
{
    bool ok = false;

    if (address[0] == 'S' || address[0] == 'L') {
        String rosterName = address;  // for now -- could look this up...
        String cmd = "MT+" + address + PROPERTY_SEPARATOR + rosterName;
        sendCommand(cmd);

        currentAddress = address;
        ok = true;
    }

    return ok;
}


bool
WiThrottle::releaseLocomotive()
{
    String cmd = "MT-*";
    cmd.concat(PROPERTY_SEPARATOR);
    sendCommand(cmd);

    return true;
}


bool
WiThrottle::setSpeed(int speed)
{
    if (speed < 0 || speed > 126) {
        return false;
    }

    String cmd = "MTA*";
    cmd.concat(PROPERTY_SEPARATOR);
    cmd.concat("V");
    cmd.concat(String(speed));
    sendCommand(cmd);
    currentSpeed = speed;
    return true;
}


int
WiThrottle::getSpeed()
{
    return currentSpeed;
}


bool
WiThrottle::setDirection(Direction direction)
{
    String cmd = "MTA*";
    cmd.concat(PROPERTY_SEPARATOR);
    cmd.concat("R");
    if (direction == Reverse) {
        cmd += "0";
    }
    else {
        cmd += "1";
    }
    sendCommand(cmd);

    currentDirection = direction;
    return true;
}


Direction
WiThrottle::getDirection()
{
    return currentDirection;
}


bool
WiThrottle::emergencyStop()
{
    String cmd = "MTA*";
    cmd.concat(PROPERTY_SEPARATOR);
    cmd.concat("X");

    sendCommand(cmd);

    return true;
}


void
WiThrottle::setFunction(int funcNum, bool pressed)
{
    if (funcNum < 0 || funcNum > 28) {
        return;
    }

    String cmd = "MTA";
    cmd.concat(currentAddress);
    cmd.concat(PROPERTY_SEPARATOR);
    cmd.concat("F");

    if (pressed) {
        cmd += "1";
    }
    else {
        cmd += "0";
    }

    cmd += funcNum;

    sendCommand(cmd);
}
