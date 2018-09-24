/*
 * Copyright (C) 2011 The Android Open Source Project
 *
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
 */
#include <linux/input.h>
#include <linux/hidraw.h>
#include "speech_decode.h"
#include "def.h"
#include "ti/ti_audio.h"
#include "bcm/mainSBC.h"
#include "nordic/adpcm/dvi_adpcm.h"
#include "nordic/opus-1.1.3/opus.h"
#include "nordic/bv32fp-1.2/typedef.h"
#include "nordic/bv32fp-1.2/bvcommon.h"
#include "nordic/bv32fp-1.2/bv32cnst.h"
#include "nordic/bv32fp-1.2/bv32strct.h"
#include "nordic/bv32fp-1.2/bv32.h"
#include "nordic/bv32fp-1.2/bitpack.h"
#include "log/speech_log.h"

struct pcm_config pcm_config_vg = {
    .channels = 1,
    .rate = 16000,
    .period_size = DEFAULT_PERIOD_SIZE,
    .period_count = PLAYBACK_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

static int part_index = 0;
static int total_lenth = 0;
static int receive_index = 0;
static unsigned char ADPCM_Data_Frame[ADPCM_DATA_PART_NUM*GATT_PDU_LENGTH];
static OpusDecoder *st = NULL;
static struct BV32_Decoder_State bv32_st;
static short decode_buf[1024];
static int hidraw_fd = -1;
static int speech_rc_platform = RC_PLATFORM_UNKOWN;

int speech_check_input_parameters(uint32_t sample_rate, audio_format_t format, int channel_count)
{
    LOGFUNC("%s(sample_rate=%d, format=%d, channel_count=%d)", __FUNCTION__, sample_rate, format, channel_count);

    if (format != AUDIO_FORMAT_PCM_16_BIT)
        return -EINVAL;

    if ((channel_count < 1) || (channel_count > 2))
        return -EINVAL;

    switch(sample_rate) {
    case 8000:
    case 11025:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

size_t speech_get_input_buffer_size(uint32_t sample_rate, audio_format_t format, int channel_count)
{
    size_t size;

    LOGFUNC("%s(sample_rate=%d, format=%d, channel_count=%d)", __FUNCTION__, sample_rate, format, channel_count);

    if (speech_check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames */
    size = (pcm_config_vg.period_size * sample_rate) / pcm_config_vg.rate;
    size = ((size + 15) / 16) * 16;

    return size * channel_count * sizeof(short);
}

uint32_t speech_in_get_sample_rate(const struct audio_stream *stream __unused)
{
    return pcm_config_vg.rate;
}

int speech_in_set_sample_rate(struct audio_stream *stream __unused, uint32_t rate __unused)
{	
    return 0;
}

size_t speech_in_get_buffer_size(const struct audio_stream *stream __unused)
{   

    return speech_get_input_buffer_size(pcm_config_vg.rate,
                                 AUDIO_FORMAT_PCM_16_BIT,
                                 pcm_config_vg.channels);
}

audio_channel_mask_t speech_in_get_channels(const struct audio_stream *stream __unused)
{
    if (pcm_config_vg.channels == 1) {
        return AUDIO_CHANNEL_IN_MONO;
    } else {
        return AUDIO_CHANNEL_IN_STEREO;
    }
}

audio_format_t speech_in_get_format(const struct audio_stream *stream __unused)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

int speech_in_set_format(struct audio_stream *stream __unused, audio_format_t format __unused)
{
    return 0;
}

int speech_in_standby(struct audio_stream *stream __unused)
{
    return 0;
}

int speech_in_dump(const struct audio_stream *stream __unused, int fd __unused)
{
    return 0;
}

int speech_in_set_parameters(struct audio_stream *stream __unused, const char *kvpairs __unused)
{	
    return 0;
}

char * speech_in_get_parameters(const struct audio_stream *stream __unused,
                                const char *keys __unused)
{	
    return strdup("");
}

int speech_in_set_gain(struct audio_stream_in *stream __unused, float gain __unused)
{
    return 0;
}

inline static int is_start_frame(unsigned char *buf)
{
    ////it is best to compare all the 19 bytes to be zero
    return ((buf[0] & 0x7) == RAS_START_CMD && buf[1] == 0 && buf[2] == 0 && buf[3] == 0 && buf[12] == 0 && buf[19] == 0)?1:0;
}

inline static int is_stop_frame(unsigned char *buf)
{
    ////it is best to compare all the 19 bytes to be zero
    return ((buf[0] & 0x7) == RAS_STOP_CMD && buf[1] == 0 && buf[2] == 0 && buf[3] == 0 && buf[12] == 0 && buf[19] == 0)?1:0;
}

inline static int is_data_frame_part_1(unsigned char *buf)
{
    return (buf[0] & 0x7) == RAS_DATA_TIC1_CMD;
}

static ssize_t in_read_ti(struct audio_stream_in *stream __unused, void* buffer,
                       size_t bytes __unused)
{
    int count = 0;
    unsigned short decode_len = 0;
    unsigned char buf[HIDRAW_PDU_LENGTH];    
	
begin:
    memset(buf, 0, sizeof(buf));
    count = read(hidraw_fd, buf, sizeof(buf));
    if (count <= 0) {
        //ALOGI("%s no data!", __FUNCTION__);
        return 0;
    }

    /*ALOGI("%s length:%d,first 11 byte:0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X\n", __FUNCTION__, count,
		buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10]);*/
    //ALOGI("%s length:%d,first 2 byte:0X%02X,0X%02X\n", __FUNCTION__, count, buf[1],buf[2]);

    if (count != HIDRAW_PDU_LENGTH || buf[0] != REPORT_ID) {
        ALOGI("%s drop gabage len:%d!", __FUNCTION__, count);
        goto begin;
    }

    decode_len = 0;

    if (is_start_frame(buf + 1)) {
        ALOGI("%s RAS_START_CMD comes!", __FUNCTION__);
        if (part_index > 0) {
            ALOGE("%s drop some parts.part_index:%d!", __FUNCTION__, part_index);
        }
        part_index = 0;
        memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
        memset(decode_buf, 0, sizeof(decode_buf));
        audio_ParseData(buf + 1, GATT_PDU_LENGTH, decode_buf, &decode_len);
        goto begin;
    } else if (is_stop_frame(buf + 1)) {
        ALOGI("%s RAS_STOP_CMD comes!", __FUNCTION__);
        if (part_index == ADPCM_DATA_PART_NUM) {
            memset(decode_buf, 0, sizeof(decode_buf));
            audio_ParseData(ADPCM_Data_Frame, sizeof(ADPCM_Data_Frame), decode_buf, &decode_len);
            if (decode_len > 0) {
                memcpy(buffer, decode_buf, decode_len);
            }
            count = decode_len;
        } else {
            if (part_index > 0) {
                ALOGE("%s stop frame drop some parts.part_index:%d!", __FUNCTION__, part_index);
            }
            count = 0;
        }
        memset(decode_buf, 0, sizeof(decode_buf));
        audio_ParseData(buf + 1, GATT_PDU_LENGTH, decode_buf, &decode_len);
        part_index = 0;
        memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
    } else if (is_data_frame_part_1(buf + 1)) {
        //ALOGI("%s RAS_DATA_TIC1_CMD comes!", __FUNCTION__);
        if (part_index > ADPCM_DATA_PART_NUM) {
            ALOGE("%s tic1 drop some parts.part_index:%d!", __FUNCTION__, part_index);
            //I find the right beginning?God bless me.
            memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
            memcpy(ADPCM_Data_Frame, buf + 1, GATT_PDU_LENGTH);
            part_index = 1;
            goto begin;
        } else if (part_index == ADPCM_DATA_PART_NUM) {
            if (is_data_frame_part_1(ADPCM_Data_Frame)) {
                memset(decode_buf, 0, sizeof(decode_buf));
                audio_ParseData(ADPCM_Data_Frame, sizeof(ADPCM_Data_Frame), decode_buf, &decode_len);
                if (decode_len > 0) {
                   memcpy(buffer, decode_buf, decode_len);
                }
                count = decode_len;
            } else {
                ALOGE("%s why the first part is bad?drop all the five parts!", __FUNCTION__);
            }

            //I find the right beginning.
            memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
            memcpy(ADPCM_Data_Frame, buf + 1, GATT_PDU_LENGTH);
            part_index = 1;

            if (decode_len == 0) {
                goto begin;
            }
        } else if (part_index == (ADPCM_DATA_PART_NUM - 1)) {
            if (is_data_frame_part_1(ADPCM_Data_Frame)) {
                /* treat this part as data, is it correct?
                     the system and bt stack will not drop data,I think that this part is data!
                     buf[1] meets the function of is_data_frame_part_1 by chance.
                */
                memcpy(ADPCM_Data_Frame + part_index*GATT_PDU_LENGTH, buf + 1, GATT_PDU_LENGTH);
                memset(decode_buf, 0, sizeof(decode_buf));
                audio_ParseData(ADPCM_Data_Frame, sizeof(ADPCM_Data_Frame), decode_buf, &decode_len);
                if (decode_len > 0) {
                    memcpy(buffer, decode_buf, decode_len);
                }
                count = decode_len;
                part_index = 0;
                memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
                if (decode_len == 0) {
                    goto begin;
                }
            } else {
                ALOGE("%s: why the first part is bad?drop all the four parts!", __FUNCTION__);
                //I find the right beginning?God bless me.
                memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
                memcpy(ADPCM_Data_Frame, buf + 1, GATT_PDU_LENGTH);
                part_index = 1;

                goto begin;
            }
        } else {
            if (part_index == 0) {
                //I find the right beginning?God bless me.
                memcpy(ADPCM_Data_Frame + part_index*GATT_PDU_LENGTH, buf + 1, GATT_PDU_LENGTH);
                part_index++;
            } else if (is_data_frame_part_1(ADPCM_Data_Frame)) {
                 /* treat this part as data, is it correct?
                     the system and bt stack will not drop data,I think that this part is data!
                     buf[1] meets the function of is_data_frame_part_1 by chance.
                */
                memcpy(ADPCM_Data_Frame + part_index*GATT_PDU_LENGTH, buf + 1, GATT_PDU_LENGTH);
                part_index++;
            } else {
                ALOGE("%s tic2 drop some parts.part_index:%d!", __FUNCTION__, part_index);
                //I find the right beginning?God bless me.
                memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
                memcpy(ADPCM_Data_Frame, buf + 1, GATT_PDU_LENGTH);
                part_index = 1;
            }

            goto begin;
        }
    } else {
        if (part_index > ADPCM_DATA_PART_NUM) {
            ALOGE("%s data frame drop some parts,drop this part also.part_index:%d!", __FUNCTION__, part_index);
            memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
            part_index = 0;
            goto begin;
        } else if (part_index == ADPCM_DATA_PART_NUM) {
            if (is_data_frame_part_1(ADPCM_Data_Frame)) {
                ALOGE("%s is it right to decode this frame?decode it...", __FUNCTION__);
                memset(decode_buf, 0, sizeof(decode_buf));
                audio_ParseData(ADPCM_Data_Frame, sizeof(ADPCM_Data_Frame), decode_buf, &decode_len);
                if (decode_len > 0) {
                    memcpy(buffer, decode_buf, decode_len);
                }
                count = decode_len;
            } else {
                ALOGE("%s drop all the five parts,where is the start part?", __FUNCTION__);
            }
            ALOGE("%s drop this part,why does it happen?", __FUNCTION__);
            memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
            part_index = 0;
            if (decode_len == 0) {
                goto begin;
            }
        } else if (part_index == (ADPCM_DATA_PART_NUM - 1)) {
            if (is_data_frame_part_1(ADPCM_Data_Frame)) {
                memcpy(ADPCM_Data_Frame + part_index*GATT_PDU_LENGTH, buf + 1, GATT_PDU_LENGTH);
                memset(decode_buf, 0, sizeof(decode_buf));
                audio_ParseData(ADPCM_Data_Frame, sizeof(ADPCM_Data_Frame), decode_buf, &decode_len);
                if (decode_len > 0) {
                    memcpy(buffer, decode_buf, decode_len);
                }
                count = decode_len;
            } else {
                ALOGE("%s 2 drop all the five parts,where is the start part?", __FUNCTION__);
            }
            part_index = 0;
            memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
            if (decode_len == 0) {
                goto begin;
            }
        } else {
            if (part_index > 0 && is_data_frame_part_1(ADPCM_Data_Frame)) {
                memcpy(ADPCM_Data_Frame + part_index*GATT_PDU_LENGTH, buf + 1, GATT_PDU_LENGTH);
                part_index++;
            } else {
                if (part_index > 0)
                    ALOGE("%s drop all the five parts and part,where is the start part?part_index:%d", __FUNCTION__, part_index);
                else
                    ALOGE("%s drop this part,where is the start part?part_index:0", __FUNCTION__);
                part_index = 0;
                memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));
            }
            goto begin;
        }
    }

    return count;
}

inline static int is_start_frame_bcm(unsigned char *buf)
{
    ////it is best to compare all the 19 bytes to be zero
    return ((buf[0]) == 0x01 && buf[2] == 0xad && buf[3] == 0 && buf[4] == 0)?1:0;
}

static ssize_t in_read_bcm(struct audio_stream_in *stream __unused, void* buffer,
                       size_t bytes __unused)
{
    int count = 0;
    unsigned char buf[HIDRAW_PDU_LENGTH];

begin:
    memset(buf, 0, sizeof(buf));
    count = read(hidraw_fd, buf, sizeof(buf));
    if (count <= 0) {
        //ALOGI("%s no data!", __FUNCTION__);
        return 0;
    }

    /*ALOGE("%s length:%d,part_index=%d,first 11 byte:0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X\n", __FUNCTION__, count,part_index,
		buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10]);*/
    //ALOGI("%s length:%d,first 2 byte:0X%02X,0X%02X\n", __FUNCTION__, count, buf[1],buf[2]);

    if (count != HIDRAW_PDU_LENGTH || buf[0] != REPORT_ID) {
        ALOGI("%s drop gabage len:%d!", __FUNCTION__, count);
        goto begin;
    }

    if (part_index == 0) {
        if (is_start_frame_bcm(buf + 1)) {
            memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));//use ti's data buf,it is big enough
            memcpy(ADPCM_Data_Frame, buf + 1 + 5, 15);//copy the last 15 bytes,skip report id,flag,frame seq,sync
            part_index++;
        } else {
            ALOGE("%s drop gabage, not start frame!", __FUNCTION__);
        }
        goto begin;
    } else if (part_index == 1) {
        memcpy(ADPCM_Data_Frame + 15, buf + 1, GATT_PDU_LENGTH);
        part_index++;
        goto begin;
    } else {
        memcpy(ADPCM_Data_Frame + 15 + GATT_PDU_LENGTH, buf + 1, GATT_PDU_LENGTH);
        part_index = 0;
        memset(decode_buf, 0, sizeof(decode_buf));
        count = BCM_SBC_Decode(ADPCM_Data_Frame, (uint16_t *)decode_buf, 0);
        if (count > 0) {
            log_write((unsigned char *)decode_buf, count);
            memcpy(buffer, decode_buf, count);
        }
    }

