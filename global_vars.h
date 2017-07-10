/*
 * song_file.h
 *
 *  Created on: Apr 07, 2017
 *  Author: Sid Parida
 */


#define MAX_TIME 600

// Global Variables
long  msecs01 = 0;			// milliseconds
int secs = 0;				// seconds
int decodeFlag = 0;			// should decode ?
int playFlag = 0;			// should play ?
int songChoice = 1;			// song choice number
int songDecode = 1;			// should decode mid play ?
int recordNote = 1;			// shoudld record note ?
int lastGlobalTime = -1;	// global time 
int maxTime;				// max time of song
int currPlaybackTime = 0;	// current playback time
int tempo = 100;			// playback tempo
int timeFlag = 1;			// timer interrupt flaG
int currNoteCount = 0;		// current note value
int seq = 0;				// sequence number

// Struct to hold note information
typedef struct
{
	int time; 		   //ticks
	char type;         // n or o - On or Off
	char key[2];       // C, Db, etc
	int octave;        // 0 through 9
} Note;

// Note data
Note notes[MAX_TIME][6];
Note tempnotes[100];

// Fret Control Logic Output Signals
int fret0[6] 	 = {0, 0, 0, 0, 0, 0};
int fret1[6] 	 = {0, 0, 0, 0, 0, 0};
int fret2[6] 	 = {0, 0, 0, 0, 0, 0};
int fret3[6] 	 = {0, 0, 0, 0, 0, 0};
int fret4[6] 	 = {0, 0, 0, 0, 0, 0};
int fret5[6] 	 = {0, 0, 0, 0, 0, 0};
int fret6[6] 	 = {0, 0, 0, 0, 0, 0};
int fret7[6] 	 = {0, 0, 0, 0, 0, 0};
int fret8[6] 	 = {0, 0, 0, 0, 0, 0};
int fret9[6] 	 = {0, 0, 0, 0, 0, 0};
int fret10[6] 	 = {0, 0, 0, 0, 0, 0};

// Pluck Control Logic Output Signals
int picks[6] 	 = {0, 0, 0, 0, 0, 0};
int old_picks[6] = {0, 0, 0, 0, 0, 0};
