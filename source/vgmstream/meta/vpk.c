#include "meta.h"
#include "../coding/coding.h"

/* VPK - from SCE America second party devs [God of War (PS2), NBA 08 (PS3)] */
VGMSTREAM * init_vgmstream_ps2_vpk(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    off_t start_offset, loop_channel_offset;
    size_t channel_size;


    /* checks */
    if (!check_extensions(streamFile, "vpk"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x204B5056) /* " KPV" */
        goto fail;

    /* seems this consistently has 0x10-0x20 extra bytes, landing in garbage 0xC00000..00 frames at the end */
    channel_size = read_32bitLE(0x04,streamFile) - 0x20; /* remove for cleaner ends */

    start_offset = read_32bitLE(0x08,streamFile);
    channel_count = read_32bitLE(0x14,streamFile);
    /* 0x18+(per channel): channel config(?) */
    loop_channel_offset = read_32bitLE(0x7FC,streamFile);
    loop_flag = (loop_channel_offset != 0); /* found in Sly 2/3 */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size*vgmstream->channels,vgmstream->channels);
    if (vgmstream->loop_flag) {
        vgmstream->loop_start_sample = ps_bytes_to_samples(loop_channel_offset*vgmstream->channels,vgmstream->channels);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->meta_type = meta_VPK;
    vgmstream->coding_type = coding_PSX;
    vgmstream->interleave_block_size = read_32bitLE(0x0C,streamFile) / 2; /* even in >2ch */
    vgmstream->layout_type = layout_interleave;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
