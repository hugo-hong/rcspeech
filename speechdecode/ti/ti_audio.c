/**************************************************************************************************
  Filename:       ti_audio.c

  Description:    audio over BLe, audio decoding app


  Copyright 2013 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>

#include "RAS_lib.h"
#include <cutils/log.h>
#include "../def.h"
#include "ti_audio.h"

#define printf ALOGI
#define perror ALOGE

#define TRUE 1
#define FALSE 0

int fp;

int frame_type;
int SeqNum,PrevSeqNum;
static float throughput, throughput_min = 10000000, throughput_max = 0, throughput_ave=0,throughput_raw_ave=0;
static int cumulatedBytes = 0;
static int totalcumulatedBytes = 0;
static int totalcumulatedRawBytes = 0;
struct timeval  prevTimeThrougput,prevTimeStarting, curTime, prevPacket;
long int elapsedTimeSinceLastPacket = 1;
char ras_pec_mode;
unsigned char audio_data_len;
FILE* fout_wav;
FILE* fout_tic1;
unsigned char Data_len;
int file_open=FALSE;
int streamStarted = /*TRUE*/FALSE;
int nbFrameLost;
int nbFrameReceived;

const char *outFile_wav_3 = "/data/misc/audio/ht_mic_3.wav";
const char *outFile_wav_2 = "/data/misc/audio/ht_mic_2.wav";
const char *outFile_wav_1 = "/data/misc/audio/ht_mic_1.wav";
const char *outFile_wav = "/data/misc/audio/ht_mic_0.wav";
const char *outFile_tic1 = "/data/misc/audio/ht_mic.tic1";

static void adjustAudioRecordFileName(void)
{
	if (0 != access(outFile_wav, F_OK))
	{
		printf("error:%s %s not exist!", __FUNCTION__, outFile_wav);
		return;
	}

	if (0 == access(outFile_wav_1, F_OK))
	{
		if (0 == access(outFile_wav_2, F_OK))
		{
			if (0 == access(outFile_wav_3, F_OK))
			{
				remove(outFile_wav_3);
			}
			
			rename(outFile_wav_2, outFile_wav_3);
			rename(outFile_wav_1, outFile_wav_2);
			rename(outFile_wav, outFile_wav_1);
		}
		else
		{
			rename(outFile_wav_1, outFile_wav_2);
			rename(outFile_wav, outFile_wav_1);
		}
	}
	else
	{
		rename(outFile_wav, outFile_wav_1);
	}
}


static void audioRenameOutputFile( void )
{
    //Open the file
    fout_wav = fopen(outFile_wav, "w");
    if (fout_wav != NULL)
	{
		printf( "===> Wav File Created %s  <=== \n",outFile_wav);

	}
    else
    {
    	perror("fopen3:");
        //exit(-1);
        return;
    }
	fout_tic1 = fopen(outFile_tic1, "w");
	if (fout_tic1 != NULL)
	{
		printf( "===> TIC1 File Created %s  <=== \n",outFile_tic1);
	}
	else
    {
    	perror("fopen4:");
        //exit(-1);
        return;
    }
	file_open=TRUE;
}

static void audioAddWaveHeader ( FILE* fout_wav  )
{
	//Initialize directly subchunksize1
	unsigned char Header[100] = { 0x52, 0x49, 0x46, 0x46, 0xFF, 0xFF, 0xFF, 0xFF, 0x57, 0x41, 0x56, 0x45};


	//subchunksize2
	Header[12] =0x66;
	Header[13] =0x6d;
	Header[14] =0x74;
	Header[15] =0x20;

	Header[16] =16;
	Header[17] =0;
	Header[18] =0;
	Header[19] =0;

	Header[20] =1;
	Header[21] =0;

	Header[22] =1;
	Header[23] =0;

	Header[24] =0x80;
	Header[25] =0x3E;
	Header[26] =0;
	Header[27] =0;

	Header[28] =0x00;
	Header[29] =0x7D;
	Header[30] =0;
	Header[31] =0;

	Header[32] =2;
	Header[33] =0;

	Header[34] =16;
	Header[35] =0;

	Header[36] =0x64;
	Header[37] =0x61;
	Header[38] =0x74;
	Header[39] =0x61;

	Header[40] =0xFF;
	Header[41] =0xFF;
	Header[42] =0xFF;
	Header[43] =0xFF;


	fwrite(Header, sizeof(char), 44, fout_wav);
	printf( "===> Add Wav Header <=== \n");
}

