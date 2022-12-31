/*
* Player for simple MIDI/iMelody/eMelody melodies on pulse-width modulated buzzer (linux /sys/class/pwm/pwmchip0/pwm* devices)
* Copyright (C) 2022 MaxWolf d5713fb35e03d9aa55881eaa23f86fb6f09982ed4da2a59410639a1c9d35bfbf
* SPDX-License-Identifier: GPL-3.0-or-later
* see https://www.gnu.org/licenses/ for license terms
*/
#include <iostream>
#include <cstdio>
#include <cstring>
#include <climits>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <string>
#include <map>
#include <vector>

using namespace std;

using std::chrono::time_point;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::chrono::microseconds;
using std::chrono::steady_clock;


typedef std::chrono::steady_clock TClock;
typedef std::chrono::time_point<TClock> TPoint;

#define TP2MSEC(tp) std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count()
#define NOW TClock::now()
#define NOWMSEC TP2MSEC(NOW)


extern bool Debug;
void Play(int pitch, int velocity, int duration_us);
void Mute();

