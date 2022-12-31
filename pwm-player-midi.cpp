/*
 * MIDI player
 * Copyright (C) 2022 MaxWolf d5713fb35e03d9aa55881eaa23f86fb6f09982ed4da2a59410639a1c9d35bfbf
 * SPDX-License-Identifier: GPL-3.0-or-later
 * see https://www.gnu.org/licenses/ for license terms
 * based on sources from https://code.soundsoftware.ac.uk/projects/midifile/repository
 */
#include "pwm-player.h"
#include "pwm-player-midi.h"

#include "MIDIEvent.h"
#include "MIDIComposition.h"
#include "MIDIFileReader.h"
using namespace MIDIConstants;

__attribute__ ((used)) static char s_RCSVersion[] = "$Id: pwm-player-midi.cpp 285 2022-12-31 14:56:40Z maxwolf $";
__attribute__ ((used)) static char s_RCSsrc[] = "https://github.com/sthamster/pwm-player";

MIDIFileReader *Fr = NULL;

int PrepareMIDIFile(const char *filename) {

    Fr = new MIDIFileReader(filename);
    if (Fr == NULL || !Fr->isOK()) {
    	fprintf(stderr, "MIDI file error: %s\n", Fr->getError().c_str());
		return -1;
    }

    if (Debug) {
      MIDIComposition &cmp = Fr->getComposition();
      int td = Fr->getTimingDivision(); // ticks per beat (or parts per quarter note)
        switch (Fr->getFormat()) {
    		case MIDI_SINGLE_TRACK_FILE: printf("Format: MIDI Single Track File\n"); break;
    		case MIDI_SIMULTANEOUS_TRACK_FILE: printf("Format: MIDI Simultaneous Track File\n"); break;
    		case MIDI_SEQUENTIAL_TRACK_FILE: printf("Format: MIDI Sequential Track File\n"); break;
    		default: printf("Format: Unknown MIDI file format?\n"); break;
        }
        printf("Tracks: %d\n", (int)cmp.size());
        if (td < 32768) {
    		printf("Timing division: %d ppq\n", Fr->getTimingDivision());
        } else {
    		int frames = 256 - (td >> 8);
    		int subframes = td & 0xff;
    		printf("SMPTE timing: %d fps, %d subframes\n", frames, subframes);
        }
	}
	return 1;
}


