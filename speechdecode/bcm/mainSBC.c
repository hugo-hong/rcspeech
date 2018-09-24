
typedef unsigned  char 	UINT8;
typedef short			INT16;
typedef unsigned short	UINT16;
typedef int 			INT32;
typedef unsigned int	UINT32;
typedef int 			BOOL;
typedef unsigned  char 	BYTE;


typedef unsigned short	uint16_t ;
typedef unsigned  char 	uint8_t ;
//typedef char			int8_t;
typedef short			int16_t ;
typedef int 			int32_t ;
//typedef long			int64_t ;
typedef unsigned int 	u_int32_t ;
typedef unsigned int 	uint32_t ;
//typedef unsigned long	uint64_t;

#define TRUE 		1
#define FALSE		0



#include <stdio.h>
#include <math.h>
#include <memory.h>

//#include "ADPCM.h"
//#include "adpcm_thirdparty.h"



#define SBC_MAX_SAMPLES_PER_PACKET   128
#define SBC_MAX_PACKET_SIZE          262  /* 2 + 8 / 2 + 16 * 8 * 16 / 8 */

#define QUALITY_MEDIUM  1   /* pretty good */
#define QUALITY_GREAT   2   /* as good as it will get without an FPU */

/* /config options begin */

#define QUALITY QUALITY_MEDIUM
/* set to cheat a bit with shifts (saves a divide per sample) */
//#define SPEED_OVER_ACCURACY
/* iterator up to 180 use fastest type for your platform */
#define ITER   uint32_t

/* /config options end */

#define DEBUG_DECODING 0

#if QUALITY == QUALITY_MEDIUM

	#define CONST(x)       (x >> 16)
	#define SAMPLE_CVT(x)  (x)
	#define INSAMPLE       int16_t
	#define OUTSAMPLE      uint16_t
	#define FIXED          int16_t
	#define FIXED_S        int32_t
	#define OUT_CLIP_MAX   0x7FFF
	#define OUT_CLIP_MIN   -0x8000

	#define NUM_FRAC_BITS_PROTO 16
	#define NUM_FRAC_BITS_COS   14

#elif QUALITY == QUALITY_GREAT

	#define CONST(x)       (x)
	#define SAMPLE_CVT(x)  (x)
	#define INSAMPLE       int16_t
	#define OUTSAMPLE      uint16_t
	#define FIXED          int32_t
	#define FIXED_S        int64_t
	#define OUT_CLIP_MAX   0x7FFF
	#define OUT_CLIP_MIN   -0x8000

	#define NUM_FRAC_BITS_PROTO 32
	#define NUM_FRAC_BITS_COS   30

#else

	#error "You did not define SBC decoder synthesizer quality to use"

#endif

static FIXED gV[160];

static const FIXED proto_4_40[] =
{
	CONST(0x00000000), CONST(0x00FB7991), CONST(0x02CB3E8B), CONST(0x069FDC59),
	CONST(0x22B63DA5), CONST(0x4B583FE6), CONST(0xDD49C25B), CONST(0x069FDC59),
	CONST(0xFD34C175), CONST(0x00FB7991), CONST(0x002329CC), CONST(0x00FF11CA),
	CONST(0x053B7546), CONST(0x0191E578), CONST(0x31EAB920), CONST(0x4825E4A3),
	CONST(0xEC1F5E6D), CONST(0x083DDC80), CONST(0xFF3773A8), CONST(0x00B32807),
	CONST(0x0061C5A7), CONST(0x007A4737), CONST(0x07646684), CONST(0xF89F23A7),
	CONST(0x3F23948D), CONST(0x3F23948D), CONST(0xF89F23A7), CONST(0x07646684),
	CONST(0x007A4737), CONST(0x0061C5A7), CONST(0x00B32807), CONST(0xFF3773A8),
	CONST(0x083DDC80), CONST(0xEC1F5E6D), CONST(0x4825E4A3), CONST(0x31EAB920),
	CONST(0x0191E578), CONST(0x053B7546), CONST(0x00FF11CA), CONST(0x002329CC)
};