long int audioElapsedTime ( struct timeval *timeStart, struct timeval *timeEnd)
{
	long int diffPrev;
	long int decrementValue;
	int t = 0;

	if (timeStart->tv_usec >= timeEnd->tv_usec) 
	{
		diffPrev = timeStart->tv_usec - timeEnd->tv_usec;
		t = 0;
	} else 
	{
		diffPrev = (timeStart->tv_usec + 1000000) - timeEnd->tv_usec;
		t = 1;
	}

	decrementValue = (diffPrev
			+ 1000000 * (timeStart->tv_sec - timeEnd->tv_sec - t))
			/ 1000; //Res in ms
	return decrementValue;
}

void audioResetStream( void )
{

	gettimeofday(&prevTimeThrougput, NULL);
	gettimeofday(&prevTimeStarting, NULL );

	totalcumulatedBytes = 0;
	totalcumulatedRawBytes = 0;
	cumulatedBytes      = 0;
	throughput          = 0;
	throughput_min      = 10000000;
	throughput_max      = 0;
	throughput_ave      = 0;
	throughput_raw_ave  = 0;
	PrevSeqNum=0;
	ras_pec_mode = 1;
	streamStarted = FALSE;
	nbFrameLost	    = 0;
	nbFrameReceived	    = 0;


	if (RAS_Init(ras_pec_mode))
	{
		//printf("fail to initialize remoTI audio subsystem");
		//exit(-1);
	}

	elapsedTimeSinceLastPacket = audioElapsedTime(&curTime, &prevPacket);

	if (file_open && elapsedTimeSinceLastPacket > 700)
	{
		//Equivalent to dead link timer
		//Close current file and reopen a new one.
		//Stop Frame
		fflush(fout_wav);
		fclose(fout_wav);
		fout_wav = NULL;
		fflush(fout_tic1);
		fclose(fout_tic1);
		fout_tic1 = NULL;

		//Need to add File size in the header of the wav file
		{
			int filesize;
			unsigned char file[4];
			fout_wav = fopen(outFile_wav, "r+b");
			if (fout_wav == NULL)
			{
			    perror("fopen6:");
			    exit(-1);
			}

			fseek(fout_wav, 0L, SEEK_END);
			filesize = ftell(fout_wav);

			file[0] = (unsigned char) (((filesize-44) & 0xFF));
			file[1] = (unsigned char) (((filesize-44) & 0xFF00)>>8);
			file[2] = (unsigned char) (((filesize-44) & 0xFF0000)>>16);
			file[3] = (unsigned char) (((filesize-44) & 0xFF000000)>>24);
			fseek(fout_wav, 40, SEEK_SET);
			fwrite(file, 1, 4, fout_wav);
			fflush(fout_wav);
			fclose(fout_wav);
			fout_wav = NULL;
		}
		file_open=FALSE;
		adjustAudioRecordFileName();

		printf( "===> AUDIO STOP BEFORE START <=== \n");
	}


	if (!file_open)
	{
		audioRenameOutputFile();
		//Add .Wav header to .wave output file
		if (fout_wav)
			audioAddWaveHeader(fout_wav);
	}



}

