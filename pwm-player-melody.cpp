/*
 * iMelody/eMelody parser and player
 * Copyright (C) 2022 MaxWolf d5713fb35e03d9aa55881eaa23f86fb6f09982ed4da2a59410639a1c9d35bfbf
 * based on https://raw.githubusercontent.com/id-Software/DOOM-IOS2/master/common/embeddedaudiosynthesis/arm-fm-22k/lib_src/eas_imelody.c (see below)
 * # SPDX-License-Identifier: Apache-2.0
*/
/*----------------------------------------------------------------------------
 *
 * File:
 * eas_imelody.c
 *
 * Contents and purpose:
 * iMelody parser
 *
 * Copyright Sonic Network Inc. 2005

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *----------------------------------------------------------------------------
 * Revision Control:
 *   #Revision: 797 #
 *   #Date: 2007-08-01 00:15:56 -0700 (Wed, 01 Aug 2007) #
 *----------------------------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "pwm-player.h"

__attribute__ ((used)) static char s_RCSVersion[] = "$Id: pwm-player-melody.cpp 285 2022-12-31 14:56:40Z maxwolf $";
__attribute__ ((used)) static char s_RCSsrc[] = "https://github.com/sthamster/pwm-player";


// #define _DEBUG_IMELODY

////////////////////////////////////////////////////////////////////////////
// 
// various excerpts and substitutions for external code
//
#define EAS_SUCCESS                         0
#define EAS_FAILURE                         -1
#define EAS_ERROR_INVALID_MODULE            -2
#define EAS_ERROR_MALLOC_FAILED             -3
#define EAS_ERROR_FILE_POS                  -4
#define EAS_ERROR_INVALID_FILE_MODE         -5
#define EAS_ERROR_FILE_SEEK                 -6
#define EAS_ERROR_FILE_LENGTH               -7
#define EAS_ERROR_NOT_IMPLEMENTED           -8
#define EAS_ERROR_CLOSE_FAILED              -9
#define EAS_ERROR_FILE_OPEN_FAILED          -10
#define EAS_ERROR_INVALID_HANDLE            -11
#define EAS_ERROR_NO_MIX_BUFFER             -12
#define EAS_ERROR_PARAMETER_RANGE           -13
#define EAS_ERROR_MAX_FILES_OPEN            -14
#define EAS_ERROR_UNRECOGNIZED_FORMAT       -15
#define EAS_BUFFER_SIZE_MISMATCH            -16
#define EAS_ERROR_FILE_FORMAT               -17
#define EAS_ERROR_SMF_NOT_INITIALIZED       -18
#define EAS_ERROR_LOCATE_BEYOND_END         -19
#define EAS_ERROR_INVALID_PCM_TYPE          -20
#define EAS_ERROR_MAX_PCM_STREAMS           -21
#define EAS_ERROR_NO_VOICE_ALLOCATED        -22
#define EAS_ERROR_INVALID_CHANNEL           -23
#define EAS_ERROR_ALREADY_STOPPED           -24
#define EAS_ERROR_FILE_READ_FAILED          -25
#define EAS_ERROR_HANDLE_INTEGRITY          -26
#define EAS_ERROR_MAX_STREAMS_OPEN          -27
#define EAS_ERROR_INVALID_PARAMETER         -28
#define EAS_ERROR_FEATURE_NOT_AVAILABLE     -29
#define EAS_ERROR_SOUND_LIBRARY             -30
#define EAS_ERROR_NOT_VALID_IN_THIS_STATE   -31
#define EAS_ERROR_NO_VIRTUAL_SYNTHESIZER    -32
#define EAS_ERROR_FILE_ALREADY_OPEN         -33
#define EAS_ERROR_FILE_ALREADY_CLOSED       -34
#define EAS_ERROR_INCOMPATIBLE_VERSION      -35
#define EAS_ERROR_QUEUE_IS_FULL             -36
#define EAS_ERROR_QUEUE_IS_EMPTY            -37
#define EAS_ERROR_FEATURE_ALREADY_ACTIVE    -38

#define EAS_FALSE   0
#define EAS_TRUE    1
#define EAS_EOF     3

typedef unsigned char EAS_U8;
typedef signed char EAS_I8;
typedef char EAS_CHAR;

typedef unsigned short EAS_U16;
typedef short EAS_I16;

typedef unsigned int EAS_U32;
typedef int EAS_I32;

typedef int EAS_INT;
typedef int EAS_RESULT;
typedef int EAS_BOOL;
typedef int EAS_STATE;
typedef int E_EAS_METADATA_TYPE;
typedef void* EAS_VOID_PTR;

//dummy values
#define EAS_FILE_IMELODY 1
#define EAS_METADATA_TITLE 1
#define EAS_METADATA_AUTHOR 1

typedef struct {
    char fname[PATH_MAX];
	char *buf;
	char *pos;
	int len;
} MEM_FILE_HANDLE;


int GetFile(const char *fileName, MEM_FILE_HANDLE *fh) {
  FILE *f = NULL;
  size_t fsize;

    strncpy(fh->fname, fileName, sizeof(fh->fname) - 1);
	if ((f = fopen(fileName, "rb")) == NULL) {
		return EAS_ERROR_FILE_OPEN_FAILED;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return EAS_ERROR_FILE_SEEK;
	}
	fsize = ftell(f);
	if (fsize > INT_MAX) {
		fclose(f);
		return EAS_ERROR_FILE_LENGTH;
	}
	fh->buf = (char*)malloc(fsize);
	if (fh->buf == NULL) {
		fclose(f);
		return EAS_ERROR_MALLOC_FAILED;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return EAS_ERROR_FILE_SEEK;
	}
	if (fread(fh->buf, 1, fsize, f) != fsize) {
		fclose(f);
		free(fh->buf);
		return EAS_ERROR_FILE_LENGTH;
	}
	fclose(f);
	fh->len = (int)fsize;
	fh->pos = fh->buf;
	return EAS_SUCCESS; 

}


int GetString(const char *str, MEM_FILE_HANDLE *fh) {
  int sl = strlen(str);
	fh->buf = (char*)malloc(sl + 1);
	if (fh->buf == NULL) {
		return EAS_ERROR_MALLOC_FAILED;
	}
	strcpy(fh->buf, str);
	fh->len = sl;
	fh->pos = fh->buf;
	strcpy(fh->fname, "string");
	return EAS_SUCCESS; 
}



int EAS_HWGetByte(MEM_FILE_HANDLE *fh, EAS_I8 *c) {
	if (fh->pos - fh->buf >= fh->len) {
		return EAS_EOF;
	}
	if (c) {
		*c = (EAS_I8)*(fh->pos);
	}
	++(fh->pos);
	return EAS_SUCCESS;
}

int EAS_HWFilePos(MEM_FILE_HANDLE *fh, int *pos) {
	if (pos) {
		*pos = (fh->pos - fh->buf);
	}
	return EAS_SUCCESS;
}

int EAS_HWFileSeek(MEM_FILE_HANDLE *fh, int offset) {
	if (offset >= fh->len) {
		return EAS_ERROR_FILE_SEEK;
	} else {
		fh->pos = fh->buf + offset;
		return EAS_SUCCESS;
	}
}

int EAS_HWCloseFile(MEM_FILE_HANDLE *fh) {
	free(fh->buf); fh->buf = NULL;
	fh->pos = 0;
	fh->len = 0;
	return EAS_SUCCESS;
}

typedef enum
{
    eParserModePlay,
    eParserModeLocate,
    eParserModeMute,
    eParserModeMetaData
} E_PARSE_MODE;

typedef enum
{
    EAS_STATE_READY = 0,
    EAS_STATE_PLAY,
    EAS_STATE_STOPPING,
    EAS_STATE_PAUSING,
    EAS_STATE_STOPPED,
    EAS_STATE_PAUSED,
    EAS_STATE_OPEN,
    EAS_STATE_ERROR,
    EAS_STATE_EMPTY
} E_EAS_STATE;

typedef enum
{
    PARSER_DATA_FILE_TYPE,
    PARSER_DATA_PLAYBACK_RATE,
    PARSER_DATA_TRANSPOSITION,
    PARSER_DATA_VOLUME,
    PARSER_DATA_SYNTH_HANDLE,
    PARSER_DATA_METADATA_CB,
    PARSER_DATA_DLS_COLLECTION,
    PARSER_DATA_EAS_LIBRARY,
    PARSER_DATA_POLYPHONY,
    PARSER_DATA_PRIORITY,
    PARSER_DATA_FORMAT,
    PARSER_DATA_MEDIA_LENGTH,
    PARSER_DATA_JET_CB,
    PARSER_DATA_MUTE_FLAGS,
    PARSER_DATA_SET_MUTE,
    PARSER_DATA_CLEAR_MUTE,
    PARSER_DATA_NOTE_COUNT,
    PARSER_DATA_MAX_PCM_STREAMS,
    PARSER_DATA_GAIN_OFFSET,
    PARSER_DATA_PLAY_MODE
} E_PARSER_DATA;

/* maximum line size as specified in iMelody V1.2 spec */
#define MAX_LINE_SIZE           75

