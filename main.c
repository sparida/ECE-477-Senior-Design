/*
 *  Created on: Mar 10, 2017
 *  Author: Sid Parida
 */


#include <stm32f4xx.h>
#include <stm32f4xx_usart.h>
#include "song_file.h"
#include "global_vars.h"

/*---------------------------------------------------------------------------------------------------*/
// MIDI Decode Functions
/*---------------------------------------------------------------------------------------------------*/

// Decodes MIDI File and populates notes array
void decodeSong()
{
	int i;
	int fileLength = 480;
	int globalTicks = 0;
	int currentTicks = 0;
	char noteNames[12][3] = {"C ", "Db", "D ", "Eb", "E ", "F ", "Gb", "G ", "Ab", "A ", "Bb", "B "};


	// Copy song to song
	int j;
	USART_puts(USART1, "\nDecoding Song Number: ");
	initNotesArray();
	USART_puti(songChoice);
	USART_puts(USART1, "\n");
	unsigned char song[1024];
	for(j = 0; j < 1024;j++)
	{
	
		if(songChoice == 1) song[j] = song1[j];
		else if(songChoice == 2) song[j] = song2[j];
		else if(songChoice == 3) song[j] = song3[j];
		else if(songChoice == 4) song[j] = song4[j];
	}
	
	// Read header

	// Current Byte
	int cb = 0;

	// MThd Offset
	cb += 4;

	// Header size offset
	cb += 4;

	//  Midi Type
	int midiType = 0;
	midiType += 256 * song[cb] + song[cb+1];
	cb += 2;
	//printf("Midi Type: %d\n", midiType);

	// Number of tracks
	int numTracks = 0;
	numTracks += 256 * song[cb] + song[cb+1];
	cb += 2;
	//printf("Number of tracks: %d\n", numTracks);

	// Ticks per quarter note
	int divisions = 0;
	divisions += 256 * song[cb] + song[cb+1];
	cb += 2;
	//printf("Ticks per quarter note: %d\n", divisions);
	
	// Start processing each track
	
	// For each track
	int currTrack = 0;
	
	for (currTrack = 0; currTrack < numTracks; currTrack++)
	{

		// Inituialize track offset to 0
		int to = cb;

		// MTrk Offset
		to += 4;

		// Tracksize
		unsigned int trackSize = 0;
		unsigned int b3 = (int)song[to]   << 24 ;
		unsigned int b2 = (int)song[to+1] << 16 ;
		unsigned int b1 = (int)song[to+2] << 8 ;
		unsigned int b0 = (int)song[to+3] ;

		trackSize = b3 + b2 + b1 + b0;
		to += 4;
		
		// Calculate track end
		int trackEnd = to + trackSize;
		
		// Print Track Information
		//printf("\nTrack %d: \n\n", currTrack);
		//printf("Track Start: %d\n", cb);
		//printf("Track End  : %d\n", trackEnd);
		//printf("Data Size : %d\n", trackSize);


		// While end of track not reached
		while(to < trackEnd)
		{
			// Read variable length value
			unsigned int dt = 0;
			int stopReading = 0;
			unsigned char byteRead;

			while(!stopReading)
			{
				byteRead = song[to];
				// Read dt
				dt = dt << 7;
				dt += (byteRead & 127);
	
				unsigned char testbit = (byteRead >> 7) & 0x01;
	
				if(testbit == 0) stopReading = 1;
				to++;
			}

			currentTicks = dt;
			byteRead = song[to];
			// Event type already read
				to++;


			if( (byteRead == 0xF0) || (byteRead == 0xF7) || (byteRead == 0xFF))
			{


				// If meta event read type byte
				if(byteRead == 0xFF) to++;


				// Read length of data (variable length)
				dt = 0;
				stopReading = 0;
				while(!stopReading)
				{
					byteRead = song[to];
					// Read  Variable Length
					dt = dt << 7;
					dt += (byteRead & 127);
					
					unsigned char testbit = (byteRead >> 7) & 0x01;
					if(testbit == 0) stopReading = 1;
					to++;
				}

				// Skip meta data
				to += dt;
			}
			
			// Note event type
			else
			{
				
				unsigned char msbStatus = byteRead >> 4;
				unsigned char noteData  = song[to];
				
				if(msbStatus == 0x08)
				{
					if(noteData != 0)
					{
						globalTicks += currentTicks;
					}
					to += 2 ;
				}
				else if(msbStatus == 0x09)
				{
					if(noteData != 0)
					{
						globalTicks += currentTicks;
						if (recordNote)
						{
				
							int currTime = (int)(globalTicks / 96);

							if (currTime == lastGlobalTime)
							{
								currNoteCount++;
								if(currNoteCount > 5) currNoteCount = 5;
							}
							else
							{
								lastGlobalTime = currTime;
								currNoteCount = 0;
							}
						
							notes[lastGlobalTime][currNoteCount].time = lastGlobalTime;
							notes[lastGlobalTime][currNoteCount].type = 'o';
							notes[lastGlobalTime][currNoteCount].key[0] =  noteNames[noteData % 12][0];
							notes[lastGlobalTime][currNoteCount].key[1] =  noteNames[noteData % 12][1];
							notes[lastGlobalTime][currNoteCount].octave =  (int )noteData / 12;
			
							// Increase GlobalNoteCount
							maxTime = lastGlobalTime;
						}
					}	
					to += 2 ;
				}
				else if(msbStatus == 0x0C || msbStatus == 0x0D)
				{
					to += 1;
				}
				else
				{
					to += 2;
				}
			}
		}
		// Update current byte at end of track
		cb   += trackSize + 8;
	}
	//USART_puts(USART1, "Song Decoding Complete\n");
}


