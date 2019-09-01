#include "meta.h"
#include "../util.h"
#include "../coding/nwa_decoder.h"
#include <string.h>
#include <ctype.h>


static int get_loops_nwainfo_ini(STREAMFILE *streamFile, int *out_loop_flag, int32_t *out_loop_start);
static int get_loops_gameexe_ini(STREAMFILE *streamFile, int *out_loop_flag, int32_t *out_loop_start, int32_t *out_loop_end);
static nwa_codec_data *open_nwa_vgmstream(STREAMFILE *streamFile);
static void free_nwa_vgmstream(nwa_codec_data *data);

/* NWA - Visual Art's streams [Air (PC), Clannad (PC)] */
VGMSTREAM * init_vgmstream_nwa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag = 0;
    int32_t loop_start_sample = 0, loop_end_sample = 0;
    int nwainfo_ini_found = 0, gameexe_ini_found = 0;
    int compression_level;


    /* checks */
    if (!check_extensions(streamFile, "nwa"))
        goto fail;

    channel_count = read_16bitLE(0x00,streamFile);
    if (channel_count != 1 && channel_count != 2) goto fail;

    /* check if we're using raw pcm */
    if ( read_32bitLE(0x08,streamFile)==-1 || /* compression level */
         read_32bitLE(0x10,streamFile)==0  || /* block count */
         read_32bitLE(0x18,streamFile)==0  || /* compressed data size */
         read_32bitLE(0x20,streamFile)==0  || /* block size */
         read_32bitLE(0x24,streamFile)==0 ) { /* restsize */
        compression_level = -1;
    } else {
        compression_level = read_32bitLE(0x08,streamFile);
    }

    /* loop points come from external files */
    nwainfo_ini_found = get_loops_nwainfo_ini(streamFile, &loop_flag, &loop_start_sample);
    gameexe_ini_found = !nwainfo_ini_found && get_loops_gameexe_ini(streamFile, &loop_flag, &loop_start_sample, &loop_end_sample);

    start_offset = 0x2c;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x04,streamFile);
    vgmstream->num_samples = read_32bitLE(0x1c,streamFile) / channel_count;

    switch(compression_level) {
        case -1:
            switch (read_16bitLE(0x02,streamFile)) {
                case 8:
                    vgmstream->coding_type = coding_PCM8;
                    vgmstream->interleave_block_size = 0x01;
                    break;
                case 16:
                    vgmstream->coding_type = coding_PCM16LE;
                    vgmstream->interleave_block_size = 0x02;
                    break;
                default:
                    goto fail;
            }
            vgmstream->layout_type = layout_interleave;
            break;

        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            vgmstream->coding_type = coding_NWA;
            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = open_nwa_vgmstream(streamFile);
            if (!vgmstream->codec_data) goto fail;
            break;

        default:
            goto fail;
            break;
    }


    if (nwainfo_ini_found) {
        vgmstream->meta_type = meta_NWA_NWAINFOINI;
        if (loop_flag) {
            vgmstream->loop_start_sample = loop_start_sample;
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    } else if (gameexe_ini_found) {
        vgmstream->meta_type = meta_NWA_GAMEEXEINI;
        if (loop_flag) {
            vgmstream->loop_start_sample = loop_start_sample;
            vgmstream->loop_end_sample = loop_end_sample;
        }
    } else {
        vgmstream->meta_type = meta_NWA;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* try to locate NWAINFO.INI in the same directory */
static int get_loops_nwainfo_ini(STREAMFILE *streamFile, int *out_loop_flag, int32_t *out_loop_start) {
    STREAMFILE *streamLoops;
    char namebase[PATH_LIMIT];
    const char * ext;
    int length;
    int found;
    off_t offset;
    size_t file_size;
    off_t found_off = -1;
    int loop_flag = 0;
    int32_t loop_start_sample = 0;


    streamLoops = open_streamfile_by_filename(streamFile, "NWAINFO.INI");
    if (!streamLoops) goto fail;

    get_streamfile_filename(streamFile,namebase,PATH_LIMIT);

    /* ini found, try to find our name */
    ext = filename_extension(namebase);
    length = ext - 1 - namebase;
    file_size = get_streamfile_size(streamLoops);

    for (found = 0, offset = 0; !found && offset < file_size; offset++) {
        off_t suboffset;
        /* Go for an n*m search 'cause it's easier than building an
         * FSA for the search string. Just wanted to make the point that
         * I'm not ignorant, just lazy. */
        for (suboffset = offset;
                suboffset < file_size &&
                suboffset-offset < length &&
                read_8bit(suboffset,streamLoops) == namebase[suboffset-offset];
                suboffset++) {
            /* skip */
        }

        if (suboffset-offset==length && read_8bit(suboffset,streamLoops)==0x09) { /* tab */
            found = 1;
            found_off = suboffset+1;
        }
    }

    /* if found file name in INI */
    if (found) {
        char loopstring[9] = {0};

        if (read_streamfile((uint8_t*)loopstring,found_off,8,streamLoops) == 8) {
            loop_start_sample = atol(loopstring);
            if (loop_start_sample > 0)
                loop_flag = 1;
        }
    }


    *out_loop_flag = loop_flag;
    *out_loop_start = loop_start_sample;

    close_streamfile(streamLoops);
    return 1;

fail:
    close_streamfile(streamLoops);
    return 0;
}

/* try to locate Gameexe.ini in the same directory */
static int get_loops_gameexe_ini(STREAMFILE *streamFile, int *out_loop_flag, int32_t *out_loop_start, int32_t *out_loop_end) {
    STREAMFILE *streamLoops;
    char namebase[PATH_LIMIT];
    const char * ext;
    int length;
    int found;
    off_t offset;
    off_t file_size;
    off_t found_off = -1;
    int loop_flag = 0;
    int32_t loop_start_sample = 0, loop_end_sample = 0;


    streamLoops = open_streamfile_by_filename(streamFile, "Gameexe.ini");
    if (!streamLoops) goto fail;

    get_streamfile_filename(streamFile,namebase,PATH_LIMIT);

    /* ini found, try to find our name */
    ext = filename_extension(namebase);
    length = ext-1-namebase;
    file_size = get_streamfile_size(streamLoops);

    /* format of line is:
     * #DSTRACK = 00000000 - eeeeeeee - ssssssss = "name"    = "name2?"
     *                       ^22        ^33         ^45         ^57
     */

    for (found = 0, offset = 0; !found && offset<file_size; offset++) {
        off_t suboffset;
        uint8_t buf[10];

        if (read_8bit(offset,streamLoops)!='#') continue;
        if (read_streamfile(buf,offset+1,10,streamLoops)!=10) break;
        if (memcmp("DSTRACK = ",buf,10)) continue;
        if (read_8bit(offset+44,streamLoops)!='\"') continue;

        for (suboffset = offset+45;
                suboffset < file_size &&
                suboffset-offset-45 < length &&
                tolower(read_8bit(suboffset,streamLoops)) == tolower(namebase[suboffset-offset-45]);
                suboffset++) {
            /* skip */
        }

        if (suboffset-offset-45==length && read_8bit(suboffset,streamLoops)=='\"') { /* tab */
            found = 1;
            found_off = offset+22; /* loop end */
        }
    }

    if (found) {
        char loopstring[9] = {0};
        int start_ok = 0, end_ok = 0;
        int32_t total_samples = read_32bitLE(0x1c,streamFile) / read_16bitLE(0x00,streamFile);

        if (read_streamfile((uint8_t*)loopstring,found_off,8,streamLoops)==8)
        {
            if (!memcmp("99999999",loopstring,8)) {
                loop_end_sample = total_samples;
            }
            else {
                loop_end_sample = atol(loopstring);
            }
            end_ok = 1;
        }
        if (read_streamfile((uint8_t*)loopstring,found_off+11,8,streamLoops)==8)
        {
            if (!memcmp("99999999",loopstring,8)) {
                /* not ok to start at last sample,
                 * don't set start_ok flag */
            }
            else if (!memcmp("00000000",loopstring,8)) {
                /* loops from the start aren't really loops */
            }
            else {
                loop_start_sample = atol(loopstring);
                start_ok = 1;
            }
        }

        if (start_ok && end_ok) loop_flag = 1;
    }   /* if found file name in INI */


    *out_loop_flag = loop_flag;
    *out_loop_start = loop_start_sample;
    *out_loop_end = loop_end_sample;

    close_streamfile(streamLoops);
    return 1;

fail:
    close_streamfile(streamLoops);
    return 0;
}


static nwa_codec_data *open_nwa_vgmstream(STREAMFILE *streamFile) {
    nwa_codec_data *data = NULL;
    char filename[PATH_LIMIT];

    streamFile->get_name(streamFile,filename,sizeof(filename));

    data = malloc(sizeof(nwa_codec_data));
    if (!data) goto fail;

    data->nwa = open_nwa(streamFile,filename);
    if (!data->nwa) goto fail;

    return data;

fail:
    free_nwa_vgmstream(data);
    return NULL;
}

static void free_nwa_vgmstream(nwa_codec_data *data) {
    if (data) {
        if (data->nwa) {
            close_nwa(data->nwa);
        }
        free(data);
    }
}