/*----------------------------------------------------------------------------
 *
 * S_IMELODY_DATA
 *
 * This structure contains the state data for the iMelody parser
 *----------------------------------------------------------------------------
*/

typedef struct
{
    MEM_FILE_HANDLE* fileHandle;                /* file handle */
    char            subType;                    /* either 'I'melody or 'E'melody format */
	int             octaveShift;                /* EMelody octave shift for a next note (+1 or +2) */
	char            durationEMY;                /* EMelody duration ('1' or '3') */
    EAS_I32         fileOffset;                 /* offset to start of data */
    EAS_I32         time;                       /* current time in 256ths of a msec */
    EAS_I32         tickBase;                   /* basline length of 32nd note in 256th of a msec */
    EAS_I32         tick;                       /* actual length of 32nd note in 256th of a msec */
    EAS_I32         restTicks;                  /* ticks to rest after current note */
    int             startLine;                  /* file offset at start of line (for repeats) */
    int             repeatOffset;               /* file offset to start of repeat section */
    EAS_I16         repeatCount;                /* repeat counter */
    EAS_U8          state;                      /* current state EAS_STATE_XXXX */
    EAS_U8          style;                      /* from STYLE */
    EAS_U8          index;                      /* index into buffer */
    EAS_U8          octave;                     /* octave prefix */
    EAS_U8          volume;                     /* current volume */
    EAS_U8          note;                       /* MIDI note number */
    EAS_I8          noteModifier;               /* sharp or flat */
    EAS_I8          buffer[MAX_LINE_SIZE+1];    /* buffer for ASCII data */
} S_IMELODY_DATA;


////////////////////////////////////////////////////////////////////////////


/* increase gain for mono ringtones */
#define IMELODY_GAIN_OFFSET     8

/* length of 32nd note in 1/256ths of a msec for 120 BPM tempo */
#define DEFAULT_TICK_CONV       16000
#define TICK_CONVERT            1920000

/* default channel and program for iMelody playback */
#define IMELODY_CHANNEL         0
#define IMELODY_PROGRAM         80
/*#define IMELODY_VEL_MUL         4
#define IMELODY_VEL_OFS         67*/
#define IMELODY_VEL_MUL         8
#define IMELODY_VEL_OFS         0

/* multiplier for fixed point triplet conversion */
#define TRIPLET_MULTIPLIER      683
#define TRIPLET_SHIFT           10

static const char* const tokens[] =
{
    "BEGIN:IMELODY",
    "BEGIN:EMELODY",
    "VERSION:",
    "FORMAT:CLASS",
    "NAME:",
    "COMPOSER:",
    "BEAT:",
    "STYLE:",
    "VOLUME:",
    "MELODY:",
    "END:IMELODY",
    "END:EMELODY"
};

typedef enum
{
    TOKEN_BEGIN,
    TOKEN_BEGIN_E,
    TOKEN_VERSION,
    TOKEN_FORMAT,
    TOKEN_NAME,
    TOKEN_COMPOSER,
    TOKEN_BEAT,
    TOKEN_STYLE,
    TOKEN_VOLUME,
    TOKEN_MELODY,
    TOKEN_END,
    TOKEN_END_E,
    TOKEN_INVALID
} ENUM_IMELODY_TOKENS;