/*---------------------------------------------------------------------------------------------------*/
// Note to fretboard conversion and control signal functions
/*---------------------------------------------------------------------------------------------------*/

// Return the number of strings that have active note on events in rthe current time frame
int processingNeeded(int playbackTime)
{
	int procNeed = 0;

	int j = 0;
	for (j = 0; j < 6; j++)
	{
		if(notes[playbackTime][j].type == 'o') procNeed++;
	}

	return procNeed;
}

// Process note information and map tpo fretboard
// Subsequently if string needs to be strummed
void processFretsPicks(int playbackTime)
{

	// Convert Note
	int procCount = processingNeeded(playbackTime);
	if(processingNeeded(playbackTime))
	{
		// Pass Null Frets
		passNullFrets();

		// Debug Info
		//USART_puts(USART1, "Time: ");
		//USART_puti(playbackTime);
		//USART_puts(USART1, "\n");

		// Process Frets
		int k = 0;
		for(k = 0; k < procCount; k++)
		{
			convertNoteToStringFret(notes[playbackTime][k]);
		}

		// Process Picks
		for (k = 0; k < 6; k++)
		{
			picks[k] = fret0[k] | fret1[k] | fret2[k] | fret3[k] | fret4[k] | fret5[k] | fret6[k] | fret7[k] | fret8[k]  | fret9[k] | fret10[k] ;
		}
		passFretsPicks(procCount); 
	}
	else
	{
		return;
	}

}

