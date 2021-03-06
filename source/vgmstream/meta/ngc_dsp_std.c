#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* If these variables are packed properly in the struct (one after another)
 * then this is actually how they are laid out in the file, albeit big-endian */
struct dsp_header {
    uint32_t sample_count;
    uint32_t nibble_count;
    uint32_t sample_rate;
    uint16_t loop_flag;
    uint16_t format;
    uint32_t loop_start_offset;
    uint32_t loop_end_offset;
    uint32_t ca;
    int16_t coef[16]; /* really 8x2 */
    uint16_t gain;
    uint16_t initial_ps;
    int16_t initial_hist1;
    int16_t initial_hist2;
    uint16_t loop_ps;
    int16_t loop_hist1;
    int16_t loop_hist2;
    int16_t channel_count; /* DSPADPCM.exe ~v2.7 extension */
    int16_t block_size;
    /* padding/reserved up to 0x60 */
    /* DSPADPCM.exe from GC adds some extra data here (uninitialized MSVC memory?) */
};

/* read the above struct; returns nonzero on failure */
static int read_dsp_header_endian(struct dsp_header *header, off_t offset, STREAMFILE *streamFile, int big_endian) {
    int32_t (*get_32bit)(uint8_t *) = big_endian ? get_32bitBE : get_32bitLE;
    int16_t (*get_16bit)(uint8_t *) = big_endian ? get_16bitBE : get_16bitLE;
    int i;
    uint8_t buf[0x4e];

    if (offset > get_streamfile_size(streamFile))
        return 1;
    if (read_streamfile(buf, offset, 0x4e, streamFile) != 0x4e)
        return 1;
    header->sample_count =      get_32bit(buf+0x00);
    header->nibble_count =      get_32bit(buf+0x04);
    header->sample_rate =       get_32bit(buf+0x08);
    header->loop_flag =         get_16bit(buf+0x0c);
    header->format =            get_16bit(buf+0x0e);
    header->loop_start_offset = get_32bit(buf+0x10);
    header->loop_end_offset =   get_32bit(buf+0x14);
    header->ca =                get_32bit(buf+0x18);
    for (i=0; i < 16; i++)
        header->coef[i] =       get_16bit(buf+0x1c+i*0x02);
    header->gain =              get_16bit(buf+0x3c);
    header->initial_ps =        get_16bit(buf+0x3e);
    header->initial_hist1 =     get_16bit(buf+0x40);
    header->initial_hist2 =     get_16bit(buf+0x42);
    header->loop_ps =           get_16bit(buf+0x44);
    header->loop_hist1 =        get_16bit(buf+0x46);
    header->loop_hist2 =        get_16bit(buf+0x48);
    header->channel_count =     get_16bit(buf+0x4a);
    header->block_size =        get_16bit(buf+0x4c);
    return 0;
}
static int read_dsp_header(struct dsp_header *header, off_t offset, STREAMFILE *file) {
    return read_dsp_header_endian(header, offset, file, 1);
}
static int read_dsp_header_le(struct dsp_header *header, off_t offset, STREAMFILE *file) {
    return read_dsp_header_endian(header, offset, file, 0);
}

/* ********************************* */

typedef struct {
    /* basic config */
    int little_endian;
    int channel_count;
    int max_channels;

    off_t header_offset;            /* standard DSP header */
    size_t header_spacing;          /* distance between DSP header of other channels */
    off_t start_offset;             /* data start */
    size_t interleave;              /* distance between data of other channels */
    size_t interleave_first;        /* same, in the first block */
    size_t interleave_first_skip;   /* extra info */
    size_t interleave_last;         /* same, in the last block */

    meta_t meta_type;

    /* hacks */
    int force_loop;                 /* force full loop */
    int force_loop_seconds;         /* force loop, but must be longer than this (to catch jingles) */
    int fix_looping;                /* fix loop end going past num_samples */
    int fix_loop_start;             /* weird files with bad loop start */
    int single_header;              /* all channels share header, thus totals are off */
    int ignore_header_agreement;    /* sometimes there are minor differences between headers */
} dsp_meta;

#define COMMON_DSP_MAX_CHANNELS 6

/* Common parser for most DSPs that are basically the same with minor changes.
 * Custom variants will just concatenate or interleave standard DSP headers and data,
 * so we make sure to validate read vs expected values, based on dsp_meta config. */