/* ledon or ledoff */
static const char ledStr[] = "edo";

/* vibeon or vibeoff */
static const char vibeStr[] = "ibeo";

/* backon or backoff */
static const char backStr[] = "cko";


/* lookup table for note values */
static const EAS_I8 noteTable[] = { 9, 11, 0, 2, 4, 5, 7 };

/* inline functions */
#ifdef _DEBUG_IMELODY
static void PutBackChar (S_IMELODY_DATA *pData)
{
    if (pData->index)
        pData->index--;
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "PutBackChar '%c'\n", pData->buffer[pData->index]); */ }
}
#else
void PutBackChar (S_IMELODY_DATA *pData) { if (pData->index) pData->index--; }
#endif


/* local prototypes */
static EAS_RESULT IMY_CheckFileType (MEM_FILE_HANDLE* fileHandle, S_IMELODY_DATA* pData);
static EAS_RESULT IMY_Event (EAS_VOID_PTR pInstData, EAS_INT parserMode);
static EAS_RESULT IMY_State (EAS_VOID_PTR pInstData, EAS_STATE *pState);
static EAS_RESULT IMY_Close (EAS_VOID_PTR pInstData);
static EAS_BOOL IMY_PlayNote (S_IMELODY_DATA *pData, EAS_I8 note, EAS_INT parserMode);
static EAS_BOOL IMY_PlayRest (S_IMELODY_DATA *pData);
static EAS_BOOL IMY_GetDuration (S_IMELODY_DATA *pData, EAS_I32 *pDuration);
static EAS_BOOL IMY_GetLEDState (S_IMELODY_DATA *pData);
static EAS_BOOL IMY_GetVibeState (S_IMELODY_DATA *pData);
static EAS_BOOL IMY_GetBackState (S_IMELODY_DATA *pData);
static EAS_BOOL IMY_GetVolume (S_IMELODY_DATA *pData, EAS_BOOL inHeader);
static EAS_BOOL IMY_GetNumber (S_IMELODY_DATA *pData, EAS_INT *temp, EAS_BOOL inHeader);
static EAS_RESULT IMY_ParseHeader (S_IMELODY_DATA* pData);
static EAS_I8 IMY_GetNextChar (S_IMELODY_DATA *pData, EAS_BOOL inHeader);
static EAS_RESULT IMY_ReadLine (MEM_FILE_HANDLE* fileHandle, EAS_I8 *buffer, int *pStartLine);
static EAS_INT IMY_ParseLine (EAS_I8 *buffer, EAS_U8 *pIndex);

/*----------------------------------------------------------------------------
 * IMY_CheckFileType()
 *----------------------------------------------------------------------------
 * Purpose:
 * Check the file type to see if we can parse it
 *
 * Inputs:
 * pEASData         - pointer to overall EAS data structure
 * handle           - pointer to file handle
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_RESULT IMY_CheckFileType (MEM_FILE_HANDLE *fileHandle, S_IMELODY_DATA* pData)
{
    EAS_I8 buffer[MAX_LINE_SIZE+1];
    EAS_U8 index;
	EAS_INT t;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter IMY_CheckFileType\n"); */ }
#endif

    /* read the first line of the file */
    if (IMY_ReadLine(fileHandle, buffer, NULL) != EAS_SUCCESS)
        return EAS_SUCCESS;

    /* check for header string */
	t = IMY_ParseLine(buffer, &index);
    if ((t == TOKEN_BEGIN) || (t == TOKEN_BEGIN_E)) {
        memset(pData, 0, sizeof(S_IMELODY_DATA));
        /* initialize */
        pData->fileHandle = fileHandle;
        pData->fileOffset = 0;
        pData->state = EAS_STATE_OPEN;
		if (t == TOKEN_BEGIN) {
			pData->subType = 'I';
		} else if (t == TOKEN_BEGIN_E) {
            pData->subType = 'E';
		}
    }

    return EAS_SUCCESS;
}

