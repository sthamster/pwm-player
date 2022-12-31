/*
* Player for simple MIDI/iMelody/eMelody melodies on pulse-width modulated buzzer (linux /sys/class/pwm/pwmchip0/pwm* devices)
* Copyright (C) 2022 MaxWolf d5713fb35e03d9aa55881eaa23f86fb6f09982ed4da2a59410639a1c9d35bfbf
* SPDX-License-Identifier: GPL-3.0-or-later
* see https://www.gnu.org/licenses/ for license terms
*/
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pwm-player.h"
#include "pwm-player-midi.h"
#include "pwm-player-melody.h"


#define PWM_CHIP_TRIGGER "/sys/class/pwm/pwmchip0/export"
#define PWM_CHIP_PATH "/sys/class/pwm/pwmchip0/pwm"

#define PWM_ENABLE "/enable"
#define PWM_PERIOD "/period"
#define PWM_DUTYCYCLE "/duty_cycle"


__attribute__ ((used)) static char s_RCSVersion[] = "$Id: pwm-player.cpp 285 2022-12-31 14:56:40Z maxwolf $";
__attribute__ ((used)) static char s_RCStag[] = "d5713fb35e03d9aa55881eaa23f86fb6f09982ed4da2a59410639a1c9d35bfbf";
__attribute__ ((used)) static char s_RCSsrc[] = "https://github.com/sthamster/pwm-player";

//
// global shared data
//
bool Debug = false; 
int VolumeChange = 100; 

int PWMDevN = -1;
int PWMEnableF = -1;
int PWMPeriodF = -1;
int PWMDutyCycleF = -1;

//
// (Discover and) Setup PWM device 
//
int SetupHW() {
	int f = -1;
	struct stat fs;
	char pwmDevStr[16];
	char fileName[FILENAME_MAX];

	memset(pwmDevStr, 0, sizeof(pwmDevStr));
	if (PWMDevN != -1) {
		sprintf(pwmDevStr, "%d", PWMDevN);
	} else {
	  char *p = NULL;
    	if ((p = getenv("WB_PWM_BUZZER")) == NULL) {
        	fprintf(stderr, "No WB_PWM_BUZZER environment variable set or PWM device number given\n");	
        	return -1;
        }
        strncpy(pwmDevStr, p, sizeof(pwmDevStr) - 1);
        if (Debug) {
        	printf("WB_PWM_BUZZER points to pwm%s\n", pwmDevStr);
        }
    }

	sprintf(fileName, "%s%s%s", PWM_CHIP_PATH, pwmDevStr, PWM_ENABLE);
	if (stat(fileName, &fs) != 0) {
		if (stat(PWM_CHIP_TRIGGER, &fs) != 0) {
			fprintf(stderr, "No PWM trigger file %s\n", PWM_CHIP_TRIGGER);
			return -1;
		}
		if ((f = open(PWM_CHIP_TRIGGER, O_WRONLY)) < 0) {
			fprintf(stderr, "Error opening PWM trigger file %s(%d): %s\n", PWM_CHIP_TRIGGER, errno, strerror(errno));
			return -1;
		}
		if (write(f, pwmDevStr, strlen(pwmDevStr)) == EOF) {
			close(f);
			fprintf(stderr, "Error writing PWM trigger file %s(%d): %s\n", PWM_CHIP_TRIGGER, errno, strerror(errno));
			return -1;
		}
		close(f);
		if (stat(fileName, &fs) != 0) {
			fprintf(stderr, "Error accessing PWM file %s(%d): %s\n", fileName, errno, strerror(errno));
			return -1;
		}
	}
	if ((PWMEnableF = open(fileName, O_WRONLY)) < 0) {
openerr:
		fprintf(stderr, "Error opening PWM file %s(%d): %s\n", fileName, errno, strerror(errno));
		return -1;
	}
	sprintf(fileName, "%s%s%s", PWM_CHIP_PATH, pwmDevStr, PWM_PERIOD);
	if ((PWMPeriodF = open(fileName, O_WRONLY)) < 0) {
		goto openerr;
	}
	sprintf(fileName, "%s%s%s", PWM_CHIP_PATH, pwmDevStr, PWM_DUTYCYCLE);
	if ((PWMDutyCycleF = open(fileName, O_WRONLY)) < 0) {
		goto openerr;
	}
	return 1;
}

// approximate frequencies of MIDI notes (0 to 127)
static int freqs[128] = {
8,9,9,10,10,11,12,12,13,14,15,15,16,17,18,19,21,22,23,24,26,28,29,31,33,35,37,39,41,44,46,49,52,55,58,62,65,69,73,78,82,87,92,98,104,110,
117,123,131,139,147,156,165,175,185,196,208,220,233,247,262,277,294,311,330,349,370,392,415,440,466,494,523,554,587,622,659,698,740,784,
831,880,932,988,1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976,2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,3951,
4186,4435,4699,4978,5274,5588,5920,6272,6645,7040,7459,7902,8372,8870,9397,9956,10548,11175,11840,12544 };


//
// Start playing of MIDI note 'pitch' with 'velocity' and hold on for duration_us microseconds
//
void Play(int pitch, int velocity, int duration_us) {
	int freq, volume;

	/* freq = (int)round(440 * powf(2, (pitch - 69)/12.0));*/
	if ((pitch >= 0) && (pitch < 128)) {
		freq = freqs[pitch];
	} else {
		freq = 440;
	}

	velocity = (velocity * VolumeChange) / 100;
   	if (velocity < 0) velocity = 0;
   	if (velocity > 127) velocity = 127;

	volume = (velocity * 100) / 127; // midi velocity is in range (0; 127]

	if (Debug) printf("Playing %dHz, vol %d, dur %d\n", freq, volume, duration_us/1000);

	long period;
	char valStr[32];
	int vl;
	period = 1000000000 / freq;
	vl = sprintf(valStr, "%ld\n", period);
	write(PWMPeriodF, valStr, vl);
	vl = sprintf(valStr, "%ld\n", (period * volume) / 200);
	write(PWMDutyCycleF, valStr, vl);
	write(PWMEnableF, "1\n", 2);
	this_thread::sleep_for(microseconds(duration_us));
}