static VGMSTREAM * init_vgmstream_dsp_common(STREAMFILE *streamFile, dsp_meta *dspm) {
    VGMSTREAM * vgmstream = NULL;
    int i, j;
    int loop_flag;
    struct dsp_header ch_header[COMMON_DSP_MAX_CHANNELS];


    if (dspm->channel_count > dspm->max_channels)
        goto fail;
    if (dspm->channel_count > COMMON_DSP_MAX_CHANNELS)
        goto fail;

    /* load standard DSP header per channel */
    {
        for (i = 0; i < dspm->channel_count; i++) {
            if (read_dsp_header_endian(&ch_header[i], dspm->header_offset + i*dspm->header_spacing, streamFile, !dspm->little_endian))
                goto fail;
        }
    }

    /* fix bad/fixed value in loop start */
    if (dspm->fix_loop_start) {
        for (i = 0; i < dspm->channel_count; i++) {
            if (ch_header[i].loop_flag)
                ch_header[i].loop_start_offset = 0x00;
        }
    }

    /* check type==0 and gain==0 */
    {
        for (i = 0; i < dspm->channel_count; i++) {
            if (ch_header[i].format || ch_header[i].gain)
                goto fail;
        }
    }

    /* check for agreement between channels */
    if (!dspm->ignore_header_agreement) {
        for (i = 0; i < dspm->channel_count - 1; i++) {
            if (ch_header[i].sample_count != ch_header[i+1].sample_count ||
                ch_header[i].nibble_count != ch_header[i+1].nibble_count ||
                ch_header[i].sample_rate != ch_header[i+1].sample_rate ||
                ch_header[i].loop_flag != ch_header[i+1].loop_flag ||
                ch_header[i].loop_start_offset != ch_header[i+1].loop_start_offset ||
                ch_header[i].loop_end_offset != ch_header[i+1].loop_end_offset) {
                goto fail;
            }
        }
    }

    /* check expected initial predictor/scale */
    {
        int channels = dspm->channel_count;
        if (dspm->single_header)
            channels = 1;

        for (i = 0; i < channels; i++) {
            off_t channel_offset = dspm->start_offset + i*dspm->interleave;
            if (ch_header[i].initial_ps != (uint8_t)read_8bit(channel_offset, streamFile))
                goto fail;
        }
    }

    /* check expected loop predictor/scale */
    if (ch_header[0].loop_flag) {
        int channels = dspm->channel_count;
        if (dspm->single_header)
            channels = 1;

        for (i = 0; i < channels; i++) {
            off_t loop_offset = ch_header[i].loop_start_offset;
            if (dspm->interleave) {
                loop_offset = loop_offset / 16 * 8;
                loop_offset = (loop_offset / dspm->interleave * dspm->interleave * channels) + (loop_offset % dspm->interleave);
            }

            if (ch_header[i].loop_ps != (uint8_t)read_8bit(dspm->start_offset + i*dspm->interleave + loop_offset,streamFile))
                goto fail;
        }
    }


    /* all done, must be DSP */

    loop_flag = ch_header[0].loop_flag;
    if (!loop_flag && dspm->force_loop) {
        loop_flag = 1;
        if (dspm->force_loop_seconds &&
                ch_header[0].sample_count < dspm->force_loop_seconds*ch_header[0].sample_rate) {
            loop_flag = 0;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(dspm->channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = dspm->meta_type;
    vgmstream->coding_type = coding_NGC_DSP;
    if (dspm->interleave > 0 && dspm->interleave < 0x08)
        vgmstream->coding_type = coding_NGC_DSP_subint;
    vgmstream->layout_type = layout_interleave;
    if (dspm->interleave == 0 || vgmstream->coding_type == coding_NGC_DSP_subint)
        vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = dspm->interleave;
    vgmstream->interleave_first_block_size = dspm->interleave_first;
    vgmstream->interleave_first_skip = dspm->interleave_first_skip;
    vgmstream->interleave_last_block_size = dspm->interleave_last;

    {
        /* set coefs and initial history (usually 0) */
        for (i = 0; i < vgmstream->channels; i++) {
            for (j = 0; j < 16; j++) {
                vgmstream->ch[i].adpcm_coef[j] = ch_header[i].coef[j];
            }
            vgmstream->ch[i].adpcm_history1_16 = ch_header[i].initial_hist1;
            vgmstream->ch[i].adpcm_history2_16 = ch_header[i].initial_hist2;
        }
    }

    /* don't know why, but it does happen*/
    if (dspm->fix_looping && vgmstream->loop_end_sample > vgmstream->num_samples)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    if (dspm->single_header == 2) { /* double the samples */
        vgmstream->num_samples /= dspm->channel_count;
        vgmstream->loop_start_sample /= dspm->channel_count;
        vgmstream->loop_end_sample /= dspm->channel_count;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,dspm->start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ********************************* */

/* .dsp - standard dsp as generated by DSPADPCM.exe */
VGMSTREAM * init_vgmstream_ngc_dsp_std(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    struct dsp_header header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int i, channel_count;

    /* checks */
    /* .dsp: standard
     * .adp: Dr. Muto/Battalion Wars (GC) mono files
     * (extensionless): Tony Hawk's Downhill Jam (Wii) */
    if (!check_extensions(streamFile, "dsp,adp,"))
        goto fail;

    if (read_dsp_header(&header, 0x00, streamFile))
        goto fail;

    channel_count = 1;
    start_offset = header_size;

    if (header.initial_ps != (uint8_t)read_8bit(start_offset,streamFile))
        goto fail; /* check initial predictor/scale */
    if (header.format || header.gain)
        goto fail; /* check type==0 and gain==0 */

    /* Check for a matching second header. If we find one and it checks
     * out thoroughly, we're probably not dealing with a genuine mono DSP.
     * In many cases these will pass all the other checks, including the
     * predictor/scale check if the first byte is 0 */
    //todo maybe this meta should be after others, so they have a chance to detect >1ch .dsp
    {
        int ko;
        struct dsp_header header2;

        /* ignore headers one after another */
        ko = read_dsp_header(&header2, header_size, streamFile);
        if (!ko &&
                header.sample_count == header2.sample_count &&
                header.nibble_count == header2.nibble_count &&
                header.sample_rate == header2.sample_rate &&
                header.loop_flag == header2.loop_flag) {
            goto fail;
        }


        /* ignore headers after interleave [Ultimate Board Collection (Wii)] */
        ko = read_dsp_header(&header2, 0x10000, streamFile);
        if (!ko &&
                header.sample_count == header2.sample_count &&
                header.nibble_count == header2.nibble_count &&
                header.sample_rate == header2.sample_rate &&
                header.loop_flag == header2.loop_flag) {
            goto fail;
        }
    }
        
    if (header.loop_flag) {
        off_t loop_off;
        /* check loop predictor/scale */
        loop_off = header.loop_start_offset/16*8;
        if (header.loop_ps != (uint8_t)read_8bit(start_offset+loop_off,streamFile)) {
            /* rarely won't match (ex ESPN 2002), not sure if header or calc problem, but doesn't seem to matter
             *  (there may be a "click" when looping, or loop values may be too big and loop disabled anyway) */
            VGM_LOG("DSP (std): bad loop_predictor\n");
            //header.loop_flag = 0;
            //goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(header.loop_end_offset)+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->allow_dual_stereo = 1; /* very common in .dsp */
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;

    {
        /* adpcm coeffs/history */
        for (i = 0; i < 16; i++)
            vgmstream->ch[0].adpcm_coef[i] = header.coef[i];
        vgmstream->ch[0].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[0].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .dsp - little endian dsp, possibly main Switch .dsp [LEGO Worlds (Switch)] */
VGMSTREAM * init_vgmstream_ngc_dsp_std_le(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    struct dsp_header header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int i, channel_count;

    /* checks */
    /* .adpcm: LEGO Worlds */
    if (!check_extensions(streamFile, "adpcm"))
        goto fail;

    if (read_dsp_header_le(&header, 0x00, streamFile))
        goto fail;

    channel_count = 1;
    start_offset = header_size;

    if (header.initial_ps != (uint8_t)read_8bit(start_offset,streamFile))
        goto fail; /* check initial predictor/scale */
    if (header.format || header.gain)
        goto fail; /* check type==0 and gain==0 */

    /* Check for a matching second header. If we find one and it checks
     * out thoroughly, we're probably not dealing with a genuine mono DSP.
     * In many cases these will pass all the other checks, including the
     * predictor/scale check if the first byte is 0 */
    {
        struct dsp_header header2;
        read_dsp_header_le(&header2, header_size, streamFile);

        if (header.sample_count == header2.sample_count &&
            header.nibble_count == header2.nibble_count &&
            header.sample_rate == header2.sample_rate &&
            header.loop_flag == header2.loop_flag) {
            goto fail;
        }
    }

    if (header.loop_flag) {
        off_t loop_off;
        /* check loop predictor/scale */
        loop_off = header.loop_start_offset/16*8;
        if (header.loop_ps != (uint8_t)read_8bit(start_offset+loop_off,streamFile)) {
            /* rarely won't match (ex ESPN 2002), not sure if header or calc problem, but doesn't seem to matter
             *  (there may be a "click" when looping, or loop values may be too big and loop disabled anyway) */
            VGM_LOG("DSP (std): bad loop_predictor\n");
            //header.loop_flag = 0;
            //goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(header.loop_end_offset)+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->allow_dual_stereo = 1;

    {
        /* adpcm coeffs/history */
        for (i = 0; i < 16; i++)
            vgmstream->ch[0].adpcm_coef[i] = header.coef[i];
        vgmstream->ch[0].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[0].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .dsp - standard multi-channel dsp as generated by DSPADPCM.exe (later revisions) */
VGMSTREAM * init_vgmstream_ngc_mdsp_std(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    struct dsp_header header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int i, c, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "dsp,mdsp"))
        goto fail;

    if (read_dsp_header(&header, 0x00, streamFile))
        goto fail;

    channel_count = header.channel_count==0 ? 1 : header.channel_count;
    start_offset = header_size * channel_count;

    /* named .dsp and no channels? likely another interleaved dsp */
    if (check_extensions(streamFile,"dsp") && header.channel_count == 0)
        goto fail;

    if (header.initial_ps != (uint8_t)read_8bit(start_offset, streamFile))
        goto fail; /* check initial predictor/scale */
    if (header.format || header.gain)
        goto fail; /* check type==0 and gain==0 */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample = dsp_nibbles_to_samples(header.loop_end_offset) + 1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen*/
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = header.block_size * 8;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (header.nibble_count / 2 % vgmstream->interleave_block_size + 7) / 8 * 8;

    for (i = 0; i < channel_count; i++) {
        if (read_dsp_header(&header, header_size * i, streamFile)) goto fail;

        /* adpcm coeffs/history */
        for (c = 0; c < 16; c++)
            vgmstream->ch[i].adpcm_coef[c] = header.coef[c];
        vgmstream->ch[i].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[i].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ********************************* */

/* .stm - Intelligent Systems + others (same programmers) full interleaved dsp [Paper Mario TTYD (GC), Fire Emblem: POR (GC), Cubivore (GC)] */
VGMSTREAM * init_vgmstream_ngc_dsp_stm(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .lstm/dsp: renamed to avoid hijacking Scream Tracker 2 Modules */
    if (!check_extensions(streamFile, "stm,lstm,dsp"))
        goto fail;
    if (read_16bitBE(0x00, streamFile) != 0x0200)
        goto fail;
    /* 0x02(2): sample rate, 0x08+: channel sizes/loop offsets? */

    dspm.channel_count = read_32bitBE(0x04, streamFile);
    dspm.max_channels = 2;
    dspm.fix_looping = 1;

    dspm.header_offset =  0x40;
    dspm.header_spacing = 0x60;
    dspm.start_offset = 0x100;
    dspm.interleave = (read_32bitBE(0x08, streamFile) + 0x20) / 0x20 * 0x20; /* strange rounding, but works */

    dspm.meta_type = meta_DSP_STM;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .(mp)dsp - single header + interleaved dsp [Monopoly Party! (GC)] */
VGMSTREAM * init_vgmstream_ngc_mpdsp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .mpdsp: renamed since standard .dsp would catch it otherwise */
    if (!check_extensions(streamFile, "mpdsp"))
        goto fail;

    /* at 0x48 is extra data that could help differenciating these DSPs, but seems like
     * memory garbage created by the encoder that other games also have */
    /* 0x02(2): sample rate, 0x08+: channel sizes/loop offsets? */

    dspm.channel_count = 2;
    dspm.max_channels = 2;
    dspm.single_header = 2;

    dspm.header_offset =  0x00;
    dspm.header_spacing = 0x00; /* same header for both channels */
    dspm.start_offset = 0x60;
    dspm.interleave = 0xf000;

    dspm.meta_type = meta_DSP_MPDSP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* various dsp with differing extensions and interleave values */
VGMSTREAM * init_vgmstream_ngc_dsp_std_int(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};
    char filename[PATH_LIMIT];

    /* checks */
    if (!check_extensions(streamFile, "dsp,mss,gcm"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;
    dspm.fix_looping = 1;

    dspm.header_offset  = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = 0xc0;

    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strlen(filename) > 7 && !strcasecmp("_lr.dsp",filename+strlen(filename)-7)) { //todo improve
        dspm.interleave = 0x14180;
        dspm.meta_type = meta_DSP_JETTERS; /* Bomberman Jetters (GC) */
    } else if (check_extensions(streamFile, "mss")) {
        dspm.interleave = 0x1000;
        dspm.meta_type = meta_DSP_MSS; /* Free Radical GC games */
        /* Timesplitters 2 GC's ts2_atom_smasher_44_fx.mss differs slightly in samples but plays ok */
        dspm.ignore_header_agreement = 1;
    } else if (check_extensions(streamFile, "gcm")) {
        /* older Traveller's Tales games [Lego Star Wars (GC), The Chronicles of Narnia (GC), Sonic R (GC)] */
        dspm.interleave = 0x8000;
        dspm.meta_type = meta_DSP_GCM;
    } else {
        goto fail;
    }

    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* IDSP - Namco header (from NUS3) + interleaved dsp [SSB4 (3DS), Tekken Tag Tournament 2 (WiiU)] */
VGMSTREAM * init_vgmstream_idsp_nus3(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "idsp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x49445350) /* "IDSP" */
        goto fail;
    /* 0x0c: sample rate, 0x10: num_samples, 0x14: loop_start_sample, 0x18: loop_start_sample */

    dspm.channel_count = read_32bitBE(0x08, streamFile);
    dspm.max_channels = 8;
    /* games do adjust loop_end if bigger than num_samples (only happens in user-created IDSPs) */
    dspm.fix_looping = 1;

    dspm.header_offset = read_32bitBE(0x20,streamFile);
    dspm.header_spacing = read_32bitBE(0x24,streamFile);
    dspm.start_offset = read_32bitBE(0x28,streamFile);
    dspm.interleave = read_32bitBE(0x1c,streamFile); /* usually 0x10 */
    if (dspm.interleave == 0) /* Taiko no Tatsujin: Atsumete Tomodachi Daisakusen (WiiU) */
        dspm.interleave = read_32bitBE(0x2c,streamFile); /* half interleave, use channel size */

    dspm.meta_type = meta_IDSP_NUS3;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* sadb - Procyon Studio header + interleaved dsp [Shiren the Wanderer 3 (Wii), Disaster: Day of Crisis (Wii)] */
VGMSTREAM * init_vgmstream_sadb(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "sad"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x73616462) /* "sadb" */
        goto fail;

    dspm.channel_count = read_8bit(0x32, streamFile);
    dspm.max_channels = 2;

    dspm.header_offset =  0x80;
    dspm.header_spacing = 0x60;
    dspm.start_offset = read_32bitBE(0x48,streamFile);
    dspm.interleave = 0x10;

    dspm.meta_type = meta_DSP_SADB;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* sadf - Procyon Studio Header Variant [Xenoblade Chronicles 2 (Switch)] (sfx) */
VGMSTREAM * init_vgmstream_sadf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int  channel_count, loop_flag;
    off_t start_offset;

	/* checks */
    if (!check_extensions(streamFile, "sad"))
        goto fail;
    if (read_32bitBE(0x00, streamFile) != 0x73616466) /* "sadf" */
        goto fail;

    channel_count = read_8bit(0x18, streamFile);
    loop_flag = read_8bit(0x19, streamFile);
    start_offset = read_32bitLE(0x1C, streamFile);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = read_32bitLE(0x28, streamFile);
    vgmstream->sample_rate = read_32bitLE(0x24, streamFile);
    if (loop_flag) {
		vgmstream->loop_start_sample = read_32bitLE(0x2c, streamFile);
		vgmstream->loop_end_sample = read_32bitLE(0x30, streamFile);
	 }
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = channel_count == 1 ? 0x8 :
		read_32bitLE(0x20, streamFile) / channel_count;
    vgmstream->meta_type = meta_DSP_SADF;

    dsp_read_coefs_le(vgmstream, streamFile, 0x80, 0x80);

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
		goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* IDSP - Traveller's Tales header + interleaved dsps [Lego Batman (Wii), Lego Dimensions (Wii U)] */
VGMSTREAM * init_vgmstream_idsp_tt(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};
    int version_main, version_sub;

    /* checks */
    /* .gcm: standard
     * .idsp: header id?
     * .wua: Lego Dimensions (Wii U) */
    if (!check_extensions(streamFile, "gcm,idsp,wua"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x49445350) /* "IDSP" */
        goto fail;

    version_main = read_32bitBE(0x04, streamFile);
    version_sub  = read_32bitBE(0x08, streamFile); /* extra check since there are other IDSPs */
    if (version_main == 0x01 && version_sub == 0xc8) {
        /* Transformers: The Game (Wii) */
        dspm.channel_count = 2;
        dspm.max_channels = 2;
        dspm.header_offset = 0x10;
    }
    else if (version_main == 0x02 && version_sub == 0xd2) {
        /* Lego Batman (Wii)
         * The Chronicles of Narnia: Prince Caspian (Wii)
         * Lego Indiana Jones 2 (Wii)
         * Lego Star Wars: The Complete Saga (Wii)
         * Lego Pirates of the Caribbean (Wii)
         * Lego Harry Potter: Years 1-4 (Wii) */
        dspm.channel_count = 2;
        dspm.max_channels = 2;
        dspm.header_offset = 0x20;
        /* 0x10+: null */
    }
    else if (version_main == 0x03 && version_sub == 0x12c) {
        /* Lego The Lord of the Rings (Wii) */
        /* Lego Dimensions (Wii U) */
        dspm.channel_count = read_32bitBE(0x10, streamFile);
        dspm.max_channels = 2;
        dspm.header_offset = 0x20;
        /* 0x14+: "I_AM_PADDING" */
    }
    else {
        goto fail;
    }

    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + 0x60 * dspm.channel_count;
    dspm.interleave = read_32bitBE(0x0c, streamFile);

    dspm.meta_type = meta_IDSP_TT;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* IDSP - from Next Level games [Super Mario Strikers (GC), Mario Strikers: Charged (Wii)] */
VGMSTREAM * init_vgmstream_idsp_nl(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "idsp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x49445350) /* "IDSP" */
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset =  0x0c;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing*dspm.channel_count;
    dspm.interleave = read_32bitBE(0x04,streamFile);
    /* 0x08: usable channel size */
    {
        size_t stream_size = get_streamfile_size(streamFile);
        if (read_32bitBE(stream_size - 0x04,streamFile) == 0x30303030)
            stream_size -= 0x14; /* remove padding */
        stream_size -= dspm.start_offset;

        if (dspm.interleave)
            dspm.interleave_last = (stream_size / dspm.channel_count) % dspm.interleave;
    }

    dspm.fix_looping = 1;
    dspm.force_loop = 1;
    dspm.force_loop_seconds = 15;

    dspm.meta_type = meta_IDSP_NL;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .wsd - Custom header + full interleaved dsp [Phantom Brave (Wii)] */
VGMSTREAM * init_vgmstream_wii_wsd(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "wsd"))
        goto fail;
    if (read_32bitBE(0x08,streamFile) != read_32bitBE(0x0c,streamFile)) /* channel sizes */
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset =  read_32bitBE(0x00,streamFile);
    dspm.header_spacing = read_32bitBE(0x04,streamFile) - dspm.header_offset;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_WII_WSD;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .ddsp - full interleaved dsp [The Sims 2 - Pets (Wii)] */
VGMSTREAM * init_vgmstream_dsp_ddsp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "ddsp"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x00;
    dspm.header_spacing = (get_streamfile_size(streamFile) / dspm.channel_count);
    dspm.start_offset = 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_DDSP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* iSWS - Sumo Digital header + interleaved dsp [DiRT 2 (Wii), F1 2009 (Wii)] */
VGMSTREAM * init_vgmstream_wii_was(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "was,dsp,isws"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x69535753) /* "iSWS" */
        goto fail;

    dspm.channel_count = read_32bitBE(0x08,streamFile);
    dspm.max_channels = 2;

    dspm.header_offset = 0x08 + read_32bitBE(0x04,streamFile);
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channel_count*dspm.header_spacing;
    dspm.interleave = read_32bitBE(0x10,streamFile);

    dspm.meta_type = meta_WII_WAS;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .str - Infogrames raw interleaved dsp [Micro Machines (GC), Superman: Shadow of Apokolips (GC)] */
VGMSTREAM * init_vgmstream_dsp_str_ig(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "str"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x80;
    dspm.start_offset = 0x800;
    dspm.interleave = 0x4000;
    
    dspm.meta_type = meta_DSP_STR_IG;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .dsp - Ubisoft interleaved dsp with bad loop start [Speed Challenge: Jacques Villeneuve's Racing Vision (GC), XIII (GC)] */
VGMSTREAM * init_vgmstream_dsp_xiii(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;
    dspm.fix_loop_start = 1; /* loop flag but strange loop start instead of 0 (maybe shouldn't loop) */

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing * dspm.channel_count;
    dspm.interleave = 0x08;

    dspm.meta_type = meta_DSP_XIII;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* NPD - Icon Games header + subinterleaved DSPs [Vertigo (Wii), Build n' Race (Wii)] */
VGMSTREAM * init_vgmstream_wii_ndp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "ndp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4E445000) /* "NDP\0" */
        goto fail;
    if (read_32bitLE(0x08,streamFile) + 0x18 != get_streamfile_size(streamFile))
        goto fail;
    /* 0x0c: sample rate */

    dspm.channel_count = read_32bitLE(0x10,streamFile);
    dspm.max_channels = 2;

    dspm.header_offset = 0x18;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channel_count*dspm.header_spacing;
    dspm.interleave = 0x04;

    dspm.meta_type = meta_WII_NDP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* Cabela's series (Magic Wand dev?) - header + interleaved dsp
 *  [Cabela's Big Game Hunt 2005 Adventures (GC), Cabela's Outdoor Adventures (GC)] */
VGMSTREAM * init_vgmstream_dsp_cabelas(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;
    /* has extra stuff in the reserved data, without it this meta may catch other DSPs it shouldn't */
    if (read_32bitBE(0x50,streamFile) == 0 || read_32bitBE(0x54,streamFile) == 0)
        goto fail;

    /* sfx are mono, but standard dsp will catch them tho */
    dspm.channel_count = read_32bitBE(0x00,streamFile) == read_32bitBE(0x60,streamFile) ? 2 : 1;
    dspm.max_channels = 2;
    dspm.force_loop = (dspm.channel_count > 1);

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channel_count*dspm.header_spacing;
    dspm.interleave = 0x10;

    dspm.meta_type = meta_DSP_CABELAS;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* AAAp - Acclaim Austin Audio header + interleaved dsp [Vexx (GC), Turok: Evolution (GC)] */
VGMSTREAM * init_vgmstream_ngc_dsp_aaap(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41414170) /* "AAAp" */
        goto fail;

    dspm.channel_count = read_16bitBE(0x06,streamFile);
    dspm.max_channels = 2;

    dspm.header_offset = 0x08;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channel_count*dspm.header_spacing;
    dspm.interleave = (uint16_t)read_16bitBE(0x04,streamFile);

    dspm.meta_type = meta_NGC_DSP_AAAP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* DSPW - Capcom header + full interleaved DSP [Sengoku Basara 3 (Wii), Monster Hunter 3 Ultimate (WiiU)] */
VGMSTREAM * init_vgmstream_dsp_dspw(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};
    size_t data_size;

    /* check extension */
    if (!check_extensions(streamFile, "dspw"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x44535057) /* "DSPW" */
        goto fail;

    /* ignore time marker */
    data_size = read_32bitBE(0x08, streamFile);
    if (read_32bitBE(data_size - 0x10, streamFile) == 0x74494D45) /* "tIME" */
        data_size -= 0x10; /* (ignore, 2 ints in YYYYMMDD hhmmss00) */

    /* some files have a mrkr section with multiple loop regions added at the end (variable size) */
    {
        off_t mrkr_offset = data_size - 0x04;
        off_t max_offset = data_size - 0x1000;
        while (mrkr_offset > max_offset) {
            if (read_32bitBE(mrkr_offset, streamFile) != 0x6D726B72) { /* "mrkr" */
                mrkr_offset -= 0x04;
            } else {
                data_size = mrkr_offset;
                break;
            }
        }
    }
    data_size -= 0x20; /* header size */
    /* 0x10: loop start, 0x14: loop end, 0x1c: num_samples */

    dspm.channel_count = read_32bitBE(0x18, streamFile);
    dspm.max_channels = 6; /* 6ch in Monster Hunter 3 Ultimate */

    dspm.header_offset = 0x20;
    dspm.header_spacing = data_size / dspm.channel_count;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = data_size / dspm.channel_count;

    dspm.meta_type = meta_DSP_DSPW;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* iadp - custom header + interleaved dsp [Dr. Muto (GC)] */
VGMSTREAM * init_vgmstream_ngc_dsp_iadp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .adp: actual extension, .iadp: header id */
    if (!check_extensions(streamFile, "adp,iadp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x69616470) /* "iadp" */
        goto fail;

    dspm.channel_count = read_32bitBE(0x04,streamFile);
    dspm.max_channels = 2;

    dspm.header_offset = 0x20;
    dspm.header_spacing = 0x60;
    dspm.start_offset = read_32bitBE(0x1C,streamFile);
    dspm.interleave = read_32bitBE(0x08,streamFile);

    dspm.meta_type = meta_NGC_DSP_IADP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .mcadpcm - Custom header + full interleaved dsp [Skyrim (Switch)] */
VGMSTREAM * init_vgmstream_dsp_mcadpcm(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "mcadpcm"))
        goto fail;
    /* could validate dsp sizes but only for +1ch, check_dsp_samples will do it anyway */
    //if (read_32bitLE(0x08,streamFile) != read_32bitLE(0x10,streamFile))
    //   goto fail;

    dspm.channel_count = read_32bitLE(0x00,streamFile);
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset =  read_32bitLE(0x04,streamFile);
    dspm.header_spacing = dspm.channel_count == 1 ? 0 :
        read_32bitLE(0x0c,streamFile) - dspm.header_offset; /* channel 2 start, only with Nch */
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_MCADPCM;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .switch_audio - UE4 standard LE header + full interleaved dsp [Gal Gun 2 (Switch)] */
VGMSTREAM * init_vgmstream_dsp_switch_audio(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .switch_audio: possibly UE4 class name rather than extension, .dsp: assumed */
    if (!check_extensions(streamFile, "switch_audio,dsp"))
        goto fail;

    /* manual double header test */
    if (read_32bitLE(0x00, streamFile) == read_32bitLE(get_streamfile_size(streamFile) / 2, streamFile))
        dspm.channel_count = 2;
    else
        dspm.channel_count = 1;
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset = 0x00;
    dspm.header_spacing = get_streamfile_size(streamFile) / dspm.channel_count;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_SWITCH_AUDIO;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .vag - Nippon Ichi SPS wrapper [Penny-Punching Princess (Switch), Ys VIII (Switch)] */
VGMSTREAM * init_vgmstream_dsp_sps_n1(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .vag: Penny-Punching Princess (Switch)
     * .nlsd: Ys VIII (Switch) */
    if (!check_extensions(streamFile, "vag,nlsd"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x08000000) /* file type (see other N1 SPS) */
        goto fail;
    if ((uint16_t)read_16bitLE(0x08,streamFile) != read_32bitLE(0x24,streamFile)) /* header has various repeated values */
        goto fail;

    dspm.channel_count = 1;
    dspm.max_channels = 1;
    dspm.little_endian = 1;

    dspm.header_offset = 0x1c;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing*dspm.channel_count;
    dspm.interleave = 0;

    dspm.fix_loop_start = 1;

    dspm.meta_type = meta_DSP_VAG;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .itl - from Chanrinko Hero (GC) */
VGMSTREAM * init_vgmstream_dsp_itl_ch(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "itl"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing*dspm.channel_count;
    dspm.interleave = 0x23C0;

    dspm.fix_looping = 1;

    dspm.meta_type = meta_DSP_ITL;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* ADPY - AQUASTYLE wrapper [Touhou Genso Wanderer -Reloaded- (Switch)] */
VGMSTREAM * init_vgmstream_dsp_adpy(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "adpcmx"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41445059) /* "ADPY" */
        goto fail;

    /* 0x04(2): 1? */
    /* 0x08: some size? */
    /* 0x0c: null */

    dspm.channel_count = read_16bitLE(0x06,streamFile);
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset = 0x10;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing*dspm.channel_count;
    dspm.interleave = 0x08;

    dspm.meta_type = meta_DSP_ADPY;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* ADPX - AQUASTYLE wrapper [Fushigi no Gensokyo: Lotus Labyrinth (Switch)] */
VGMSTREAM * init_vgmstream_dsp_adpx(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "adpcmx"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41445058) /* "ADPX" */
        goto fail;

    /* from 0x04 *6 are probably channel sizes, so max would be 6ch; this assumes 2ch */
    if (read_32bitLE(0x04,streamFile) != read_32bitLE(0x08,streamFile) &&
        read_32bitLE(0x0c,streamFile) != 0)
        goto fail;
    dspm.channel_count = 2;
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset = 0x1c;
    dspm.header_spacing = read_32bitLE(0x04,streamFile);
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_ADPX;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .ds2 - LucasArts wrapper [Star Wars: Bounty Hunter (GC)] */
VGMSTREAM * init_vgmstream_dsp_ds2(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};
    size_t file_size, channel_offset;

    /* checks */
    /* .ds2: real extension, dsp: fake/renamed */
    if (!check_extensions(streamFile, "ds2,dsp"))
        goto fail;
    if (!(read_32bitBE(0x50,streamFile) == 0 &&
          read_32bitBE(0x54,streamFile) == 0 &&
          read_32bitBE(0x58,streamFile) == 0 &&
          read_32bitBE(0x5c,streamFile) != 0))
        goto fail;
    file_size = get_streamfile_size(streamFile);
    channel_offset = read_32bitBE(0x5c,streamFile);  /* absolute offset to 2nd channel */
    if (channel_offset < file_size / 2 || channel_offset > file_size) /* just to make sure */
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;
    dspm.single_header = 1;

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x00;
    dspm.start_offset = 0x60;
    dspm.interleave = channel_offset - dspm.start_offset;

    dspm.meta_type = meta_DSP_DS2;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .itl - Incinerator Studios interleaved dsp [Cars Race-o-rama (Wii), MX vs ATV Untamed (Wii)] */
VGMSTREAM * init_vgmstream_dsp_itl(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};
    size_t stream_size;

    /* checks */
    /* .itl: standard
     * .dsp: default to catch a similar file, not sure which devs */
    if (!check_extensions(streamFile, "itl,dsp"))
        goto fail;

    stream_size = get_streamfile_size(streamFile);
    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.start_offset = 0x60;
    dspm.interleave = 0x10000;
    dspm.interleave_first_skip = dspm.start_offset;
    dspm.interleave_first = dspm.interleave - dspm.interleave_first_skip;
    dspm.interleave_last = (stream_size / dspm.channel_count) % dspm.interleave;
    dspm.header_offset = 0x00;
    dspm.header_spacing = dspm.interleave;

    //todo some files end in half a frame and may click at the very end
    //todo when .dsp should refer to Ultimate Board Collection (Wii), not sure about dev
    dspm.meta_type = meta_DSP_ITL_i;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}