/*----------------------------------------------------------------------------
 * IMY_Event()
 *----------------------------------------------------------------------------
 * Purpose:
 * Parse the next event in the file
 *
 * Inputs:
 * pEASData         - pointer to overall EAS data structure
 * handle           - pointer to file handle
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_RESULT IMY_Event (EAS_VOID_PTR pInstData, EAS_INT parserMode)
{
    S_IMELODY_DATA* pData;
    EAS_RESULT result;
    EAS_I8 c;
    EAS_BOOL eof;
    EAS_INT temp;

    pData = (S_IMELODY_DATA*) pInstData;
    if (pData->state >= EAS_STATE_OPEN)
        return EAS_SUCCESS;

    /* initialize MIDI channel when the track starts playing */
    if (pData->time == 0)
    {

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_Event: Reset\n"); */ }
#endif
        /* set program to square lead */
        //VMProgramChange(pEASData->pVoiceMgr, pData->pSynth, IMELODY_CHANNEL, IMELODY_PROGRAM);

        /* set channel volume to max */
        //VMControlChange(pEASData->pVoiceMgr, pData->pSynth, IMELODY_CHANNEL, 7, 127);
    }

    /* check for end of note */
    if (pData->note)
    {

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Stopping note %d\n", pData->note); */ }
#endif
        /* stop the note */
        Mute();
        pData->note = 0;

        /* check for rest between notes */
        if (pData->restTicks)
        {
            pData->time += pData->restTicks;
            pData->restTicks = 0;
            return EAS_SUCCESS;
        }
    }

    /* parse the next event */
    eof = EAS_FALSE;
    while (!eof)
    {

        /* get next character */
        c = IMY_GetNextChar(pData, EAS_FALSE);
        if (Debug) {
        	printf("Next char is '%c'\n", c);
        }

        switch (c)
        {
            /* start repeat */
            case '+':
				if (pData->subType == 'E') {
					if (pData->octaveShift < 2) {
						++(pData->octaveShift);
					}
				}
				break;
            case '(':

#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter repeat section\n", c); */ }
#endif
				if (pData->subType == 'E') {
					IMY_GetNextChar(pData, EAS_FALSE); /* assume 'b' */
					IMY_GetNextChar(pData, EAS_FALSE); /* assume ')' */
					pData->noteModifier = 1;
				} else {
					if (pData->repeatOffset < 0)
					{
						pData->repeatOffset = pData->startLine + (EAS_I32) pData->index;

#ifdef _DEBUG_IMELODY
						{ /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Repeat offset = %d\n", pData->repeatOffset); */ }
#endif
					}
					else
						{ /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Ignoring nested repeat section\n"); */ }
				}
                break;

            /* end repeat */
            case ')':

#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "End repeat section, repeat offset = %d\n", pData->repeatOffset); */ }
#endif
                /* ignore invalid repeats */
                if (pData->repeatCount >= 0)
                {

                    /* decrement repeat count (repeatCount == 0 means infinite loop) */
                    if (pData->repeatCount > 0)
                    {
                        if (--pData->repeatCount == 0)
                        {
                            pData->repeatCount = -1;
#ifdef _DEBUG_IMELODY
                            { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Repeat loop complete\n"); */ }
#endif
                        }
                    }

//2 TEMPORARY FIX: If locating, don't do infinite loops.
//3 We need a different mode for metadata parsing where we don't loop at all
                    if ((parserMode == eParserModePlay) || (pData->repeatCount != 0))
                    {

#ifdef _DEBUG_IMELODY
                        { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Rewinding file for repeat\n"); */ }
#endif
                        /* rewind to start of loop */
                        if ((result = EAS_HWFileSeek(pData->fileHandle, pData->repeatOffset)) != EAS_SUCCESS)
                            return result;
                        IMY_ReadLine(pData->fileHandle, pData->buffer, &pData->startLine);
                        pData->index = 0;

                        /* if last loop, prevent future loops */
                        if (pData->repeatCount == -1)
                            pData->repeatOffset = -1;
                    }
                }
                break;

            /* repeat count */
            case '@':
                if (!IMY_GetNumber(pData, &temp, EAS_FALSE))
                    eof = EAS_TRUE;
                else if (pData->repeatOffset > 0)
                {

#ifdef _DEBUG_IMELODY
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Repeat count = %dt", pData->repeatCount); */ }
#endif
                    if (pData->repeatCount < 0)
                        pData->repeatCount = (EAS_I16) temp;
                }
                break;

            /* volume */
            case 'V':
                if (!IMY_GetVolume(pData, EAS_FALSE))
                    eof = EAS_TRUE;
                break;

            /* flat */
            case '&':
                pData->noteModifier = -1;
                break;

            /* sharp */
            case '#':
                pData->noteModifier = +1;
                break;

            /* octave */
            case '*':
                c = IMY_GetNextChar(pData, EAS_FALSE);
                if (isdigit(c))
                    pData->octave = (EAS_U8) ((c - '0' + 1) * 12);
                else if (!c)
                    eof = EAS_TRUE;
                break;

            /* ledon or ledoff */
            case 'l':
                if (!IMY_GetLEDState(pData))
                    eof = EAS_TRUE;
                break;

            /* vibeon or vibeoff */
            case 'v':
                if (!IMY_GetVibeState(pData))
                    eof = EAS_TRUE;
                break;

            /* either a B note or backon or backoff */
            case 'b':
                if (IMY_GetNextChar(pData, EAS_FALSE) == 'a')
                {
                    if (!IMY_GetBackState(pData))
                        eof = EAS_TRUE;
                }
                else
                {
                    PutBackChar(pData);
                    if (IMY_PlayNote(pData, c, parserMode))
                        return EAS_SUCCESS;
                    eof = EAS_TRUE;
                }
                break;

            /* rest */
            case 'r':
            case 'R':
                if (IMY_PlayRest(pData))
                    return EAS_SUCCESS;
                eof = EAS_TRUE;
                break;

            /* EMelody pause (rest) */
            case 'p':
				pData->durationEMY = '3';
                if (IMY_PlayRest(pData))
                    return EAS_SUCCESS;
                eof = EAS_TRUE;
				break;
            case 'P':
				pData->durationEMY = '1';
                if (IMY_PlayRest(pData))
                    return EAS_SUCCESS;
                eof = EAS_TRUE;
				break;

            /* EOF */
            case 0:
#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_Event: end of iMelody file detected\n"); */ }
#endif
                eof = EAS_TRUE;
            break;

            /* must be a note */
            default:
				if (pData->subType == 'E') {
					if ((c >= 'A') && (c <= 'G')) {
						pData->durationEMY = '1';
					} else {
						pData->durationEMY = '3';
					}
				}
				c = tolower(c);
                if ((c >= 'a') && (c <= 'g'))
                {
                    if (IMY_PlayNote(pData, c, parserMode))
                        return EAS_SUCCESS;
                    eof = EAS_TRUE;
                }
                else
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Ignoring unexpected character '%c' [0x%02x]\n", c, c); */ }
                break;
        }
    }

    /* handle EOF */
#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_Event: state set to EAS_STATE_STOPPING\n"); */ }
#endif
    pData->state = EAS_STATE_STOPPING;
    //VMReleaseAllVoices(pEASData->pVoiceMgr, pData->pSynth);
    return EAS_SUCCESS;
}

/*----------------------------------------------------------------------------
 * IMY_State()
 *----------------------------------------------------------------------------
 * Purpose:
 * Returns the current state of the stream
 *
 * Inputs:
 * pEASData         - pointer to overall EAS data structure
 * handle           - pointer to file handle
 * pState           - pointer to variable to store state
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_RESULT IMY_State (EAS_VOID_PTR pInstData, EAS_I32 *pState)
{
    S_IMELODY_DATA* pData;

    /* establish pointer to instance data */
    pData = (S_IMELODY_DATA*) pInstData;

    /* if stopping, check to see if synth voices are active */
    if (pData->state == EAS_STATE_STOPPING)
    {
        if (1) //(VMActiveVoices(pData->pSynth) == 0)
        {
            pData->state = EAS_STATE_STOPPED;
#ifdef _DEBUG_IMELODY
            { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_State: state set to EAS_STATE_STOPPED\n"); */ }
#endif
        }
    }

    if (pData->state == EAS_STATE_PAUSING)
    {
        if (1) //(VMActiveVoices(pData->pSynth) == 0)
        {
#ifdef _DEBUG_IMELODY
            { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_State: state set to EAS_STATE_PAUSED\n"); */ }
#endif
            pData->state = EAS_STATE_PAUSED;
        }
    }

    /* return current state */
    *pState = pData->state;
    return EAS_SUCCESS;
}

/*----------------------------------------------------------------------------
 * IMY_Close()
 *----------------------------------------------------------------------------
 * Purpose:
 * Close the file and clean up
 *
 * Inputs:
 * pEASData         - pointer to overall EAS data structure
 * handle           - pointer to file handle
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_RESULT IMY_Close (EAS_VOID_PTR pInstData)
{
    S_IMELODY_DATA* pData;
    EAS_RESULT result;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_Close: close file\n"); */ }
#endif

    pData = (S_IMELODY_DATA*) pInstData;

    /* close the file */
    if ((result = EAS_HWCloseFile(pData->fileHandle)) != EAS_SUCCESS)
            return result;

    return EAS_SUCCESS;
}


/*----------------------------------------------------------------------------
 * IMY_PlayNote()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_BOOL IMY_PlayNote (S_IMELODY_DATA *pData, EAS_I8 note, EAS_INT parserMode)
{
    EAS_I32 duration;
    EAS_U8 velocity;

//!!! подумать, как сделать игралку из параметров (-i строка -I файл ?)

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_PlayNote: start note %d\n", note); */ }
#endif

    /* get the duration */
    if (!IMY_GetDuration(pData, &duration))
        return EAS_FALSE;

    /* save note value */
	if (pData->subType == 'E') {
		pData->note = (EAS_U8) (48 + pData->octaveShift * 12 + noteTable[note - 'a'] + pData->noteModifier);
	} else {
		pData->note = (EAS_U8) (pData->octave + noteTable[note - 'a'] + pData->noteModifier);
	}
    velocity = (EAS_U8) (pData->volume ? pData->volume * IMELODY_VEL_MUL + IMELODY_VEL_OFS : 0);
    if (Debug) {
    	printf("Playing note %d(%c), velocity %d, duration %d ticks\n", pData->note, note, velocity, duration);
    }

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_PlayNote: Start note %d, duration %d\n", pData->note, duration); */ }
#endif

    /* determine note length */
    switch (pData->style)
    {
        case 0:
            pData->restTicks = duration >> 4;
            break;
        case 1:
            pData->restTicks = 0;
            break;
        case 2:
            pData->restTicks = duration >> 1;
            break;
        default:
            { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "IMY_PlayNote: Note style out of range: %d\n", pData->style); */ }
            pData->restTicks = duration >> 4;
            break;
    }

    /* next event is at end of this note */
    pData->time += duration - pData->restTicks;

    /* reset the flat/sharp modifier */
    pData->noteModifier = 0;
	/* reset EMelody default octave shift */
	pData->octaveShift = 0;

    /* start note only if in play mode */
    if (parserMode == eParserModePlay)
        Play(pData->note, velocity, ((duration - pData->restTicks) * 1000) / 256);
    Mute();
	if (pData->restTicks != 0) {
		this_thread::sleep_for(microseconds((pData->restTicks * 1000)/ 256));
	}
    return EAS_TRUE;
}