//
// Stop PWM sound
//
void Mute() {
	write(PWMEnableF, "0\n", 2);
}

//
// reclaim resources
//
void Cleanup() {
	if (PWMEnableF > 0) {
		Mute();
		close(PWMEnableF);
		PWMEnableF = -1;
	}
	if (PWMPeriodF > 0) {
		close(PWMPeriodF);
		PWMPeriodF = -1;
	}
	if (PWMDutyCycleF > 0) {
		close(PWMDutyCycleF);
		PWMDutyCycleF = -1;
	}

}


void SigHandler(int n)
{
	signal(SIGTERM, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	exit(2);
}



int main(int argc, char *argv[])
{
    int c;
    bool background = false;
    const char *midiFile = NULL;
    const char *melodyFile = NULL;
    const char *iMelody = NULL;
    const char *eMelody = NULL;
    int startNote = 0;
    int endNote = INT_MAX;
    char *p;
    int trkN = 0;
    string rev("$Revision: 285 $");


    printf("pwm-player v0.1 %s Copyright (C) 2022 by MaxWolf\n", rev.substr(1, rev.length() - 2).c_str());
    while ( (c = getopt(argc, argv, "m:e:E:i:I:bdv:n:p:t:h")) != -1) {
        switch (c) {
        case 'm': // MIDI file
        	midiFile = (optarg);
            break;
        case 'i': // iMelody file
        	melodyFile = optarg;
            break;
        case 'I': // iMelody string
        	iMelody = optarg;
            break;
        case 'e': // eMelody file
        	melodyFile = optarg;
            break;
        case 'E': // eMelody string
        	eMelody = optarg;
            break;
        case 'b': // play melody in background
        	background = true;
            break;
        case 'd': // show debug output
        	Debug = true;
            break;
        case 'v': // volume increase
        	VolumeChange = atoi(optarg);
	        if (Debug) {
	        	printf("Will increase volume by <%d>\n", VolumeChange);
	        }
        	break;
        case 'n': // start and end note numbers
        	sscanf(optarg, " %u", &startNote);
        	if ((p = strchr(optarg, ':')) != 0) {
	        	sscanf(p + 1, " %u", &endNote);
	        }
	        if (Debug) {
	        	printf("Will play notes <%d> to <%d>\n", startNote, endNote);
	        }
        	break;
        case 'p': // PWM device number to use (could also be taken from WB_PWM_BUZZER environment variable
        	if (sscanf(optarg, "%d", &PWMDevN) != 1) {
        		fprintf(stderr, "Invalid PWM device number '%s' given\n", optarg);
        		exit(1);
        	}
	        if (Debug) {
	        	printf("Will use PWM device %d\n", PWMDevN);
	        }
        	break;
        case 't':
        	if (sscanf(optarg, "%d", &trkN) != 1) {
        		fprintf(stderr, "Invalid track number '%s' given\n", optarg);
        		exit(1);
        	}
	        if (Debug) {
	        	printf("Will try to play MIDI track %d\n", trkN);
	        }
	        break;
        
        case '?':
        case 'h':
        	fprintf(stderr, "usage: %s [-p <pwmN>] <-m file.mid>|<-i file.imy>|<-e file.emy>|<-I iMelody>|<-E eMelody> [-d] [-h] [-v <Volume>] [-n [<StartNote>][:<EndNote>] [-t <TrackN>]\n", argv[0]);
        	exit(1);
            break;
        default:
            //fprintf(stderr, "getopt returned character code 0x%x", c);
        	break;
        }
    }

	if (!midiFile && !melodyFile && !eMelody && !iMelody) {
		fprintf(stderr, "No melody specified\n");	
		exit(1);
	}

	atexit(Cleanup);
	if (SetupHW() < 0) {
		exit(1);
	}

	if (midiFile) {
	    printf("Playing MIDI file %s\n", midiFile);
    	if (PrepareMIDIFile(midiFile) < 0) {
    		exit(1);
    	}
    } else if (melodyFile) {
	    printf("Playing iMelody/eMelody file %s\n", melodyFile);
    	if (PrepareMelodyFile(melodyFile) < 0) {
    		exit(1);
    	}
    } else if (iMelody) {
	    printf("Playing iMelody %s\n", iMelody);
    	if (PrepareMelodyString(iMelody, 'I') < 0) {
    		exit(1);
    	}
    } else if (eMelody) {
	    printf("Playing eMelody %s\n", eMelody);
    	if (PrepareMelodyString(eMelody, 'E') < 0) {
    		exit(1);
    	}
    } else {
        exit(2);
    }

	if (background) {
		int rc = fork();
		if (rc < 0) {
			fprintf(stderr, "Unable to fork child process (%d): %s", errno, strerror(errno));
			exit(1);
		} else if (rc != 0) {
			exit(0);
		}
	}

	signal( SIGHUP, SigHandler );
	signal( SIGINT,  SigHandler );
	signal( SIGTERM, SigHandler );
	signal( SIGUSR1, SigHandler );

	if (midiFile) {
		if (PlayMIDIFile(trkN, startNote, endNote) < 0) {
			CleanupMIDIFile();
			exit(1);
		}
    	CleanupMIDIFile();
    	exit(0);
	}
	if (PlayMelody() < 0) {
		CleanupMelody();
		exit(1);
	}
	CleanupMelody();
	return 0;
}