    return count;
}

static ssize_t in_read_dialog(struct audio_stream_in *stream __unused, void* buffer __unused,
                       size_t bytes __unused)
{
    return 0;
}

static ssize_t in_read_nordic(struct audio_stream_in *stream __unused, void* buffer,
                       size_t bytes __unused)
{
    int count = 0;
    unsigned char buf[HIDRAW_PDU_LENGTH];    

begin:
    memset(buf, 0, sizeof(buf));
    count = read(hidraw_fd, buf, sizeof(buf));
    if (count <= 0) {
        //ALOGI("%s no data!", __FUNCTION__);
        return 0;
    }

   /*ALOGI("%s length=%d,part_index=%d first 11 byte:0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X,0X%02X\n", __FUNCTION__, count,part_index,
		buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10]);*/
    //ALOGI("%s length:%d,first 2 byte:0X%02X,0X%02X\n", __FUNCTION__, count, buf[1],buf[2]);
    if ((buf[0] != REPORT_ID_NORDIC_OPUS) && (buf[0] != REPORT_ID_NORDIC_ADPCM) && (buf[0] != REPORT_ID_NORDIC_BV32)) {
        ALOGW("%s drop gabage len:%d!", __FUNCTION__, count);
        goto begin;
    }
    if (buf[0] == REPORT_ID_NORDIC_BV32) {
        struct BV32_Bit_Stream BitStruct;

        BV32_BitUnPack(buf+1, &BitStruct);
        BV32_Decode(&BitStruct, &bv32_st, decode_buf);

        count = BV32_FRAME_LEN * sizeof(short);
        log_write((unsigned char *)decode_buf, count);
	    memcpy(buffer, decode_buf, count);
	}
	else if (buf[0] == REPORT_ID_NORDIC_OPUS)
	{
        if (part_index == 0) {
            if (count == 21) {
                //ALOGI(" count=%d",count);
                memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));//use ti's data buf,it is big enough
                memcpy(ADPCM_Data_Frame, buf + 3, count-3);//
                total_lenth = (buf[1] << 8) + buf[2];
                receive_index += (count-3);
                part_index++;
            }
                goto begin;
            }else {
                memcpy(ADPCM_Data_Frame+receive_index, buf + 1, count-1);
                receive_index += (count-1);
                if ( ((count-1)<20) || receive_index == total_lenth)
                {
                    memset(decode_buf, 0, sizeof(decode_buf));
                    count = opus_decode(st, ADPCM_Data_Frame, receive_index, decode_buf, sizeof(decode_buf), 0);
                    if (count > 0) {
                        count *= 2;
                        log_write((unsigned char *)decode_buf, count);
                        memcpy(buffer, decode_buf, count);
                    }
                    part_index = 0;
                    receive_index = 0;
                    total_lenth = 0;
                }
               else {
                 part_index++;
                 goto begin;
              }
        }
    }
    else if(buf[0] == REPORT_ID_NORDIC_ADPCM)
    {
        if (part_index == 0) {
            memset(ADPCM_Data_Frame, 0, sizeof(ADPCM_Data_Frame));//use ti's data buf,it is big enough
            memcpy(ADPCM_Data_Frame, buf + 1, 20);
            part_index++;
            ALOGI("Receive head");
            goto begin;
        } else if ((part_index == 1)||(part_index == 2)) {
            memcpy(ADPCM_Data_Frame + 20*part_index, buf + 1, 20);
            part_index++;
            ALOGI("Receive data");
            goto begin;
        } else {
            memcpy(ADPCM_Data_Frame + 60, buf + 1, 7);
            part_index = 0;
            ALOGI("Receive end");
            memset(decode_buf, 0, sizeof(decode_buf));
            //count = BCM_SBC_Decode(ADPCM_Data_Frame, decode_buf, 0);
            dvi_adpcm_decode(ADPCM_Data_Frame, 67, decode_buf, &count, NULL);
            if (count > 0) {
                log_write((unsigned char *)decode_buf, count);
                memcpy(buffer, decode_buf, count);
            }
        }
    }

    return count;
}