/*----------------------------------------------------------------------------
 * IMY_PlayRest()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_BOOL IMY_PlayRest (S_IMELODY_DATA *pData)
{
    EAS_I32 duration;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter IMY_PlayRest]n"); */ }
#endif

    /* get the duration */
    if (!IMY_GetDuration(pData, &duration))
        return EAS_FALSE;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_PlayRest: note duration %d\n", duration); */ }
#endif

    /* next event is at end of this note */
    pData->time += duration;
    if (Debug) {
    	printf("Pause for %d ticks\n", duration);
    }
    this_thread::sleep_for(microseconds((duration * 1000)/ 256));
    return EAS_TRUE;
}

/*----------------------------------------------------------------------------
 * IMY_GetDuration()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/

static EAS_BOOL IMY_GetDuration (S_IMELODY_DATA *pData, EAS_I32 *pDuration)
{
    EAS_I32 duration;
    EAS_I8 c;

    /* get the duration */
    *pDuration = 0;

	/* EMelody case */
	if (pData->subType == 'E') {
		duration = pData->tick * (1 << ('5' - pData->durationEMY));
		c = IMY_GetNextChar(pData, EAS_FALSE);
		if (!c) {
			*pDuration = duration;
			return EAS_TRUE;
		}
		if (c == '.') {
			duration = duration * 2;
		} else {
            PutBackChar(pData);
		}
		*pDuration = duration;
		return EAS_TRUE;
	}		

	/* IMelody case */
	c = IMY_GetNextChar(pData, EAS_FALSE);
    if (!c)
        return EAS_FALSE;
    if ((c < '0') || (c > '5'))
    {
#ifdef _DEBUG_IMELODY
        { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetDuration: error in duration '%c'\n", c); */ }
#endif
        return EAS_FALSE;
    }

    /* calculate total length of note */
    duration = pData->tick * (1 << ('5' - c));

    /* check for duration modifier */
    c = IMY_GetNextChar(pData, EAS_FALSE);
    if (c)
    {
        if (c == '.')
            duration += duration >> 1;
        else if (c == ':')
            duration += (duration >> 1) + (duration >> 2);
        else if (c == ';')
            duration = (duration * TRIPLET_MULTIPLIER) >> TRIPLET_SHIFT;
        else
            PutBackChar(pData);
    }

    *pDuration = duration;
    return EAS_TRUE;
}

/*----------------------------------------------------------------------------
 * IMY_GetLEDState()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_BOOL IMY_GetLEDState (S_IMELODY_DATA *pData)
{
    EAS_I8 c;
    EAS_INT i;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter IMY_GetLEDState\n"); */ }
