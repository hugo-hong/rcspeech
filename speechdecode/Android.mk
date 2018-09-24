# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

	LOCAL_PATH := $(call my-dir)

# The default audio HAL module, which is a stub, that is loaded if no other
# device specific modules are present. The exact load order can be seen in
# libhardware/hardware.c
#
# The format of the name is audio.<type>.<hardware/etc>.so where the only
# required type is 'primary'. Other possibilites are 'a2dp', 'usb', etc.


include $(CLEAR_VARS)

LOCAL_MODULE := audio.bt.remote
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := \
                huitong_audio.c \
                ti/RAS_lib.c \
                ti/ti_audio.c \
                bcm/sbc.c \
                bcm/mainSBC.c \
                nordic/opus-1.1.3/bands.c \
                nordic/opus-1.1.3/bwexpander.c \
                nordic/opus-1.1.3/bwexpander_32.c \
                nordic/opus-1.1.3/celt.c \
                nordic/opus-1.1.3/celt_decoder.c \
                nordic/opus-1.1.3/celt_lpc.c \
                nordic/opus-1.1.3/CNG.c \
                nordic/opus-1.1.3/code_signs.c \
                nordic/opus-1.1.3/cwrs.c \
                nordic/opus-1.1.3/dec_API.c \
                nordic/opus-1.1.3/decode_core.c \
                nordic/opus-1.1.3/decode_frame.c \
                nordic/opus-1.1.3/decode_indices.c \
                nordic/opus-1.1.3/decode_parameters.c \
                nordic/opus-1.1.3/decode_pitch.c \
                nordic/opus-1.1.3/decode_pulses.c \
                nordic/opus-1.1.3/decoder_set_fs.c \
                nordic/opus-1.1.3/entcode.c \
                nordic/opus-1.1.3/entdec.c \
                nordic/opus-1.1.3/entenc.c \
                nordic/opus-1.1.3/gain_quant.c \
                nordic/opus-1.1.3/init_decoder.c \
                nordic/opus-1.1.3/kiss_fft.c \
                nordic/opus-1.1.3/laplace.c \
                nordic/opus-1.1.3/lin2log.c \
                nordic/opus-1.1.3/log2lin.c \
                nordic/opus-1.1.3/LPC_analysis_filter.c \
                nordic/opus-1.1.3/LPC_inv_pred_gain.c \
                nordic/opus-1.1.3/mathops.c \
                nordic/opus-1.1.3/mdct.c \
                nordic/opus-1.1.3/modes.c \
                nordic/opus-1.1.3/NLSF_decode.c \
                nordic/opus-1.1.3/NLSF_stabilize.c \
                nordic/opus-1.1.3/NLSF_unpack.c \
                nordic/opus-1.1.3/NLSF_VQ_weights_laroia.c \
                nordic/opus-1.1.3/NLSF2A.c \
                nordic/opus-1.1.3/opus.c \
                nordic/opus-1.1.3/opus_decoder.c \
                nordic/opus-1.1.3/pitch.c \
                nordic/opus-1.1.3/pitch_est_tables.c \
                nordic/opus-1.1.3/PLC.c \
                nordic/opus-1.1.3/quant_bands.c \
                nordic/opus-1.1.3/rate.c \
                nordic/opus-1.1.3/resampler.c \
                nordic/opus-1.1.3/resampler_private_AR2.c \
                nordic/opus-1.1.3/resampler_private_down_FIR.c \
                nordic/opus-1.1.3/resampler_private_IIR_FIR.c \
                nordic/opus-1.1.3/resampler_private_up2_HQ.c \
                nordic/opus-1.1.3/resampler_rom.c \
                nordic/opus-1.1.3/shell_coder.c \
                nordic/opus-1.1.3/sort.c \
                nordic/opus-1.1.3/stereo_decode_pred.c \
                nordic/opus-1.1.3/stereo_MS_to_LR.c \
                nordic/opus-1.1.3/sum_sqr_shift.c \
                nordic/opus-1.1.3/table_LSF_cos.c \
                nordic/opus-1.1.3/tables_gain.c \
                nordic/opus-1.1.3/tables_LTP.c \
                nordic/opus-1.1.3/tables_NLSF_CB_NB_MB.c \
                nordic/opus-1.1.3/tables_NLSF_CB_WB.c \
                nordic/opus-1.1.3/tables_other.c \
                nordic/opus-1.1.3/tables_pitch_lag.c \
                nordic/opus-1.1.3/tables_pulses_per_block.c \
                nordic/opus-1.1.3/vq.c \
                nordic/bv32fp-1.2/a2lsp.c \
                nordic/bv32fp-1.2/allpole.c \
                nordic/bv32fp-1.2/allzero.c \
                nordic/bv32fp-1.2/autocor.c \
                nordic/bv32fp-1.2/bitpack.c \
                nordic/bv32fp-1.2/bvplc.c \
                nordic/bv32fp-1.2/cmtables.c \
                nordic/bv32fp-1.2/coarptch.c \
                nordic/bv32fp-1.2/decoder.c \
                nordic/bv32fp-1.2/excdec.c \
                nordic/bv32fp-1.2/excquan.c \
                nordic/bv32fp-1.2/fineptch.c \
                nordic/bv32fp-1.2/gaindec.c \
                nordic/bv32fp-1.2/gainquan.c \
                nordic/bv32fp-1.2/levdur.c \
                nordic/bv32fp-1.2/levelest.c \
                nordic/bv32fp-1.2/lsp2a.c \
                nordic/bv32fp-1.2/lspdec.c \
                nordic/bv32fp-1.2/lspquan.c \
                nordic/bv32fp-1.2/ptdec.c \
                nordic/bv32fp-1.2/ptquan.c \
                nordic/bv32fp-1.2/stblchck.c \
                nordic/bv32fp-1.2/stblzlsp.c \
                nordic/bv32fp-1.2/tables.c \
                nordic/bv32fp-1.2/utility.c \
                nordic/adpcm/dvi_adpcm.c \
                log/huitong_log.c

LOCAL_C_INCLUDES += \
        external/tinyalsa/include \
        ti \
        bcm \
        nordic/opus-1.1.3 \
        nordic/adpcm \
        nordic/bv32fp-1.2 \
        log
        
LOCAL_CFLAGS += -DUSE_ALLOCA
LOCAL_CFLAGS += -DOPUSDECODE_EXPORTS
LOCAL_CFLAGS += -DOPUS_BUILD
LOCAL_SHARED_LIBRARIES := liblog libcutils libaudioutils libutils
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

include $(BUILD_STATIC_LIBRARY)

LOCAL_LDFLAGS += Wl,-hash-style=sysv
libsysv-hash-table-library_ldflags := Wl,-hash-style=sysv

include $(call all-makefiles-under,$(LOCAL_PATH))