ssize_t speech_in_read(struct audio_stream_in *stream __unused, void* buffer,
                       size_t bytes __unused)
{
    //ALOGE("%s speech_rc_platform=%d",__FUNCTION__,speech_rc_platform);
    if (speech_rc_platform == RC_PLATFORM_TI) {
        return in_read_ti(stream, buffer, bytes);
    } else if (speech_rc_platform == RC_PLATFORM_BCM) {
        return in_read_bcm(stream, buffer, bytes);
    } else if (speech_rc_platform == RC_PLATFORM_DIALOG) {
        return in_read_dialog(stream, buffer, bytes);
    } else if (speech_rc_platform == RC_PLATFORM_NORDIC) {
        return in_read_nordic(stream, buffer, bytes);
    } else {
        return 0;
    }
}

uint32_t speech_in_get_input_frames_lost(struct audio_stream_in *stream __unused)
{
    return 0;
}

int speech_in_close_stream_in_open_stream(int hidraw_index) {
    int fd = -1;
    int error = 0;
    struct hidraw_devinfo devInfo;
    char devicePath[32] = {0};

    sprintf(devicePath, "/dev/hidraw%d", hidraw_index);

    //check hidraw
    if (0 != access(devicePath, F_OK)) {
        //ALOGE("%s could not access %s, %s\n", __FUNCTION__, devicePath, strerror(errno));
        goto done;
    }

    fd = open(devicePath, O_RDWR|O_NONBLOCK);
    if (fd <= 0) {
        //ALOGE("%s could not open %s, %s\n", __FUNCTION__, devicePath, strerror(errno));
        goto done;
    }
    memset(&devInfo, 0, sizeof(struct hidraw_devinfo));
    if (ioctl(fd, HIDIOCGRAWINFO, &devInfo)) {
        goto done; 
    }

    ALOGD("%s info.bustype:0x%x, info.vendor:0x%x, info.product:0x%x", 
        __FUNCTION__, devInfo.bustype, devInfo.vendor, devInfo.product);

    /*please define array for differnt vid&pid with the same rc platform*/
    if (devInfo.bustype == BUS_BLUETOOTH) {
        //check vendor id & product id
        if ((devInfo.vendor & 0XFFFF) == speech_TI_VID &&
            (devInfo.product & 0XFFFF) == speech_TI_PID) {
            speech_rc_platform = RC_PLATFORM_TI;
        }
        else if ((devInfo.vendor & 0XFFFF) == SPEECH_BCM_VID &&
            ((devInfo.product & 0XFFFF) == SPEECH_BCM_PID_20734 ||
            (devInfo.product & 0XFFFF) == SPEECH_BCM_PID_20735)) {
            speech_rc_platform = RC_PLATFORM_BCM;
        }
        else if ((devInfo.vendor & 0XFFFF) == SPEECH_DIALOG_VID &&
            (devInfo.product & 0XFFFF) == SPEECH_DIALOG_PID) {
            speech_rc_platform = RC_PLATFORM_DIALOG;
        }
        else if ((devInfo.vendor & 0XFFFF) == SPEECH_NORDIC_VID &&
            (devInfo.product & 0XFFFF) == SPEECH_NORDIC_PID) {
            speech_rc_platform = RC_PLATFORM_NORDIC;
        }
        else {
            speech_rc_platform = RC_PLATFORM_UNKOWN;
        }        
    }

done:
    if (fd > 0) {
        hidraw_fd = fd;
        part_index = 0;
        memset (ADPCM_Data_Frame, 0, sizeof (ADPCM_Data_Frame) ); //for ti rc        

        if (!st) {
            st = opus_decoder_create(16000, 1, &error);
            Reset_BV32_Decoder(&bv32_st);
        }
    }

    return fd;
}

void speech_in_close_stream() {
    ALOGD("%s", __FUNCTION__);
    if (hidraw_fd > 0) {
        close(hidraw_fd);
        hidraw_fd = -1;

        if (st) {
            opus_decoder_destroy(st);
            st = NULL;
        }
    }

    speech_rc_platform = RC_PLATFORM_UNKOWN;
}