#endif

    for (i = 0; i < 5; i++)
    {
        c = IMY_GetNextChar(pData, EAS_FALSE);
        if (!c)
            return EAS_FALSE;
        switch (i)
        {
            case 3:
                if (c == 'n')
                {
#ifdef _DEBUG_IMELODY
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetLEDState: LED on\n"); */ }
#endif
//                    EAS_HWLED(EAS_TRUE);
                    return EAS_TRUE;
                }
                else if (c != 'f')
                    return EAS_FALSE;
                break;

            case 4:
                if (c == 'f')
                {
#ifdef _DEBUG_IMELODY
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetLEDState: LED off\n"); */ }
#endif
//                    EAS_HWLED(EAS_FALSE);
                    return EAS_TRUE;
                }
                return EAS_FALSE;

            default:
                if (c != ledStr[i])
                    return EAS_FALSE;
                break;
        }
    }
    return EAS_FALSE;
}

/*----------------------------------------------------------------------------
 * IMY_GetVibeState()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_BOOL IMY_GetVibeState (S_IMELODY_DATA *pData)
{
    EAS_I8 c;
    EAS_INT i;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter IMY_GetVibeState\n"); */ }
#endif

    for (i = 0; i < 6; i++)
    {
        c = IMY_GetNextChar(pData, EAS_FALSE);
        if (!c)
            return EAS_FALSE;
        switch (i)
        {
            case 4:
                if (c == 'n')
                {
#ifdef _DEBUG_IMELODY
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetVibeState: vibrate on\n"); */ }
#endif
//                    EAS_HWVibrate(EAS_TRUE);
                    return EAS_TRUE;
                }
                else if (c != 'f')
                    return EAS_FALSE;
                break;

            case 5:
                if (c == 'f')
                {
#ifdef _DEBUG_IMELODY
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetVibeState: vibrate off\n"); */ }
#endif
//                    EAS_HWVibrate(EAS_FALSE);
                    return EAS_TRUE;
                }
                return EAS_FALSE;

            default:
                if (c != vibeStr[i])
                    return EAS_FALSE;
                break;
        }
    }
    return EAS_FALSE;
}

/*----------------------------------------------------------------------------
 * IMY_GetBackState()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_BOOL IMY_GetBackState (S_IMELODY_DATA *pData)
{
    EAS_I8 c;
    EAS_INT i;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter IMY_GetBackState\n"); */ }
#endif

    for (i = 0; i < 5; i++)
    {
        c = IMY_GetNextChar(pData, EAS_FALSE);
        if (!c)
            return EAS_FALSE;
        switch (i)
        {
            case 3:
                if (c == 'n')
                {
#ifdef _DEBUG_IMELODY
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetBackState: backlight on\n"); */ }
#endif
//                    EAS_HWBackLight(EAS_TRUE);
                    return EAS_TRUE;
                }
                else if (c != 'f')
                    return EAS_FALSE;
                break;

            case 4:
                if (c == 'f')
                {
#ifdef _DEBUG_IMELODY
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetBackState: backlight off\n"); */ }
#endif
//                    EAS_HWBackLight(EAS_FALSE);
                    return EAS_TRUE;
                }
                return EAS_FALSE;

            default:
                if (c != backStr[i])
                    return EAS_FALSE;
                break;
        }
    }
    return EAS_FALSE;
}

/*----------------------------------------------------------------------------
 * IMY_GetVolume()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_BOOL IMY_GetVolume (S_IMELODY_DATA *pData, EAS_BOOL inHeader)
{
    EAS_INT temp;
    EAS_I8 c;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter IMY_GetVolume\n"); */ }
#endif

    c = IMY_GetNextChar(pData, inHeader);
    if (c == '+')
    {
        if (pData->volume < 15)
            pData->volume++;
        return EAS_TRUE;
    }
    else if (c == '-')
    {
        if (pData->volume > 0)
            pData->volume--;
        return EAS_TRUE;
    }
    else if (isdigit(c))
        temp = c - '0';
    else
        return EAS_FALSE;

    c = IMY_GetNextChar(pData, inHeader);
    if (isdigit(c))
        temp = temp * 10 + c - '0';
    else if (c)
        PutBackChar(pData);
    if ((temp >= 0) && (temp <= 15))
    {
        if (inHeader && (temp == 0))
            { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Ignoring V0 encountered in header\n"); */ }
        else
            pData->volume = (EAS_U8) temp;
    }
    return EAS_TRUE;
}

/*----------------------------------------------------------------------------
 * IMY_GetNumber()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_BOOL IMY_GetNumber (S_IMELODY_DATA *pData, EAS_INT *temp, EAS_BOOL inHeader)
{
    EAS_BOOL ok;
    EAS_I8 c;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter IMY_GetNumber\n"); */ }
#endif

    *temp = 0;
    ok = EAS_FALSE;
    for (;;)
    {
        c = IMY_GetNextChar(pData, inHeader);
        if (isdigit(c))
        {
            *temp = *temp * 10 + c - '0';
            ok = EAS_TRUE;
        }
        else
        {
            if (c)
                PutBackChar(pData);

#ifdef _DEBUG_IMELODY
            { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetNumber: value %d\n", *temp); */ }
#endif

            return ok;
        }
    }
}

/*----------------------------------------------------------------------------
 * IMY_GetVersion()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_BOOL IMY_GetVersion (S_IMELODY_DATA *pData, EAS_INT *pVersion)
{
    EAS_I8 c;
    EAS_INT temp;
    EAS_INT version;

    version = temp = 0;
    for (;;)
    {
        c = pData->buffer[pData->index++];
        if ((c == 0) || (c == '.'))
        {
            version = (version << 8) + temp;
            if (c == 0)
            {

#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetVersion: version 0x%04x\n", version); */ }
#endif

                *pVersion = version;
                return EAS_TRUE;
            }
            temp = 0;
        }
        else if (isdigit(c))
            temp = (temp * 10) + c - '0';
    }
}