// Convert Node Data yo Fret Data
void convertNoteToStringFret(Note note)
{

	// Debug Info
	USART_puts(USART1, "K:  ");
	USART_puts(USART1, note.key);
	USART_puts(USART1, "   ");

	USART_puts(USART1, "O: ");
	USART_puti(note.octave);
	USART_puts(USART1, "\n");

	// E String Low
	if      ((note.key[0] == 'E') && (note.key[1] == ' ') && (note.octave == 2))  fret0[5] = 1;
	else if ((note.key[0] == 'F') && (note.key[1] == ' ') && (note.octave == 2))  fret1[5] = 1; //PE0
	//else if ((note.key[0] == 'G') && (note.key[1] == 'b') && (note.octave == 2))  fret2[5] = 1;
	//else if ((note.key[0] == 'G') && (note.key[1] == ' ') && (note.octave == 2))  fret3[5] = 1;
	else if ((note.key[0] == 'A') && (note.key[1] == 'b') && (note.octave == 2))  fret4[5] = 1; //PE1
	else if ((note.key[0] == 'D') && (note.key[1] == 'b') && (note.octave == 3))  fret9[5] = 1; //PE11

	// A String
	else if ((note.key[0] == 'A') && (note.key[1] == ' ') && (note.octave == 2))  fret0[4] = 1;
	//else if ((note.key[0] == 'B') && (note.key[1] == 'b') && (note.octave == 2))  fret1[4] = 1;
	else if ((note.key[0] == 'B') && (note.key[1] == ' ') && (note.octave == 2))  fret2[4] = 1; //PE2
	else if ((note.key[0] == 'C') && (note.key[1] == ' ') && (note.octave == 2))  fret3[4] = 1; //PE3
	else if ((note.key[0] == 'D') && (note.key[1] == ' ') && (note.octave == 2))  fret5[4] = 1; //PE4
	else if ((note.key[0] == 'E') && (note.key[1] == 'b') && (note.octave == 2))  fret6[4] = 1; //PE5
	else if ((note.key[0] == 'E') && (note.key[1] == ' ') && (note.octave == 2))  fret7[4] = 1; //PE6
	//else if ((note.key[0] == 'D') && (note.key[1] == 'b') && (note.octave == 3))  fret4[4] = 1;

	// D String
	else if ((note.key[0] == 'D') && (note.key[1] == ' ') && (note.octave == 3))  fret0[3] = 1;
	//else if ((note.key[0] == 'E') && (note.key[1] == 'b') && (note.octave == 3))  fret1[3] = 1;
	//else if ((note.key[0] == 'E') && (note.key[1] == ' ') && (note.octave == 3))  fret2[3] = 1;
	//else if ((note.key[0] == 'F') && (note.key[1] == ' ') && (note.octave == 3))  fret3[3] = 1;
	else if ((note.key[0] == 'G') && (note.key[1] == 'b') && (note.octave == 3))  fret4[3] = 1;  //PE7

	// G String
	else if ((note.key[0] == 'G') && (note.key[1] == ' ') && (note.octave == 3))  fret0[2] = 1;
	else if ((note.key[0] == 'A') && (note.key[1] == 'b') && (note.octave == 3))  fret1[2] = 1;  // PE8
	else if ((note.key[0] == 'A') && (note.key[1] == ' ') && (note.octave == 3))  fret2[2] = 1;  // PE9
	//else if ((note.key[0] == 'B') && (note.key[1] == 'b') && (note.octave == 3))  fret3[2] = 1;

	// B String
	else if ((note.key[0] == 'B') && (note.key[1] == ' ') && (note.octave == 3))  fret0[1] = 1;
	//else if ((note.key[0] == 'C') && (note.key[1] == ' ') && (note.octave == 4))  fret1[1] = 1;
	//else if ((note.key[0] == 'D') && (note.key[1] == 'b') && (note.octave == 4))  fret2[1] = 1;
	else if ((note.key[0] == 'D') && (note.key[1] == ' ') && (note.octave == 4))  fret3[1] = 1;  //PE10
	else if ((note.key[0] == 'E') && (note.key[1] == 'b') && (note.octave == 4))  fret4[1] = 1;
	else if ((note.key[0] == 'G') && (note.key[1] == ' ') && (note.octave == 4))  fret8[1] = 1;  // PE12
	else if ((note.key[0] == 'A') && (note.key[1] == ' ') && (note.octave == 4))  fret10[1] = 1; // PE13

	// High E String
	else if ((note.key[0] == 'E') && (note.key[1] == ' ') && (note.octave == 4))  fret0[0] = 1;
	else if ((note.key[0] == 'F') && (note.key[1] == ' ') && (note.octave == 4))  fret1[0] = 1;
	else if ((note.key[0] == 'G') && (note.key[1] == 'b') && (note.octave == 4))  fret2[0] = 1; // PE14
	//else if ((note.key[0] == 'G') && (note.key[1] == ' ') && (note.octave == 4))  fret3[0] = 1;
	//else if ((note.key[0] == 'A') && (note.key[1] == 'b') && (note.octave == 4))  fret4[0] = 1;
}

// Set all frets to logic value low
void passNullFrets()
{
	int k;
	for (k = 0; k < 6; k++)
	{
		fret0[k] = 0;
		fret1[k] = 0;
		fret2[k] = 0;
		fret3[k] = 0;
		fret4[k] = 0;
		fret5[k] = 0;
		fret6[k] = 0;
		fret7[k] = 0;
		fret8[k] = 0;
		fret9[k] = 0;
		fret10[k] = 0;
		picks[k] = 0;
	}
}