static const FIXED proto_8_80[] =
{
	CONST(0x00000000), CONST(0x0083D8D4), CONST(0x0172E691), CONST(0x034FD9E0),
	CONST(0x116860F5), CONST(0x259ED8EB), CONST(0xEE979F0B), CONST(0x034FD9E0),
	CONST(0xFE8D196F), CONST(0x0083D8D4), CONST(0x000A42E6), CONST(0x0089DE90),
	CONST(0x020E372C), CONST(0x02447D75), CONST(0x153E7D35), CONST(0x253844DE),
	CONST(0xF2625120), CONST(0x03EBE849), CONST(0xFF1ACF26), CONST(0x0074E5CF),
	CONST(0x00167EE3), CONST(0x0082B6EC), CONST(0x02AD6794), CONST(0x00BFA1FF),
	CONST(0x18FAB36D), CONST(0x24086BF5), CONST(0xF5FF2BF8), CONST(0x04270CA8),
	CONST(0xFF93E21B), CONST(0x0060C1E9), CONST(0x002458FC), CONST(0x0069F16C),
	CONST(0x03436717), CONST(0xFEBDD6E5), CONST(0x1C7762DF), CONST(0x221D9DE0),
	CONST(0xF950DCFC), CONST(0x0412523E), CONST(0xFFF44825), CONST(0x004AB4C5),
	CONST(0x0035FF13), CONST(0x003B1FA4), CONST(0x03C04499), CONST(0xFC4086B8),
	CONST(0x1F8E43F2), CONST(0x1F8E43F2), CONST(0xFC4086B8), CONST(0x03C04499),
	CONST(0x003B1FA4), CONST(0x0035FF13), CONST(0x004AB4C5), CONST(0xFFF44825),
	CONST(0x0412523E), CONST(0xF950DCFC), CONST(0x221D9DE0), CONST(0x1C7762DF),
	CONST(0xFEBDD6E5), CONST(0x03436717), CONST(0x0069F16C), CONST(0x002458FC),
	CONST(0x0060C1E9), CONST(0xFF93E21B), CONST(0x04270CA8), CONST(0xF5FF2BF8),
	CONST(0x24086BF5), CONST(0x18FAB36D), CONST(0x00BFA1FF), CONST(0x02AD6794),
	CONST(0x0082B6EC), CONST(0x00167EE3), CONST(0x0074E5CF), CONST(0xFF1ACF26),
	CONST(0x03EBE849), CONST(0xF2625120), CONST(0x253844DE), CONST(0x153E7D35),
	CONST(0x02447D75), CONST(0x020E372C), CONST(0x0089DE90), CONST(0x000A42E6)
};

static const FIXED costab_4[] =
{
	CONST(0x2D413CCD), CONST(0xD2BEC333), CONST(0xD2BEC333), CONST(0x2D413CCD),
	CONST(0x187DE2A7), CONST(0xC4DF2862), CONST(0x3B20D79E), CONST(0xE7821D59),
	CONST(0x00000000), CONST(0x00000000), CONST(0x00000000), CONST(0x00000000),
	CONST(0xE7821D59), CONST(0x3B20D79E), CONST(0xC4DF2862), CONST(0x187DE2A7),
	CONST(0xD2BEC333), CONST(0x2D413CCD), CONST(0x2D413CCD), CONST(0xD2BEC333),
	CONST(0xC4DF2862), CONST(0xE7821D59), CONST(0x187DE2A7), CONST(0x3B20D79E),
	CONST(0xC0000000), CONST(0xC0000000), CONST(0xC0000000), CONST(0xC0000000),
	CONST(0xC4DF2862), CONST(0xE7821D59), CONST(0x187DE2A7), CONST(0x3B20D79E)
};