/*----------------------------------------------------------------------------
 * IMY_MetaData()
 *----------------------------------------------------------------------------
 * Purpose:
 * Prepare to parse the file. Allocates instance data (or uses static allocation for
 * static memory model).
 *
 * Inputs:
 * pEASData         - pointer to overall EAS data structure
 * handle           - pointer to file handle
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static void IMY_MetaData (S_IMELODY_DATA *pData, E_EAS_METADATA_TYPE metaType, EAS_I8 *buffer)
{
return;
}


static void InitData(S_IMELODY_DATA* pData) {

    /* initialize some defaults */
	if ((pData->subType != 'E') && (pData->subType != 'I')) {
		pData->subType = 'I'; // IMelody is default 
	}
    pData->time = 0;
    pData->tick = DEFAULT_TICK_CONV;
    pData->note = 0;
    pData->noteModifier = 0;
    pData->restTicks = 0;
    pData->volume = 7;
    pData->octave = 60;
    pData->repeatOffset = -1;
    pData->repeatCount = -1;
    pData->style = 1;

}

/*----------------------------------------------------------------------------
 * IMY_ParseHeader()
 *----------------------------------------------------------------------------
 * Purpose:
 * Prepare to parse the file. Allocates instance data (or uses static allocation for
 * static memory model).
 *
 * Inputs:
 * pEASData         - pointer to overall EAS data structure
 * handle           - pointer to file handle
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_RESULT IMY_ParseHeader (S_IMELODY_DATA* pData)
{
    EAS_RESULT result;
    EAS_INT token;
    EAS_INT temp;
    EAS_I8 c;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Enter IMY_ParseHeader\n"); */ }
#endif

	InitData(pData);
    /* force the read of the first line */
    pData->index = 1;

    /* read data until we get to melody */
    for (;;)
    {
        /* read a line from the file and parse the token */
        if (pData->index != 0)
        {
            if ((result = IMY_ReadLine(pData->fileHandle, pData->buffer, &pData->startLine)) != EAS_SUCCESS)
            {
#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_ParseHeader: IMY_ReadLine returned %d\n", result); */ }
#endif
                return result;
            }
        }
        token = IMY_ParseLine(pData->buffer, &pData->index);

        switch (token)
        {
            /* ignore these valid tokens */
            case TOKEN_BEGIN:
            case TOKEN_BEGIN_E:
                break;

            case TOKEN_FORMAT:
                if (!IMY_GetVersion(pData, &temp))
                {
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Invalid FORMAT field '%s'\n", pData->buffer); */ }
                    return EAS_ERROR_FILE_FORMAT;
                }
                if (Debug) {
                	printf("Token FORMAT: %04x\n", temp);
                }
                if ((temp != 0x0100) && (temp != 0x0200)) // both 'VERSION:' and 'FORMAT:' get here
                {
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Unsupported FORMAT %02x\n", temp); */ }
                    return EAS_ERROR_UNRECOGNIZED_FORMAT;
                }
                break;

            case TOKEN_VERSION:
                if (!IMY_GetVersion(pData, &temp))
                {
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Invalid VERSION field '%s'\n", pData->buffer); */ }
                    return EAS_ERROR_FILE_FORMAT;
                }
                if (Debug) {
                	printf("Token VERSION: %04x\n", temp);
                }
                if ((temp != 0x0100) && (temp != 0x0102))
                {
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Unsupported VERSION %02x\n", temp); */ }
                    return EAS_ERROR_UNRECOGNIZED_FORMAT;
                }
                break;

            case TOKEN_NAME:
                if (Debug) {
                	printf("Token NAME:\n");
                }
                IMY_MetaData(pData, EAS_METADATA_TITLE, pData->buffer + pData->index);
                break;

            case TOKEN_COMPOSER:
                if (Debug) {
                	printf("Token COMPOSER:\n");
                }
                IMY_MetaData(pData, EAS_METADATA_AUTHOR, pData->buffer + pData->index);
                break;

            /* handle beat */
            case TOKEN_BEAT:
                IMY_GetNumber(pData, &temp, EAS_TRUE);
                if (Debug) {
                	printf("Token BEAT: %d\n", temp);
                }
                if ((temp >= 25) && (temp <= 900))
                    pData->tick = TICK_CONVERT / temp;
                break;

            /* handle style */
            case TOKEN_STYLE:
                c = IMY_GetNextChar(pData, EAS_TRUE);
                if (c == 'S')
                    c = IMY_GetNextChar(pData, EAS_TRUE);
                if (Debug) {
                	printf("Token STYLE: %c\n", c);
                }
                if ((c >= '0') && (c <= '2'))
                    pData->style = (EAS_U8) (c - '0');
                else
                {
                    PutBackChar(pData);
                    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Error in style command: %s\n", pData->buffer); */ }
                }
                break;

            /* handle volume */
            case TOKEN_VOLUME:
                c = IMY_GetNextChar(pData, EAS_TRUE);
                if (c != 'V')
                {
                    PutBackChar(pData);
                    if (!isdigit(c))
                    {
                        { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Error in volume command: %s\n", pData->buffer); */ }
                        break;
                    }
                }
                IMY_GetVolume(pData, EAS_TRUE);
                if (Debug) {
                	printf("Token VOLUME: %d\n", pData->volume);
                }
                break;

            case TOKEN_MELODY:
                if (Debug) {
                	printf("Token MELODY:\n");
                }
#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "Header successfully parsed\n"); */ }
#endif
                return EAS_SUCCESS;

            case TOKEN_END:
            case TOKEN_END_E:
                if (Debug) {
                	printf("Token END:\n");
                }
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Unexpected END:IMELODY encountered\n"); */ }
                return EAS_ERROR_FILE_FORMAT;

            default:
                if (Debug) {
                	printf("Token unknown\n");
                }
                /* force a read of the next line */
                pData->index = 1;
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_WARNING, "Ignoring unrecognized token in iMelody file: %s\n", pData->buffer); */ }
                break;
        }
    }
}