// Convert fretboard values to actual control logic signals using output pins
void passFretsPicks(int procCount)
{

	// Pass Picks
	if(fret1[5]) GPIO_SetBits(GPIOE, GPIO_Pin_0);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_0);

	if(fret4[5]) GPIO_SetBits(GPIOE, GPIO_Pin_1);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_1);

	if(fret2[4]) GPIO_SetBits(GPIOE, GPIO_Pin_2);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_2);

	if(fret3[4]) GPIO_SetBits(GPIOE, GPIO_Pin_3);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_3);

	if(fret5[4]) GPIO_SetBits(GPIOE, GPIO_Pin_4);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_4);

	if(fret6[4]) GPIO_SetBits(GPIOE, GPIO_Pin_5);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_5);

	if(fret7[4]) GPIO_SetBits(GPIOE, GPIO_Pin_6);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_6);

	if(fret4[3]) GPIO_SetBits(GPIOE, GPIO_Pin_7);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_7);

	if(fret1[2]) GPIO_SetBits(GPIOE, GPIO_Pin_8);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_8);

	if(fret2[2]) GPIO_SetBits(GPIOE, GPIO_Pin_9);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_9);

	if(fret3[1]) GPIO_SetBits(GPIOE, GPIO_Pin_10);
	else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_10);

	if(fret9[5]) GPIO_SetBits(GPIOE, GPIO_Pin_11);
		else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_11);

	if(fret8[1]) GPIO_SetBits(GPIOE, GPIO_Pin_12);
		else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_12);

	if(fret10[1]) GPIO_SetBits(GPIOE, GPIO_Pin_13);
		else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_13);

	if(fret2[0])  GPIO_SetBits(GPIOE, GPIO_Pin_14);
		else 		 GPIO_ResetBits(GPIOE, GPIO_Pin_14);


	// Pass Picks
	USART_puts(USART1, "OK");
	USART_puti(procCount);
	USART_puts(USART1, "\n");
	if(procCount == 1)
	{
		if(picks[0]) GPIO_ToggleBits(GPIOD, GPIO_Pin_8);
		if(picks[1]) GPIO_ToggleBits(GPIOD, GPIO_Pin_9);
		if(picks[2]) GPIO_ToggleBits(GPIOD, GPIO_Pin_10);
		if(picks[3]) GPIO_ToggleBits(GPIOD, GPIO_Pin_11);
		if(picks[4]) GPIO_ToggleBits(GPIOD, GPIO_Pin_12);
		if(picks[5]) GPIO_ToggleBits(GPIOD, GPIO_Pin_13);

		Delay(1000000/3);


		if(picks[1]) GPIO_ToggleBits(GPIOD, GPIO_Pin_9);
		if(picks[2]) GPIO_ToggleBits(GPIOD, GPIO_Pin_10);
		if(picks[3]) GPIO_ToggleBits(GPIOD, GPIO_Pin_11);
		if(picks[4]) GPIO_ToggleBits(GPIOD, GPIO_Pin_12);
		if(picks[5]) GPIO_ToggleBits(GPIOD, GPIO_Pin_13);
		Delay(1000000/3);

		if(picks[0]) GPIO_ToggleBits(GPIOD, GPIO_Pin_8);
	}
	else
	{

		if(picks[0]) GPIO_ToggleBits(GPIOD, GPIO_Pin_8);
		if(picks[1]) GPIO_ToggleBits(GPIOD, GPIO_Pin_9);
		if(picks[2]) GPIO_ToggleBits(GPIOD, GPIO_Pin_10);
		if(picks[3]) GPIO_ToggleBits(GPIOD, GPIO_Pin_11);
		if(picks[4]) GPIO_ToggleBits(GPIOD, GPIO_Pin_12);
		if(picks[5]) GPIO_ToggleBits(GPIOD, GPIO_Pin_13);

		Delay(1000000/3);


		if(picks[1]) GPIO_ToggleBits(GPIOD, GPIO_Pin_9);
		if(picks[2]) GPIO_ToggleBits(GPIOD, GPIO_Pin_10);
		if(picks[3]) GPIO_ToggleBits(GPIOD, GPIO_Pin_11);
		if(picks[4]) GPIO_ToggleBits(GPIOD, GPIO_Pin_12);
		if(picks[5]) GPIO_ToggleBits(GPIOD, GPIO_Pin_13);
		Delay(1000000/3);

		if(picks[0]) GPIO_ToggleBits(GPIOD, GPIO_Pin_8);
		/*
		//USART_puts(USART1, "Hello\n");
		if(picks[0])
		{
			//USART_puts(USART1, "Hello333\n");
			GPIO_SetBits(GPIOD, GPIO_Pin_8);
			Delay(1000000/3);
			GPIO_ResetBits(GPIOD, GPIO_Pin_8);
			//Delay(1000000/3);
		}

		if(picks[1])
		{
			//USART_puts(USART1, "Hello333\n");
			GPIO_SetBits(GPIOD, GPIO_Pin_9);
			Delay(1000000/3);
			GPIO_ResetBits(GPIOD, GPIO_Pin_9);
			//Delay(1000000/3);
		}

		if(picks[2])
		{
			//USART_puts(USART1, "Hello333\n");
			GPIO_SetBits(GPIOD, GPIO_Pin_10);
			Delay(1000000/3);
			GPIO_ResetBits(GPIOD, GPIO_Pin_10);
			//Delay(1000000/3);
		}

		if(picks[3])

		{
			//USART_puts(USART1, "Hello333\n");
			GPIO_SetBits(GPIOD, GPIO_Pin_11);
			Delay(1000000/3);
			GPIO_ResetBits(GPIOD, GPIO_Pin_11);
			//Delay(1000000/3);
		}

		if(picks[4])
		{
			//USART_puts(USART1, "Hello333\n");

			GPIO_SetBits(GPIOD, GPIO_Pin_12);
			Delay(1000000/3);
			GPIO_ResetBits(GPIOD, GPIO_Pin_12);
			//Delay(100000/3);
		}

		if(picks[5])
		{
			//USART_puts(USART1, "Hello333\n");
			GPIO_SetBits(GPIOD, GPIO_Pin_13);
			Delay(1000000/3);
			GPIO_ResetBits(GPIOD, GPIO_Pin_13);
			//Delay(1000000/3);
		}
		*/

	}
}