static const FIXED costab_8[] =
{
	CONST(0x2D413CCD), CONST(0xD2BEC333), CONST(0xD2BEC333), CONST(0x2D413CCD),
	CONST(0x2D413CCD), CONST(0xD2BEC333), CONST(0xD2BEC333), CONST(0x2D413CCD),
	CONST(0x238E7673), CONST(0xC13AD060), CONST(0x0C7C5C1E), CONST(0x3536CC52),
	CONST(0xCAC933AE), CONST(0xF383A3E2), CONST(0x3EC52FA0), CONST(0xDC71898D),
	CONST(0x187DE2A7), CONST(0xC4DF2862), CONST(0x3B20D79E), CONST(0xE7821D59),
	CONST(0xE7821D59), CONST(0x3B20D79E), CONST(0xC4DF2862), CONST(0x187DE2A7),
	CONST(0x0C7C5C1E), CONST(0xDC71898D), CONST(0x3536CC52), CONST(0xC13AD060),
	CONST(0x3EC52FA0), CONST(0xCAC933AE), CONST(0x238E7673), CONST(0xF383A3E2),
	CONST(0x00000000), CONST(0x00000000), CONST(0x00000000), CONST(0x00000000),
	CONST(0x00000000), CONST(0x00000000), CONST(0x00000000), CONST(0x00000000),
	CONST(0xF383A3E2), CONST(0x238E7673), CONST(0xCAC933AE), CONST(0x3EC52FA0),
	CONST(0xC13AD060), CONST(0x3536CC52), CONST(0xDC71898D), CONST(0x0C7C5C1E),
	CONST(0xE7821D59), CONST(0x3B20D79E), CONST(0xC4DF2862), CONST(0x187DE2A7),
	CONST(0x187DE2A7), CONST(0xC4DF2862), CONST(0x3B20D79E), CONST(0xE7821D59),
	CONST(0xDC71898D), CONST(0x3EC52FA0), CONST(0xF383A3E2), CONST(0xCAC933AE),
	CONST(0x3536CC52), CONST(0x0C7C5C1E), CONST(0xC13AD060), CONST(0x238E7673),
	CONST(0xD2BEC333), CONST(0x2D413CCD), CONST(0x2D413CCD), CONST(0xD2BEC333),
	CONST(0xD2BEC333), CONST(0x2D413CCD), CONST(0x2D413CCD), CONST(0xD2BEC333),
	CONST(0xCAC933AE), CONST(0x0C7C5C1E), CONST(0x3EC52FA0), CONST(0x238E7673),
	CONST(0xDC71898D), CONST(0xC13AD060), CONST(0xF383A3E2), CONST(0x3536CC52),
	CONST(0xC4DF2862), CONST(0xE7821D59), CONST(0x187DE2A7), CONST(0x3B20D79E),
	CONST(0x3B20D79E), CONST(0x187DE2A7), CONST(0xE7821D59), CONST(0xC4DF2862),
	CONST(0xC13AD060), CONST(0xCAC933AE), CONST(0xDC71898D), CONST(0xF383A3E2),
	CONST(0x0C7C5C1E), CONST(0x238E7673), CONST(0x3536CC52), CONST(0x3EC52FA0),
	CONST(0xC0000000), CONST(0xC0000000), CONST(0xC0000000), CONST(0xC0000000),
	CONST(0xC0000000), CONST(0xC0000000), CONST(0xC0000000), CONST(0xC0000000),
	CONST(0xC13AD060), CONST(0xCAC933AE), CONST(0xDC71898D), CONST(0xF383A3E2),
	CONST(0x0C7C5C1E), CONST(0x238E7673), CONST(0x3536CC52), CONST(0x3EC52FA0),
	CONST(0xC4DF2862), CONST(0xE7821D59), CONST(0x187DE2A7), CONST(0x3B20D79E),
	CONST(0x3B20D79E), CONST(0x187DE2A7), CONST(0xE7821D59), CONST(0xC4DF2862),
	CONST(0xCAC933AE), CONST(0x0C7C5C1E), CONST(0x3EC52FA0), CONST(0x238E7673),
	CONST(0xDC71898D), CONST(0xC13AD060), CONST(0xF383A3E2), CONST(0x3536CC52)
};