/*----------------------------------------------------------------------------
 * IMY_GetNextChar()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_I8 IMY_GetNextChar (S_IMELODY_DATA *pData, EAS_BOOL inHeader)
{
    EAS_I8 c;
    EAS_U8 index;
	EAS_INT t;

    for (;;)
    {
        /* get next character */
        c = pData->buffer[pData->index++];

        /* buffer empty, read more */
        if (!c)
        {
            /* don't read the next line in the header */
            if (inHeader)
                return 0;

            pData->index = 0;
            pData->buffer[0] = 0;
            if (IMY_ReadLine(pData->fileHandle, pData->buffer, &pData->startLine) != EAS_SUCCESS)
            {
#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetNextChar: EOF\n"); */ }
#endif
                return 0;
            }

            /* check for END:IMELODY token */
			t = IMY_ParseLine(pData->buffer, &index);
            if ((t == TOKEN_END) || (t == TOKEN_END_E))
            {
#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetNextChar: found END:IMELODY\n"); */ }
#endif
                pData->buffer[0] = 0;
                return 0;
            }
            continue;
        }

        /* ignore white space */
        if (!isspace(c))
        {

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_GetNextChar returned '%c'\n", c); */ }
#endif
            return c;
        }
    }
}

/*----------------------------------------------------------------------------
 * IMY_ReadLine()
 *----------------------------------------------------------------------------
 * Purpose:
 * Reads a line of input from the file, discarding the CR/LF
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_RESULT IMY_ReadLine (MEM_FILE_HANDLE* fileHandle, EAS_I8 *buffer, int *pStartLine)
{
    EAS_RESULT result;
    EAS_INT i;
    EAS_I8 c;

    /* fetch current file position and save it */
    if (pStartLine != NULL)
    {
        if ((result = EAS_HWFilePos(fileHandle, pStartLine)) != EAS_SUCCESS)
        {
#ifdef _DEBUG_IMELODY
            { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_ParseHeader: EAS_HWFilePos returned %d\n", result); */ }
#endif
            return result;
        }
    }

    buffer[0] = 0;
    for (i = 0; i < MAX_LINE_SIZE; )
    {
        if ((result = EAS_HWGetByte(fileHandle, &c)) != EAS_SUCCESS)
        {
            if ((result == EAS_EOF) && (i > 0))
                break;
            return result;
        }

        /* return on LF or end of data */
        if (c == '\n')
            break;

        /* store characters in buffer */
        if (c != '\r')
            buffer[i++] = c;
    }
    buffer[i] = 0;

#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_ReadLine read %s\n", buffer); */ }
#endif

    return EAS_SUCCESS;
}

/*----------------------------------------------------------------------------
 * IMY_ParseLine()
 *----------------------------------------------------------------------------
 * Purpose:
 *
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 *
 * Side Effects:
 *
 *----------------------------------------------------------------------------
*/
static EAS_INT IMY_ParseLine (EAS_I8 *buffer, EAS_U8 *pIndex)
{
    EAS_INT i;
    EAS_INT j;

    /* there's no strnicmp() in stdlib, so we have to roll our own */
    for (i = 0; i < TOKEN_INVALID; i++)
    {
        for (j = 0; ; j++)
        {
            /* end of token, must be a match */
            if (tokens[i][j] == 0)
            {
#ifdef _DEBUG_IMELODY
                { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_ParseLine found token %d\n", i); */ }
#endif
                *pIndex = (EAS_U8) j;
                return i;
            }
            if (tokens[i][j] != toupper(buffer[j]))
                break;
        }
    }
#ifdef _DEBUG_IMELODY
    { /* dpp: EAS_ReportEx(_EAS_SEVERITY_DETAIL, "IMY_ParseLine: no token found\n"); */ }
#endif
    return TOKEN_INVALID;
}


static MEM_FILE_HANDLE src;
static S_IMELODY_DATA data;


int PrepareMelodyFile(const char *filename) {
  int result;
	memset(&src, 0, sizeof(src));
    if (Debug) {
    	printf("Preparing to play <I/E>Melody file [%s]...\n", filename);
    }
	if ((result = GetFile(filename, &src)) != EAS_SUCCESS) {
		printf("Error reading file %s: %d\n", filename, result);
		return -1;
	}
	if (((result = IMY_CheckFileType(&src, &data)) != EAS_SUCCESS) || 
		(data.state != EAS_STATE_OPEN)) {
		printf("Error checking file %s type: %d\n", filename, result);
		return -1;
	}
    /* parse the header */
    if ((result = IMY_ParseHeader(&data)) != EAS_SUCCESS) {
		printf("Error parsing file %s header: %d\n", filename, result);
		return -1;
	}
	return 1;
}

int PrepareMelodyString(const char *str, char type) {
  int result;
	memset(&src, 0, sizeof(src));
	if ((result = GetString(str, &src)) != EAS_SUCCESS) {
		printf("Error getting string %s: %d\n", str, result);
		return -1;
	}
	S_IMELODY_DATA *pData = &data;
    pData->fileHandle = &src;
    pData->fileOffset = 0;
    pData->state = EAS_STATE_OPEN;
	InitData(pData);
    pData->index = 0;
    pData->state = EAS_STATE_READY;
    pData->subType = (type == 'E' ? 'E' : 'I');
    if (Debug) {
    	printf("Preparing to play %cMelody string [%s]...\n", pData->subType, str);
    }
    if ((result = IMY_ReadLine(pData->fileHandle, pData->buffer, &pData->startLine)) != EAS_SUCCESS) {
        return -1;
    }
	return 1;
}

int PlayMelody() {
  int result;
  int playerState;
  S_IMELODY_DATA *pData = &data;

    if (Debug) {
    	printf("Playing %cMelody\n", pData->subType);
    }
    pData->state = EAS_STATE_READY;
	do {
		if ((result = IMY_Event(pData, eParserModePlay)) != EAS_SUCCESS) {
			printf("Error parsing %s: %d\n", pData->fileHandle->fname, result);
			return -1;
		}
		IMY_State(pData, &playerState);
		if (playerState == EAS_STATE_ERROR) {
			printf("Error playing %s\n", pData->fileHandle->fname);
			return -1;
		}
	} while (playerState != EAS_STATE_STOPPED);
	return 1;
}


int CleanupMelody() {
    if (Debug) {
    	printf("Melody cleanup\n");
    }
	IMY_Close(&data);
	return 1;
}
