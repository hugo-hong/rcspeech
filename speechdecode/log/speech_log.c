#include <stdio.h>
#include <cutils/log.h>

static const char *outFile_wav_3 = "/data/misc/audio/ht_mic_3.wav";
static const char *outFile_wav_2 = "/data/misc/audio/ht_mic_2.wav";
static const char *outFile_wav_1 = "/data/misc/audio/ht_mic_1.wav";
static const char *outFile_wav = "/data/misc/audio/ht_mic_0.wav";

static FILE * fOutput = NULL;
static unsigned long ulDataSize = 0;

void log_begin()
{
    ulDataSize = 0;
    fOutput = fopen(outFile_wav, "w");
    if (fOutput == NULL) {
        ALOGE("%s open outFile_wav failed!", __FUNCTION__);
    } else {
        fseek(fOutput, 0x2C, SEEK_SET);//for head to use
    }
}

void log_write(unsigned char *buf, int len)
{
    if (fOutput) {
        fwrite(buf, 1, len, fOutput);
        fflush(fOutput);
    }
    ulDataSize += len;
}

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

void log_end()
{
    if (fOutput != NULL) {
        if (ulDataSize > 0) {
            unsigned long ulFileSize = 0;
            unsigned char wavhadtab[]={                    // Little endian
                0x52,0x49,0x46,0x46,                        // "RIFF"
                0xac,0xae,0x28,0x00,                        // File Total Size - 8
                0x57,0x41,0x56,0x45,0x66,0x6d,0x74,0x20,    // Wave_fmt
                0x10,0x00,0x00,0x00,                        // PCM
                0x01,0x00,                                    // fmttag
                0x01,0x00,                                    // channel number = 1
                0x80,0x3E,0x00,0x00,                        // Sample Rate = 16000
                0x00,0x7D,0x00,0x00,                        // Sample Bytes Per Second = 32000
                0x02,0x00,                                    // Play Bytes Per Second = channel number * bits per sample / 8
                0x10,0x00,                                    // bits per sample = 16
                0x64,0x61,0x74,0x61,                        // "data"
                0x88,0xae,0x28,0x00                            // Data Total Size = Sample Bytes Per Second * Play Duration
                                                            //                       = File Total Size - 44
                };

             ulFileSize = ulDataSize + 44;
             printf("\r\n ulDataSize = %lu, ulFileSize = %lu",ulDataSize, ulFileSize);
             wavhadtab[0x04] = (ulFileSize-8) & 0xff;
             wavhadtab[0x05] = ((ulFileSize-8) >> 8) & 0xff;
             wavhadtab[0x06] = ((ulFileSize-8) >> 16) & 0xff;
             wavhadtab[0x07] = ((ulFileSize-8) >> 24) & 0xff;        

             wavhadtab[0x28] = ulDataSize &0xff;
             wavhadtab[0x29] = (ulDataSize>>8) &0xff;
             wavhadtab[0x2a] = (ulDataSize>>16) &0xff;
             wavhadtab[0x2b] = (ulDataSize>>24) &0xff;
                
             fseek(fOutput, 0, SEEK_SET);
             fwrite(wavhadtab,0x2C,1,fOutput);
             fflush(fOutput);
             fclose(fOutput);
             fOutput = NULL;
            adjustAudioRecordFileName();
        } else {
            fclose(fOutput);
            fOutput = NULL;
        }
    }

    ulDataSize = 0;
}
