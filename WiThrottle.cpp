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


WiThrottle::WiThrottle():
    stream(NULL),
    nextChar(0),
    heartbeatTimer(Chrono::SECONDS),
    heartbeatPeriod(0),
    fastTimeTimer(Chrono::SECONDS),
    currentFastTime(0.0),
    fastTimeRate(0.0)

{
    memset(buffer, 0, sizeof(buffer));
}

void
WiThrottle::resetChangeFlags()
{
    clockChanged = false;
}

void
WiThrottle::connect(Stream *stream)
{
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
                    buffer[nextChar] = 0;
                    changed |= processCommand(buffer, nextChar);
                }
                nextChar = 0;
            }
            else {
                buffer[nextChar] = b;
                nextChar += 1;
                if (nextChar == 1023) {
                    buffer[1023] = 0;
                    Serial.print("ERROR LINE TOO LONG: ");
                    Serial.println(buffer);
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
        stream->println(cmd);
        Serial.print("==> "); Serial.println(cmd);
    }
}


bool
WiThrottle::checkFastTime()
{
    if (fastTimeTimer.hasPassed(1)) {
        fastTimeTimer.restart();
        currentFastTime += fastTimeRate;
        clockChanged = true;
        return true;
    }
    else {
        return false;
    }
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
    else if (len > 1 && c[0]=='*') {
        return processHeartbeat(c+1, len-1);
    }
}





void
WiThrottle::setCurrentFastTime(const String& s)
{
    int t = s.toInt();
    Serial.print("updating fast time (should be "); Serial.print(t);
    Serial.print(" is "); Serial.print(currentFastTime);  Serial.println(")");
    currentFastTime = t;
}


bool
WiThrottle::processFastTime(char *c, int len)
{
    // keep this style -- I don't validate the settings and syntax
    // as well as I could, so someday we might return false

    bool changed = false;

    Serial.print("setting time with ");
    Serial.println(c);

    String s(c);

    int p = s.indexOf("<;>");
    if (p > 0) {
        String timeval = s.substring(0, p);
        String rate    = s.substring(p+3);

        setCurrentFastTime(timeval);
        fastTimeRate = rate.toFloat();
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
        changed = true;
    }
    return changed;
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