static const int8_t loudness_4[4][4] =
{
	{ -1, 0, 0, 0 }, { -2, 0, 0, 1 },
	{ -2, 0, 0, 1 }, { -2, 0, 0, 1 }
};

static const int8_t loudness_8[4][8] =
{
	{ -2, 0, 0, 0, 0, 0, 0, 1 }, { -3, 0, 0, 0, 0, 0, 1, 2 },
	{ -4, 0, 0, 0, 0, 0, 1, 2 }, { -4, 0, 0, 0, 0, 0, 1, 2 }
};

#define FILE_FORMAT_HEADER_SIZE	2

#define DECODE_STATE_COMMON	0
#define FILE_FORMAT_ADPCM		1
#define FILE_FORMAT_MSBC		2
#define FILE_FORMAT_UNKNOWN	3

#define DECODE_STATE_ERROR_PRCEDURE	10
#define DECODE_STATE_CHECK_FILE_FORMAT	((DECODE_STATE_COMMON << 4) | 1)
#define DECODE_STATE_GET_ENTIRE_GROUP	((DECODE_STATE_COMMON << 4) | 2)
#define DECODE_STATE_DECODE_ONE_GROUP	((DECODE_STATE_COMMON << 4) | 3)
#define DECODE_STATE_DECODE_SUCCESS		((DECODE_STATE_COMMON << 4) | 4)

#define SBC_GROUP_SIZE 	54	//AD 00 00 CRC[1] SCALE_FACTOR[4] DATA[49]
#define SBC_GROUP_HEADER_SIZE	5


#define ADPCM_GROUP_SIZE			60
#define ADPCM_FRAME_SIZE			20
#define ADPCM_GROUP_FRAME_NUM	3
#define ADPCM_GROUP_HEADER_SIZE	2
#define ADPCM_GROUP_TAILER_SIZE		2

#define ADPCM_GROUP_HEADER_SIZE_MODE_2	0
#define ADPCM_GROUP_TAILER_SIZE_MODE_2	3												
#define ADPCM_GROUP_SIZE_MODE_2			4000

const unsigned char c_ucFileFormat_ADPCM_Header[FILE_FORMAT_HEADER_SIZE] = {0x66, 0x01};
const unsigned char c_ucFileFormat_MSBC_Header[FILE_FORMAT_HEADER_SIZE] = {0x66, 0x02};

unsigned char ucCheckFileFormat(FILE * fInput)
{
	unsigned char ucFileHeader[FILE_FORMAT_HEADER_SIZE];
	unsigned char ucIdx;
	unsigned char ucFileFormat = FILE_FORMAT_UNKNOWN;
	unsigned char ucCmp1 = 0,ucCmp2 = 0;

	fread(ucFileHeader, FILE_FORMAT_HEADER_SIZE, 1, fInput);	
	for (ucIdx = 0; ucIdx < FILE_FORMAT_HEADER_SIZE; ucIdx++)
	{
		if (ucFileHeader[ucIdx] == c_ucFileFormat_ADPCM_Header[ucIdx])
		{
			ucCmp1++;
		}
		if (ucFileHeader[ucIdx] == c_ucFileFormat_MSBC_Header[ucIdx])
		{
			ucCmp2++;
		}		
	}

	if (ucCmp1 == FILE_FORMAT_HEADER_SIZE)
	{
		ucFileFormat = FILE_FORMAT_ADPCM;
	}
	else	if (ucCmp2 == FILE_FORMAT_HEADER_SIZE)
	{
		ucFileFormat = FILE_FORMAT_MSBC;
	}	
	else
	{
		ucFileFormat = FILE_FORMAT_UNKNOWN;
	}
	
	fseek(fInput, -FILE_FORMAT_HEADER_SIZE, SEEK_CUR);

	return ucFileFormat;
}

