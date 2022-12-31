/*
 * MIDI player
 * Copyright (C) 2022 MaxWolf d5713fb35e03d9aa55881eaa23f86fb6f09982ed4da2a59410639a1c9d35bfbf
 * SPDX-License-Identifier: GPL-3.0-or-later
 * see https://www.gnu.org/licenses/ for license terms
 * based on sources from https://code.soundsoftware.ac.uk/projects/midifile/repository
 */
/*
 * MIDI handling functions
 */
int PrepareMIDIFile(const char *filename);
int PlayMIDIFile(unsigned int trackN, int startNode, int endNote);
int CleanupMIDIFile();