int PlayMIDIFile(unsigned int trackN, int startNote, int endNote) {
	// generic rules
	//
	// freq = 440 * (2^((midiNote-69)/12))
	// tempo = microseconds per beat
	// TimingDivision (ppq)= ticks per beat
	// events duration is in ticks
	//
	//
	// 1000000000 / freq => pwm/period
	// (period/2) * (volume/100) => pwm/duty_cycle

	//unsigned int lastT = 0;
    MIDIComposition &cmp = Fr->getComposition();
    microseconds upt = microseconds(1000); // us per tick
    long tempo = 500000; // default microseconds per beat (beat is a quarter note)
    int td = Fr->getTimingDivision(); // ticks per beat (or parts per quarter note)
    int noteN;

	if ((startNote != -1) && (endNote == -1)) {
		endNote = INT_MAX;
	}
	noteN = 1;
	if (trackN > 0) {
    	if (trackN >= cmp.size()) {
    		trackN = cmp.size() - 1;
    	}
		if (Debug) {
			printf("Playing track %d\n", trackN);
		}
	}
	for (MIDITrack::const_iterator j = cmp[trackN].begin(); j != cmp[trackN].end(); ++j) {

		unsigned int t = j->getTime();
		int ch = j->getChannelNumber();

		if (j->isMeta()) {
			int code = j->getMetaEventCode();
			string name;
			bool printable = true;
			switch (code) {

			case MIDI_END_OF_TRACK:
				if (Debug) printf("%u: End of track\n", t);
				break;

			case MIDI_TEXT_EVENT: name = "Text"; break;
			case MIDI_COPYRIGHT_NOTICE: name = "Copyright"; break;
			case MIDI_TRACK_NAME: name = "Track name"; break;
			case MIDI_INSTRUMENT_NAME: name = "Instrument name"; break;
			case MIDI_LYRIC: name = "Lyric"; break;
			case MIDI_TEXT_MARKER: name = "Text marker"; break;
			case MIDI_SEQUENCE_NUMBER: name = "Sequence number"; printable = false; break;
			case MIDI_CHANNEL_PREFIX_OR_PORT: name = "Channel prefix or port"; printable = false; break;
			case MIDI_CUE_POINT: name = "Cue point"; break;
			case MIDI_CHANNEL_PREFIX: name = "Channel prefix"; printable = false; break;
			case MIDI_SEQUENCER_SPECIFIC: name = "Sequencer specific"; printable = false; break;
			case MIDI_SMPTE_OFFSET: name = "SMPTE offset"; printable = false; break;

			case MIDI_SET_TEMPO:
			{
				unsigned char m0 = j->getMetaMessage()[0];
				unsigned char m1 = j->getMetaMessage()[1];
				unsigned char m2 = j->getMetaMessage()[2];
				tempo = (((m0 << 8) + m1) << 8) + m2;
				upt = microseconds(tempo/td);
				if (Debug) {
    				printf("%u: Tempo: %f\n", t, 60000000.0 / double(tempo));
    				printf("UPT: %lu\n", (unsigned long)upt.count());
    			}
			}
			break;

			case MIDI_TIME_SIGNATURE:
			{
				int numerator = j->getMetaMessage()[0];
				int denominator = 1 << (int)j->getMetaMessage()[1];

				if (Debug) printf("%u: Time signature: %d/%d\n", t, numerator, denominator);
			}

			case MIDI_KEY_SIGNATURE:
			{
				int accidentals = j->getMetaMessage()[0];
				int isMinor = j->getMetaMessage()[1];
				bool isSharp = accidentals < 0 ? false : true;
				accidentals = accidentals < 0 ? -accidentals : accidentals;
				if (Debug) {
					printf("%u: Key signature: %d %s %s\n", t, accidentals, (isSharp ?
								(accidentals > 1 ? "sharps" : "sharp") :
								(accidentals > 1 ? "flats" : "flat")),
								(isMinor ? ", minor" : ", major"));
				}
			}
			} // switch

			if (Debug && (name != "")) {
				if (printable) {
					printf("%u: File meta event: code %d, name %s: \"%s\"\n", t, code, name.c_str(), j->getMetaMessage().c_str());
				} else {
					printf("%u: File meta event: code %d, name %s: ", t, code, name.c_str());
					for (unsigned int k = 0; k < j->getMetaMessage().length(); ++k) {
						printf("%0x ", (int)j->getMetaMessage()[k]);
					}
				}
			}
			continue;
		}

		switch (j->getMessageType()) {

		case MIDI_NOTE_ON:
			if (Debug) printf("%u: Note(%d): channel %d, duration %lu, pitch %d, velocity %d\n", t, noteN, ch, j->getDuration(), j->getPitch(), j->getVelocity());
			if (j->getVelocity() == 0) {
				Mute();
			} else {
   				if ((noteN >= startNote) && (noteN <= endNote)) {
   					Play(j->getPitch(), j->getVelocity(), upt.count() * j->getDuration());
   				}
			}
			++noteN;
			break;

		case MIDI_NOTE_OFF:
			if (Debug) printf("%u: Note off: channel %d, duration %lu, pitch %d, velocity %d\n", t, ch, j->getDuration(), j->getPitch(), j->getVelocity());
			Mute();
			break;


		case MIDI_POLY_AFTERTOUCH:
			if (Debug) printf("%u: Polyphonic aftertouch: channel %d, pitch %d, pressure %d\n", t, ch, j->getPitch(), j->getData2());
			break;

		case MIDI_CTRL_CHANGE:
		{
			if (Debug) {
    			int controller = j->getData1();
    			string name;
    			switch (controller) {
    			case MIDI_CONTROLLER_BANK_MSB: name = "Bank select MSB"; break;
    			case MIDI_CONTROLLER_VOLUME: name = "Volume"; break;
    			case MIDI_CONTROLLER_BANK_LSB: name = "Bank select LSB"; break;
    			case MIDI_CONTROLLER_MODULATION: name = "Modulation wheel"; break;
    			case MIDI_CONTROLLER_PAN: name = "Pan"; break;
    			case MIDI_CONTROLLER_SUSTAIN: name = "Sustain"; break;
    			case MIDI_CONTROLLER_RESONANCE: name = "Resonance"; break;
    			case MIDI_CONTROLLER_RELEASE: name = "Release"; break;
    			case MIDI_CONTROLLER_ATTACK: name = "Attack"; break;
    			case MIDI_CONTROLLER_FILTER: name = "Filter"; break;
    			case MIDI_CONTROLLER_REVERB: name = "Reverb"; break;
    			case MIDI_CONTROLLER_CHORUS: name = "Chorus"; break;
    			case MIDI_CONTROLLER_NRPN_1: name = "NRPN 1"; break;
    			case MIDI_CONTROLLER_NRPN_2: name = "NRPN 2"; break;
    			case MIDI_CONTROLLER_RPN_1: name = "RPN 1"; break;
    			case MIDI_CONTROLLER_RPN_2: name = "RPN 2"; break;
    			case MIDI_CONTROLLER_SOUNDS_OFF: name = "All sounds off"; break;
    			case MIDI_CONTROLLER_RESET: name = "Reset"; break;
    			case MIDI_CONTROLLER_LOCAL: name = "Local"; break;
    			case MIDI_CONTROLLER_ALL_NOTES_OFF: name = "All notes off"; break;
    			}
    			printf("%u: Controller change: channel %d controller %d (%s) value %d\n", t, ch, j->getData1(), name.c_str(), j->getData2());
    		}
		}
		break;

		case MIDI_PROG_CHANGE:
			if (Debug) printf("%u: Program change: channel %d program %d\n", t, ch, j->getData1());
			break;

		case MIDI_CHNL_AFTERTOUCH:
			if (Debug) printf("%u: Channel aftertouch: channel %d pressure %d\n", t, ch, j->getData1());
			break;

		case MIDI_PITCH_BEND:
			if (Debug) printf("%u: Pitch bend: channel %d value %d\n", t, ch, (int)j->getData2() * 128 + (int)j->getData1());
			break;

		case MIDI_SYSTEM_EXCLUSIVE:
			if (Debug) printf("%u: System exclusive: code %d message length %d\n", t, (int)j->getMessageType(), (int)j->getMetaMessage().length());
			break;
		}
	}
	return 1;
}


int CleanupMIDIFile() {
	delete Fr;
	Fr = NULL;
	return 1;
}