/*---------------------------------------------------------------------------------------------------*/
// Initialization Related Functions
/*---------------------------------------------------------------------------------------------------*/

// Reset Output pins
void resetOutputs()
{
	GPIO_ResetBits(GPIOD, GPIO_Pin_8);
	GPIO_ResetBits(GPIOD, GPIO_Pin_9);
	GPIO_ResetBits(GPIOD, GPIO_Pin_10);
	GPIO_ResetBits(GPIOD, GPIO_Pin_13);
	GPIO_ResetBits(GPIOD, GPIO_Pin_14);
	GPIO_ResetBits(GPIOD, GPIO_Pin_15);
}

// Initialize evry note initially to be inactive i.e. Off
void initNotesArray()
{
	int i = 0;
	int j = 0;
	for(i = 0; i < MAX_TIME; i++)
	{
		for(j = 0; j < 6; j++)
		{
			// Initialize every note as off
			notes[i][j].type = 'n';
		}
	}
}

// Initialize output pins
void initOutputPins()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE);

	// Initalize Fret Output Pins
	GPIO_InitTypeDef GPIO_InitStruct;

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6
			|	GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOE, &GPIO_InitStruct);

	// Initialize Pluck Output Pins
	GPIO_InitStruct.GPIO_Pin = 	GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOD, &GPIO_InitStruct);
}

// 
void init_USART1(uint32_t baudrate)
{
	GPIO_InitTypeDef GPIO_InitStruct; // this is for the GPIO pins used as TX and RX
	USART_InitTypeDef USART_InitStruct; // this is for the USART1 initilization
	NVIC_InitTypeDef NVIC_InitStructure; // this is used to configure the NVIC (nested vector interrupt controller)
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; // Pins 6 (TX) and 7 (RX) are used
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF; // the pins are configured as alternate function so the USART peripheral has access to them
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz; // this defines the IO speed and has nothing to do with the baudrate!
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP; // this defines the output type as push pull mode (as opposed to open drain)
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP; // this activates the pullup resistors on the IO pins
	GPIO_Init(GPIOB, &GPIO_InitStruct); // now all the values are passed to the GPIO_Init() function which sets the GPIO registers
	
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_USART1); //
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_USART1);
	
	USART_InitStruct.USART_BaudRate = baudrate; // the baudrate is set to the value we passed into this init function
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;// we want the data frame size to be 8 bits (standard)
	USART_InitStruct.USART_StopBits = USART_StopBits_1; // we want 1 stop bit (standard)
	USART_InitStruct.USART_Parity = USART_Parity_No; // we don't want a parity bit (standard)
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // we don't want flow control (standard)
	USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx; // we want to enable the transmitter and the receiver
	USART_Init(USART1, &USART_InitStruct); // again all the properties are passed to the USART_Init function which takes care of all the bit setting
	
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // enable the USART1 receive interrupt
	
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn; // we want to configure the USART1 interrupts
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;// this sets the priority group of the USART1 interrupts
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0; // this sets the subpriority inside the group
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // the USART1 interrupts are globally enabled
	NVIC_Init(&NVIC_InitStructure); // the properties are passed to the NVIC_Init function which takes care of the low level stuff
	
	// finally this enables the complete USART1 peripheral
	USART_Cmd(USART1, ENABLE);
}

