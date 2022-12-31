/*
 * iMelody/eMelody parser and player
 * Copyright (C) 2022 MaxWolf d5713fb35e03d9aa55881eaa23f86fb6f09982ed4da2a59410639a1c9d35bfbf
 * based on https://raw.githubusercontent.com/id-Software/DOOM-IOS2/master/common/embeddedaudiosynthesis/arm-fm-22k/lib_src/eas_imelody.c (see below)
 * # SPDX-License-Identifier: Apache-2.0
*/

/*
 * iMelody/eMelody handling functions
 */
int PrepareMelodyFile(const char *filename);
int PrepareMelodyString(const char *str, char type);
int PlayMelody();
int CleanupMelody();
