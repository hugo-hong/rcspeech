#ifndef RSA_LIB_H
#define RSA_LIB_H

#ifdef __cplusplus
extern "C" {
#endif


#if !defined PACK_1
#define PACK_1
#endif


#if defined(_MSC_VER) || defined(unix) || (defined(__ICC430__) && (__ICC430__==1))
//#pragma pack(1)
#endif


/////////////////////////////////////////////////////////////////////////////
// Defines

#define MAX_INPUT_BUF_SIZE 		128

#define RAS_PACKET_LOST 		0
#define RAS_DECODE_TI_TYPE1		1

#define RAS_NO_PEC		   		0
#define RAS_PEC_MODE1   		1

//RAS Software Version: v1.0
#define RAS_SOFTWARE_VERSION	0x0100
/////////////////////////////////////////////////////////////////////////////
// Typedefs
#ifndef int8
typedef signed   char   int8;
#endif

#ifndef uint8
typedef unsigned char   uint8;
#endif

#ifndef int16
typedef signed   short  int16;
#endif

#ifndef uint16
typedef unsigned short  uint16;
#endif

/////////////////////////////////////////////////////////////////////////////
// Global variable


/////////////////////////////////////////////////////////////////////////////
// Function declarations
/**************************************************************************************************
 *
 * @fn      RAS_GetVersion
 *
 * @brief   RemoTI Audio Subsystem, retrieve software version
 *
 * input parameters
 *
 * none
 *
 * output parameters
 *
 * None.
 *
 * @return      .
 * Software Version.	MSB	 Major revision number
 *						LSB: Minor revision number
 */
uint16 RAS_GetVersion( void );

/**************************************************************************************************
 *
 * @fn      RAS_Init
 *
 * @brief   RemoTI Audio Subsystem, initialization function
 *
 * input parameters
 *
 * @param   pec_mode:    	Packet Error concealment algorithm to apply:
 * 							RAS_NO_PEC(0): 		None (default)
 * 							RAS_PEC_MODE1(1): 	Replace lost packets by last valid.
 *
 * output parameters
 *
 * None.
 *
 * @return      .
 * status.	0				SUCCESS
 * 			-1				ERROR: INVALID PARAMETER
 */
uint8 RAS_Init( uint8 pec_mode );


/**************************************************************************************************
 *
 * @fn      RAS_Decode
 *
 * @brief   RemoTI Audio Subsystem, decoding function. decode encoded audioframe to PCM samples.
 *
 * input parameters
 *
 * @param   option:    		decoding option. can be pure decoding, or packet lot concealment algorithm:
 * 							RAS_PACKET_LOST(0)
 * 							RAS_DECODE(1)
 * @param   input: 			address of the buffer to decode, this buffer must include the 3 bytes header..
 *
 * @param   inputLenght:  	length of the buffer to decode, excluding the 3 bytes header.
 * 							cannot be greater than 128 (MAX_INPUT_BUF_SIZE);
 *
 * output parameters
 *
 * @param   output:     	buffer where the decoded PCM will be written. This buffer must be allocated by the caller.
 * 							it must have a length of 4 times the inputLength variable
 *
 * @param   outputLenght:  	length of the decoded buffer.
 * 							max possible value 512 (4*MAX_INPUT_BUF_SIZE);
 *
 *
 * @return      .
 * status.	0				SUCCESS
 * 			-1				ERROR: INVALID PARAMETER
 *
 */
uint8 RAS_Decode( uint8 option, uint8* input, uint16 inputLenght, int16* output,uint16 *outputLenght );

#ifdef __cplusplus
}
#endif

#endif // RSA_LIB_H