unsigned char ucADPCM_CheckIfFindGroupHeader(FILE * fInput)
{
	unsigned char ucGroupHeader[ADPCM_GROUP_HEADER_SIZE];

	fread(ucGroupHeader, ADPCM_GROUP_HEADER_SIZE, 1, fInput);

	//if ((ucGroupHeader[0] == 0x88) && (ucGroupHeader[1] == 0x00))
	if (ucGroupHeader[0] == 0x88)
	{		
		fseek(fInput, -ADPCM_GROUP_HEADER_SIZE, SEEK_CUR);
		return TRUE;
	}
	
	fseek(fInput, -ADPCM_GROUP_HEADER_SIZE, SEEK_CUR);
	return FALSE;	
}

unsigned char ucADPCM_CheckIfFindGroupTail(FILE * fInput)
{
	unsigned char ucGroupHeader[ADPCM_GROUP_TAILER_SIZE];

	fread(ucGroupHeader, ADPCM_GROUP_TAILER_SIZE, 1, fInput);

	if ((ucGroupHeader[0] == 0x0D) && (ucGroupHeader[1] == 0x0A))
	{		
		fseek(fInput, -ADPCM_GROUP_TAILER_SIZE, SEEK_CUR);
		return TRUE;
	}
	
	fseek(fInput, -ADPCM_GROUP_TAILER_SIZE, SEEK_CUR);
	return FALSE;	
}


unsigned char ucSBC_CheckIfFindGroupHeader(FILE * fInput)
{
	unsigned char ucGroupHeader[SBC_GROUP_HEADER_SIZE];

	fread(ucGroupHeader, SBC_GROUP_HEADER_SIZE, 1, fInput);

	if ((ucGroupHeader[0] == 0x01) && (ucGroupHeader[2] == 0xAD) && (ucGroupHeader[3] == 0x00) && (ucGroupHeader[4] == 0x00))
	{		
		if ((ucGroupHeader[1] == 0x08) || (ucGroupHeader[1] == 0x38) || (ucGroupHeader[1] == 0xC8) || (ucGroupHeader[1] == 0xF8))
		{
			fseek(fInput, -SBC_GROUP_HEADER_SIZE, SEEK_CUR);
			return TRUE;
		}
	}
	
	fseek(fInput, -SBC_GROUP_HEADER_SIZE, SEEK_CUR);
	return FALSE;	
}

void sbc_decoder_reset(void) {
	unsigned i;
	for(i = 0; i < sizeof(gV) / sizeof(*gV); i++) {
		gV[i] = 0;
	}
}

#ifdef SPEED_OVER_ACCURACY
	static inline int32_t mulshift(int32_t val, uint32_t bits) {
		/* return approximately  val / ((2^bits) - 1)  */

		static const uint32_t cooltable[] = {0, 0, 0x55555555, 0x24924925, 0x11111111, 0x08421084,
			0x04104104, 0x02040810, 0x01010101, 0x00804020, 0x00401004, 0x00200400,
			0x00100100, 0x00080040, 0x00040010, 0x00020004, 0x00010001};

		if(bits != 1) val = ((uint64_t)(uint32_t)val * (uint64_t)cooltable[bits]) >> 32;

		return val;
	}
#endif