void initializeTimer(void)
{
	// Initialize TIM Timer

   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

   TIM_TimeBaseInitTypeDef timerInitStructure;
   timerInitStructure.TIM_Prescaler = 40000;
   timerInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
   timerInitStructure.TIM_Period = 3;
   timerInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
   timerInitStructure.TIM_RepetitionCounter = 0;
   TIM_TimeBaseInit(TIM2, &timerInitStructure);
   TIM_Cmd(TIM2, ENABLE);
}

// Interrupt Handler for TIM Module
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}

// Update global time
void updateGlobalTime()
{
     if (TIM_GetCounter(TIM2)  == 3)
    {
		// Set time triggered flag
		timeFlag = 1;
    }

}


/*---------------------------------------------------------------------------------------------------*/
// Bluetooth Related Functions
/*---------------------------------------------------------------------------------------------------*/

// Send string to bluetooth display(TX)
void USART_puts(USART_TypeDef* USARTx, volatile char *s)
{
	while(*s)
	{
		// While register is not empty
		while( !(USARTx->SR & 0x00000040) );
		USART_SendData(USARTx, *s);
		*s++;
	}
}

// Send number to bluetooth display (TX)
void USART_puti(int num)
{
	char str[10];
	itoa(num, str, 10);
	USART_puts(USART1, str);
}

void USART1_IRQHandler(void)
{

	// If interrupt flag is set, i.e. character received
	if( USART_GetITStatus(USART1, USART_IT_RXNE) )
	{
		// Character received in serial input (RX)
		char t = USART1->DR; 

		// If decode instruction received
		if(t == 'd')
		{
			decodeFlag = 1;
		}
		
		// If play button hit
		if(t == 'p')
		{
			playFlag = 1;
			secs = 0;
			currPlaybackTime = 0;
		}
		
		// If stop button hit
		if(t == 's')
		{
			playFlag = 0;
			secs = 0;
			currPlaybackTime = 0;

		}
		
		// Change song choice to song 1
		if(t == '1')
		{
			songChoice = 1;
			songDecode = 1;
			secs = 0;
			currPlaybackTime = 0;
		}
		
		// Change song choice to song 2		
		if(t == '2')
		{
			songChoice = 2;
			songDecode = 1;
			secs = 0;
			currPlaybackTime = 0;
		}
		
		// Change song choice to song 3
		if(t == '3')
		{
			songChoice = 3;
			songDecode = 1;
			secs = 0;
			currPlaybackTime = 0;
		}

		// Change song choice to song 4
		if(t == '4')
		{
			songChoice = 4;
			songDecode = 1;
			secs = 0;
			currPlaybackTime = 0;
		}
	}
}

// Delay a fixed amout of time
void Delay(__IO uint32_t nCount)
{
  while(nCount--) 
  {
  }
}

/*---------------------------------------------------------------------------------------------------*/
// Main Loop Functions
/*---------------------------------------------------------------------------------------------------*/

// Main function that handles overall system
int main(void)
{

	// Initializations
	
	// Init output pins
	initOutputPins();
	
	// Initalize Bluetooth at 9600 bauds
	init_USART1(9600); // initialize USART1 @ 9600 baud
	
	// Initial delay
	Delay(1000000);

	// Reset all output pins
	resetOutputs();
	
	// Test Bluetooth connection
	// Wait till song is asked to be decoded
	while(decodeFlag != 1)
	{
		USART_puts(USART1, " ");
	}

	// Wait to receive bluetooth OK flag
	Delay(1000000);

	// Initalize Timer
	initializeTimer();

	// Main Loop
	
	// Run continuously 
	while (1)
	{
		// If song choice is changed
		if(songDecode)
		{
			songDecode = 0;
			decodeSong();
		}

		// Update global time value used to keep tracfk of song position
		updateGlobalTime();
		
		// If timer module triggered
		if (timeFlag)
		{
			timeFlag = 0;
			
			msecs01++;
			if (msecs01 == 30000)
			{

				secs++;
				msecs01 = 0;

				// If play mode is on
				if(playFlag)
				{
					// Process current onte data based on play back time
					processFretsPicks(currPlaybackTime);

					// Increase Playback Time
					currPlaybackTime++;
					
					// If end of notes array reached, loop
					if(currPlaybackTime > maxTime)
					{
						currPlaybackTime = 0;
					}
				}

			}

		}

	}
}