void audio_ParseData(unsigned char *pdu, unsigned short len, short *decode_buf, unsigned short *decode_len)
{
	long int decrementValue, elapsed_time;

	*decode_len = 0;//you must set it to zero,sometimes there is no data for decode buffer

	frame_type= pdu[/*3*/0] & 0x7;
	SeqNum= (pdu[/*3*/0] >>3 )& 0x1F;
	//s = g_string_new(NULL);

	Data_len = (len -1);//This is audio data length only (audio frame header+ audio data)
	//printf("FT = 0x%02x ,SeqNum: %d, len: %d\n", frame_type, SeqNum, len);

	
	cumulatedBytes += Data_len;
	totalcumulatedBytes += Data_len;
	totalcumulatedRawBytes += Data_len+3+4+1;//is it correct?

	gettimeofday(&curTime, NULL );
	decrementValue = audioElapsedTime(&curTime, &prevTimeThrougput);
	elapsed_time = audioElapsedTime(&curTime, &prevTimeStarting);

	//check if 1s has elapse since last packet.
	if (decrementValue > 1000)
	{
		//Calculate and Display Throughput
		throughput = (float) (8  * (((float)cumulatedBytes) / ((float)decrementValue)));
		(throughput_min > throughput) ? throughput_min = throughput : throughput_min;
		(throughput_max < throughput) ? throughput_max = throughput : throughput_max;
		gettimeofday(&prevTimeThrougput, NULL );
	}

	if (elapsed_time > 500) //Not meaningful before
	{ 
		throughput_ave = (float)(8 * ((float)totalcumulatedBytes) / ((float)elapsed_time));
		throughput_raw_ave = (float)(8 * ((float)totalcumulatedRawBytes) / ((float)elapsed_time));
	}

	if (decrementValue > 1000) 
	{
		cumulatedBytes = 0;
		printf( "ElapsedTime(ms): %ld, throughput(kbps): %2.2f (%2.2f/%2.2f)  Ave:(%2.2f) Raw Ave: (%2.2f), AudioPER:%2.2f%%, ( %d lost over %d sent)  \n",
								elapsed_time,
								throughput,
								throughput_min,
								throughput_max,
								throughput_ave,
								throughput_raw_ave,
								(double)(100*(double)nbFrameLost/(double)(nbFrameLost+nbFrameReceived)),
								nbFrameLost,
								nbFrameReceived+nbFrameLost
								 );
	}

	//Throughput estimation
	if (frame_type==RAS_START_CMD)
	{
		printf( "===> AUDIO START  <=== \n");
		audioResetStream();
		streamStarted = TRUE;
	}
	else if (frame_type == RAS_DATA_TIC1_CMD)
	{
		if(!streamStarted) 
		{
			audioResetStream();
			streamStarted = TRUE;
		}
		nbFrameReceived++;
		gettimeofday(&prevPacket, NULL);
		if (PrevSeqNum+1 != SeqNum)
		{
			if (SeqNum < (PrevSeqNum+1))
			{
				nbFrameLost += 32 + SeqNum - (PrevSeqNum+1);
			}
			else
				nbFrameLost += SeqNum - (PrevSeqNum+1);
			ALOGE("FRAME LOST !!!!(%d != %d) (%d nbFrameLost for %d sent\n", PrevSeqNum+1, SeqNum,nbFrameLost ,nbFrameLost+ nbFrameReceived );
		}

		if (SeqNum==31 )
		PrevSeqNum=-1;
		else
		PrevSeqNum=SeqNum;

		audio_data_len = Data_len; //Remove audio frame Header

		//Decode Frame if any exist.
		//if (previousFrameExist)//don't drop the last frame when stop cmd comes.
		{
			//if(audio_data_len<256)//comparison is always true due to limited range of data type
			{
				//printf("decode Frame %d\n", PrevSeqNum  );
				RAS_Decode( RAS_DECODE_TI_TYPE1, (uint8*)pdu + 1, audio_data_len, /*temp*/decode_buf, /*&decLength*/decode_len );
				if (fout_wav != NULL)
				{
					/*Write the decoded audio to file*/
					if (*decode_len > 0) {
						fwrite(/*temp*/decode_buf, sizeof(short), /*decLength*/(*decode_len)/2, fout_wav);
					}
					else
					{
						ALOGE("decode_len is 0!");
					}
				}

				if (fout_tic1 != NULL)
				{
					/*Write the decoded audio to file*/
					fwrite(&audio_data_len, sizeof(char), 1, fout_tic1);
					fwrite((unsigned char*)pdu + 1, sizeof(char), audio_data_len, fout_tic1);
				}
			}
			/*else
			{
				printf("audio Decode Overfow\n");
			}*/
		}
	}
	else if (frame_type == RAS_STOP_CMD)
	{
		printf( "===> AUDIO STOP  <=== \n");
		streamStarted = FALSE;
		//Stop Frame
		if (fout_wav != NULL)
		{
			fflush(fout_wav);
			fclose(fout_wav);
			fout_wav = NULL;
		}
		if(fout_tic1 != NULL)
		{
			fflush(fout_tic1);
			fclose(fout_tic1);
			fout_tic1 = NULL;
		}

		//Need to add File size in the header of the wav file
		{
			int filesize;
			unsigned char file[4];
			fout_wav = fopen(outFile_wav, "r+b");
			if (fout_wav != NULL)
			{
				printf( "===> Wav File open for length update: %s  <=== \n",outFile_wav);
			}
			else
			{
			    perror("fopen5:");
			    //exit(-1);
			    return;
			}
			fseek(fout_wav, 0L, SEEK_END);
			filesize = ftell(fout_wav);
			printf( "===> Wav File %s length:%d\n",outFile_wav, filesize);
			file[0] = (unsigned char) (((filesize-44) & 0xFF));
			file[1] = (unsigned char) (((filesize-44) & 0xFF00)>>8);
			file[2] = (unsigned char) (((filesize-44) & 0xFF0000)>>16);
			file[3] = (unsigned char) (((filesize-44) & 0xFF000000)>>24);
			fseek(fout_wav, 40, SEEK_SET);
			fwrite(file, 1, 4, fout_wav);
			fflush(fout_wav);
			fclose(fout_wav);
			fout_wav = NULL;
		}
		file_open=FALSE;
		adjustAudioRecordFileName();
	}
}
