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

#include <Time.h>
#include <TimeLib.h>

#include "WiThrottle.h"


#define NEWLINE '\n'



WiThrottle::WiThrottle(bool server):
    server(server),
    heartbeatTimer(Chrono::SECONDS),
    fastTimeTimer(Chrono::SECONDS)
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
    else if (len > 8 && c[0]=='M' && c[1]=='T' && c[2]=='A' && c[3]=='1' &&
             c[4]=='<' && c[5]==';' && c[6]=='>' && c[7]=='F') {
        processFunctionState(c+8, len-8);
        return true;
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

    int p = s.indexOf("<;>");
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


void
WiThrottle::processFunctionState(char *c, int len)
{
    if (delegate && len >= 2) {
        bool state = c[0]=='1' ? true : false;

        String funcNumStr = String(&c[1]);
        uint8_t funcNum = funcNumStr.toInt();

        Serial.print("func number:"); Serial.print(funcNum);
        Serial.print(" state:"); Serial.println(state ? "ON" : "OFF");

        delegate->receivedFunctionState(funcNum, state);
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
        String cmd = "MT+1<;>" + address;
        sendCommand(cmd);
        ok = true;
    }

    return ok;
}

bool
WiThrottle::releaseLocomotive()
{
    String cmd = "MT-*<;>";
    sendCommand(cmd);

    return true;
}


bool
WiThrottle::setSpeed(int speed)
{
    if (speed < 0 || speed > 126) {
        return false;
    }

    String cmd = "MTA*<;>V" + String(speed);
    sendCommand(cmd);
    return true;
}


bool
WiThrottle::setDirection(Direction direction)
{
    String cmd = "MTA*<;>R";
    if (direction == Reverse) {
        cmd += "0";
    }
    else {
        cmd += "1";
    }
    sendCommand(cmd);

    return true;
}


void
WiThrottle::setFunction(int funcNum, bool pressed)
{
    if (funcNum < 0 || funcNum > 28) {
        return;
    }

    String cmd = "MTA1<;>F";

    if (pressed) {
        cmd += "1";
    }
    else {
        cmd += "0";
    }

    cmd += funcNum;

    sendCommand(cmd);
}