static void synth_4(OUTSAMPLE* dst, const INSAMPLE* src, FIXED* V){  /* A2DP figure 12.3 */

	ITER i, j;
	const FIXED* tabl = proto_4_40;
	const FIXED* costab = costab_4;

	/* shift */
	for(i = 79; i >= 8; i--) V[i] = V[i - 8];

	/* matrix */
	i = 8;
	do{
		FIXED_S t;
		t  = (FIXED_S)costab[0] * (FIXED_S)src[0];
		t += (FIXED_S)costab[1] * (FIXED_S)src[1];
		t += (FIXED_S)costab[2] * (FIXED_S)src[2];
		t += (FIXED_S)costab[3] * (FIXED_S)src[3];
		costab += 4;
		*V++ = t >> NUM_FRAC_BITS_COS;
	}while(--i);
	V -= 8;


	/* calculate audio samples */
	j = 4;
	do{

		OUTSAMPLE s;
		FIXED_S sample;
		sample  = (FIXED_S)V[  0] * (FIXED_S)tabl[0];
		sample += (FIXED_S)V[ 12] * (FIXED_S)tabl[1];
		sample += (FIXED_S)V[ 16] * (FIXED_S)tabl[2];
		sample += (FIXED_S)V[ 28] * (FIXED_S)tabl[3];
		sample += (FIXED_S)V[ 32] * (FIXED_S)tabl[4];
		sample += (FIXED_S)V[ 44] * (FIXED_S)tabl[5];
		sample += (FIXED_S)V[ 48] * (FIXED_S)tabl[6];
		sample += (FIXED_S)V[ 60] * (FIXED_S)tabl[7];
		sample += (FIXED_S)V[ 64] * (FIXED_S)tabl[8];
		sample += (FIXED_S)V[ 76] * (FIXED_S)tabl[9];
		tabl += 10;
		V++;

		sample >>= (NUM_FRAC_BITS_PROTO - 1 - 2); /* -2 is for the -4 we need to multiply by :) */
		sample = -sample;

		if(sample >= OUT_CLIP_MAX) sample = OUT_CLIP_MAX;
		if(sample <= OUT_CLIP_MIN) sample = OUT_CLIP_MIN;
		s = sample;

		*dst++ = s;

	}while(--j);
}

static void synth_8(OUTSAMPLE* dst, const INSAMPLE* src, FIXED* V){  /* A2DP figure 12.3 */

	ITER i, j;
	const FIXED* tabl = proto_8_80;
	const FIXED* costab = costab_8;

	/* shift */
	for(i = 159; i >= 16; i--) V[i] = V[i - 16];

	/* matrix */
	i = 16;
	do{
		FIXED_S t;
		t  = (FIXED_S)costab[0] * (FIXED_S)src[0];
		t += (FIXED_S)costab[1] * (FIXED_S)src[1];
		t += (FIXED_S)costab[2] * (FIXED_S)src[2];
		t += (FIXED_S)costab[3] * (FIXED_S)src[3];
		t += (FIXED_S)costab[4] * (FIXED_S)src[4];
		t += (FIXED_S)costab[5] * (FIXED_S)src[5];
		t += (FIXED_S)costab[6] * (FIXED_S)src[6];
		t += (FIXED_S)costab[7] * (FIXED_S)src[7];
		costab += 8;
		*V++ = t >> NUM_FRAC_BITS_COS;

	}while(--i);

	V -= 16;

	/* calculate audio samples */
	j = 8;
	do{

		OUTSAMPLE s;
		FIXED_S sample;

		sample  = (FIXED_S)V[  0] * (FIXED_S)tabl[0];
		sample += (FIXED_S)V[ 24] * (FIXED_S)tabl[1];
		sample += (FIXED_S)V[ 32] * (FIXED_S)tabl[2];
		sample += (FIXED_S)V[ 56] * (FIXED_S)tabl[3];
		sample += (FIXED_S)V[ 64] * (FIXED_S)tabl[4];
		sample += (FIXED_S)V[ 88] * (FIXED_S)tabl[5];
		sample += (FIXED_S)V[ 96] * (FIXED_S)tabl[6];
		sample += (FIXED_S)V[120] * (FIXED_S)tabl[7];
		sample += (FIXED_S)V[128] * (FIXED_S)tabl[8];
		sample += (FIXED_S)V[152] * (FIXED_S)tabl[9];
		tabl += 10;
		V++;

		sample >>= (NUM_FRAC_BITS_PROTO - 1 - 3); /* -3 is for the -8 we need to multiply by :) */
		sample = -sample;

		if(sample > OUT_CLIP_MAX) sample = OUT_CLIP_MAX;
		if(sample < OUT_CLIP_MIN) sample = OUT_CLIP_MIN;
		s = sample;

		*dst++ = s;

	}while(--j);
}


static void synth(OUTSAMPLE* dst, const INSAMPLE* src, uint8_t nBands, FIXED* V) {
	/* A2DP sigure 12.3  */

	if(nBands == 4) synth_4(dst, src, V);
	else synth_8(dst, src, V);
}

int32_t BCM_SBC_Decode(uint8_t * DataIn, uint16_t *usDecodedBuffer, FILE * fOutput)
{
	/*=================================
	  *	samplingFreq = 16k
	  *	channelMode = mono
	  *	numOfSubBands = 8
	  *	numOfChannels = 1
	  *	numOfBlocks = 16
	  *	allocationMethod = 0
	  *	bitPool = 26
	  *  scaleFactor = {0x0f, 0x0c, 0x0b, 0x0b, 0x0a, 0x0a, 0x09, 0x09}
	  *                  
	  *================================*/
#define SBC_SAMPLING_FREQ 16
#define SBC_CHANNEL_MODE 0
#define SBC_NUM_OF_SUBBANDS 8
#define SBC_NUM_OF_CHANNELS 1
#define SBC_NUM_OF_BLOCKS	15
#define SBC_ALLOC_METHOD	0
#define SBC_BITPOOL	26			
#define SBC_DECODED_BUFFER_SIZE 	(16*8)

	uint8_t blocks_per_packet = SBC_NUM_OF_BLOCKS;
	uint8_t num_bits = SBC_BITPOOL;
	const uint8_t * buf = (DataIn+1);//ignore CRC byte
	uint16_t len = SBC_GROUP_SIZE;
	//uint16_t usDecodedBuffer[SBC_DECODED_BUFFER_SIZE];

	/* convenience  */
	const uint8_t * end = buf + len;
#define left (end - buf)
	uint16_t * outBufPtr = usDecodedBuffer;

	/* workspace */
	static INSAMPLE samples[16][8]; /*  We blow the stack if this is not static. */				
	ITER i, j, k;
	uint32_t scaleFactors[8]; //= {0x0f, 0x0c, 0x0b, 0x0b, 0x0a, 0x0a, 0x09, 0x09};
	int32_t bitneed[8];
	uint32_t bits[8];
	int32_t bitcount, slicecount, bitslice;
	uint8_t samplingRate, blocks, snr, numSubbands, bitpoolSz, bitpos = 0x80;
	int8_t max_bitneed = 0;	
#ifndef SPEED_OVER_ACCURACY
	int32_t levels[8];
#endif

#if (DEBUG_DECODING == 1)
	const uint8_t *start_buf = buf;
	pr_info("%s: blocks_per_packet = %d, num_bits = %d, buf = %p, len = %d\n",
		__func__, blocks_per_packet, num_bits, buf, len);
	for (i = 0; i < len; i++) {
		pr_info("buf[%d] = 0x%02x\n", i, buf[i]);
	}
#endif

	/* look into the frame header */
	if (left < SBC_GROUP_SIZE) goto out;/* too short a frame header  */

	/*  use Bemote specific constants  */
	samplingRate = 0; /*  always 16000 Hz */
	blocks = blocks_per_packet;
	snr = 0;
	numSubbands = SBC_NUM_OF_SUBBANDS;
	bitpoolSz = num_bits;				

	/* read scale factors */
	/* pr_info("sbc_decode: read scale factors, numSubbands = %d\n", numSubbands); */
	/**/
	for(i = 0; i < numSubbands; i++){

		if(bitpos == 0x80){

			scaleFactors[i] = (*buf) >> 4;
			bitpos = 0x08;
		}
		else{

			scaleFactors[i] = (*buf++) & 0x0F;
			bitpos = 0x80;
		}
	}

	/* calculate bitneed table and max_bitneed value (A2DP 12.6.3.1)  */
	if(snr){

		for(i = 0; i < numSubbands; i++){

			bitneed[i] = scaleFactors[i];
			if(bitneed[i] > max_bitneed) max_bitneed = bitneed[i];
		}
	}
	else{

		const signed char* tbl;

		if(numSubbands == 4) tbl = (const signed char*)loudness_4[samplingRate];
		else tbl = (const signed char*)loudness_8[samplingRate];

		for(i = 0; i < numSubbands; i++){

			if(scaleFactors[i]){

				int loudness = scaleFactors[i] - tbl[i];

				if(loudness > 0) loudness /= 2;
				bitneed[i] = loudness;
			}
			else bitneed[i] = -5;
			if(bitneed[i] > max_bitneed) max_bitneed = bitneed[i];
		}
	}				

	/* fit bitslices into the bitpool */
	bitcount = 0;
	slicecount = 0;
	bitslice = max_bitneed + 1;
	/* pr_info("sbc_decode: fit bitslices into the bitpool, bitslice = %d\n", bitslice ); */
	do{
		bitslice--;
		bitcount += slicecount;
		slicecount = 0;
		for(i = 0; i < numSubbands; i++){

			if(bitneed[i] > bitslice + 1 && bitneed[i] < bitslice + 16) slicecount++;
			else if(bitneed[i] == bitslice + 1) slicecount += 2;
		}

	}while(bitcount + slicecount < bitpoolSz);				

	/* distribute bits */
	for(i = 0; i < numSubbands; i++){

		if(bitneed[i] < bitslice + 2) bits[i] = 0;
		else{

			int8_t v = bitneed[i] - bitslice;
			if(v > 16) v = 16;
			bits[i] = v;
		}
	}		

	/* allocate remaining bits */
	for(i = 0; i < numSubbands && bitcount < bitpoolSz; i++){

		if(bits[i] >= 2 && bits[i] < 16){

			bits[i]++;
			bitcount++;
		}
		else if(bitneed[i] == bitslice + 1 && bitpoolSz > bitcount + 1){

			bits[i] = 2;
			bitcount += 2;
		}
	}
	for(i = 0; i < numSubbands && bitcount < bitpoolSz; i++){

		if(bits[i] < 16){

			bits[i]++;
			bitcount++;
		}
	}				

	/* reconstruct subband samples (A2DP 12.6.4) */
#ifndef SPEED_OVER_ACCURACY
		for(i = 0; i < numSubbands; i++) levels[i] = (1 << bits[i]) - 1;
#endif

	/* pr_info("sbc_decode: reconstruct subband samples, blocks = %d\n", blocks );  */
	for(j = 0; j < blocks; j++){

		for(i = 0; i < numSubbands; i++){

			if(bits[i]){

				uint32_t val = 0;
				k = bits[i];
				do{

					val <<= 1;
#if (DEBUG_DECODING == 1)
					pr_info("%s: buf = %p, offset %d\n",
						__func__, buf, buf-start_buf);
#endif
					if(*buf & bitpos) val++;
					if(!(bitpos >>= 1)){
						bitpos = 0x80;
						buf++;
					}
				}while(--k);

				val = (val << 1) | 1;
				val <<= scaleFactors[i];

				#ifdef SPEED_OVER_ACCURACY
					val = mulshift(val, bits[i]);
				#else
					val /= levels[i];
				#endif

				val -= (1 << scaleFactors[i]);

				samples[j][i] = SAMPLE_CVT(val);
			}
			else samples[j][i] = SAMPLE_CVT(0);
		}
	}		

	//sbc_decoder_reset();

	for(j = 0; j < blocks; j++){
		synth(outBufPtr, samples[j], numSubbands, gV);
		outBufPtr += numSubbands;
	}

	/* if we used a byte partially, skip the rest of it, it is "padding"  */
	if(bitpos != 0x80) buf++;

	out:
#if (DEBUG_DECODING == 1)
		if(left < 0)
			pr_err("SBC: buffer over-read by %d bytes.\n", -left);
		if(left > 0)
			pr_err("SBC: buffer under-read by %d bytes.\n", left);
#endif				
	if (fOutput) {
		fwrite(usDecodedBuffer, sizeof(uint16_t), 120, fOutput);
		fflush(fOutput);
	}
	//memset(usDecodedBuffer, 0, sizeof(usDecodedBuffer));
	return 120*2;
}
