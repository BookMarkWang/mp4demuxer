#ifndef __MP4_PARSE_H__
#define __MP4_PARSE_H__
#include <stdint.h>
#include <stdbool.h>
#include "stream.h"

#define MP4_BOX_HANDLER(type) \
	mp4_box_node_t* mp4_read_box_##type(stream_t* stream, atom_t* header);\
	void mp4_free_box_##type(atom_t* atom);\
	void mp4_print_box_##type(atom_t* atom);

typedef enum _MP4_PARSE_ERROR_CODE
{
	MP4_PARSE_ERROR_OK,
	MP4_PARSE_ERROR_DATA_TRUNCATED,
	MP$_PARSE_ERROR_UNKNOWN,
}MP4_PARSE_ERROR_CODE_T;

typedef struct _atom
{
	uint32_t 		size;
	uint32_t 		type;
	uint64_t 		large_size;
}atom_t;

typedef struct _atom_full
{
	atom_t			header;
	uint8_t			version;
	uint8_t			flags[3];
}atom_full_t;

typedef struct _atom_ftyp
{
	atom_t			header;
	uint32_t		major_brand;
	uint32_t		minor_version;
	uint32_t		compatible_brands_count;
	uint32_t*		compatible_brands;
}atom_ftyp_t;

typedef struct _atom_mvhd
{
	atom_full_t		header;
	uint64_t		creation_time;
	uint64_t		modification_time;
	uint32_t		timescale;
	uint64_t		duration;
	int32_t			rate;
	int16_t			volume;
	uint16_t		reserved;
	uint32_t		reserved2[2];
	int32_t			matrix[9];
	uint32_t		pre_defined[6];
	uint32_t		next_track_id;
}atom_mvhd_t;

typedef struct _atom_tkhd
{
	atom_full_t		header;
	uint64_t		creation_time;
	uint64_t		modification_time;
	uint32_t		track_id;
	uint32_t		reserved;
	uint64_t		duration;
	uint32_t		reserved2[2];
	int16_t			layer;
	int16_t			alternate_group;
	int16_t			volume;
	uint16_t		reserved3;
	int32_t			matrix[9];
	uint32_t		width;
	uint32_t		height;
}atom_tkhd_t;

typedef struct _atom_mdhd
{
	atom_full_t		header;
	uint64_t		creation_time;
	uint64_t		modification_time;
	uint32_t		timescale;
	uint64_t		duration;
	uint8_t			pad;
	uint8_t			language[3];
	uint16_t		pre_defined;
}atom_mdhd_t;

typedef atom_t atom_free_t;
typedef atom_t atom_mdat_t;
typedef atom_t atom_moov_t;

typedef struct _atom_dref
{
	atom_full_t		header;
	uint32_t		entry_count;
}atom_dref_t;

typedef struct _atom_url
{
	atom_full_t		header;
	char*			location;
}atom_url_t;

typedef struct _atom_urn
{
	atom_full_t		header;
	char*			name;
	char*			location;
}atom_urn_t;

typedef struct _atom_edit_content
{
	uint64_t		segment_duration;
	int64_t			media_time;
	int16_t			media_rate_integer;
	int16_t			media_rate_fraction;
}atom_edit_content_t;

typedef struct _atom_elst
{
	atom_full_t             header;
	uint32_t		entry_count;
	atom_edit_content_t*	entries;
}atom_elst_t;

typedef struct _atom_hdlr
{
	atom_full_t             header;
	uint32_t		pre_defined;
	uint32_t		handler_type;
	uint32_t		reserved[3];
	char*			name;
}atom_hdlr_t;

typedef struct _atom_vmhd
{
	atom_full_t             header;
	uint16_t		graphicsmode;
	uint16_t		opcolor[3];
}atom_vmhd_t;

typedef struct _atom_stsd
{
	atom_full_t		header;
	uint32_t		entry_count;
}atom_stsd_t;

typedef struct _atom_sample_entry
{
	atom_t			header;
	uint8_t			reserved[6];
	uint16_t		data_reference_index;
}atom_sample_entry_t;

typedef struct _atom_sample_vide
{
	atom_sample_entry_t	header;
	uint16_t		pre_defined;
	uint16_t		reserved;
	uint32_t		pre_defined2[3];
	uint16_t		width;
	uint16_t		height;
	uint32_t		horizresolution;
	uint32_t		vertresolution;
	uint32_t		reserved2;
	uint16_t		frame_count;
	uint8_t			compressorname[32];
	uint16_t		depth;
	int16_t			pre_defined3;
}atom_sample_vide_t;

typedef struct _atom_sample_soun
{
	atom_sample_entry_t     header;
	uint32_t		reserved[2];
	uint16_t		channel_count;
	uint16_t		sample_size;
	uint16_t		pre_defined;
	uint16_t		reserved2;
	uint32_t		sample_rate;
}atom_sample_soun_t;

typedef struct _atom_sample_soun_v1
{
	atom_sample_entry_t     header;
	uint16_t		entry_version;
	uint32_t		reserved[3];
	uint16_t		channel_count;
	uint16_t		sample_size;
	uint16_t		pre_defined;
	uint16_t		reserved2;
	uint32_t		sample_rate;
}atom_sample_soun_v1_t;

typedef struct _atom_srat
{
	atom_full_t		header;
	uint32_t		sampling_rate;
}atom_srat_t;

typedef struct _atom_avcc
{
	atom_t			header;
	uint8_t			configuration_version;
	uint8_t			avc_profile_indication;
	uint8_t			profile_compatibility;
	uint8_t			avc_level_indication;
	uint8_t			reserved;//6bits
	uint8_t			length_size_minus_one;//2bits
	uint8_t			reserved2;//3bits
	uint8_t			num_of_sequence_parameter_sets;//5bits
	uint16_t*		sequence_parameter_set_length;
	uint8_t**		sequence_parameter_set_nal_unit;
	uint8_t			num_of_picture_parameter_sets;
	uint16_t*		picture_parameter_set_length;
	uint8_t**		picture_parameter_set_nal_unit;

	//existed when profile_idc=100,110,122,144
	uint8_t			reserved3;//6bits
	uint8_t			chroma_format;//2bits
	uint8_t			reserved4;//5bits
	uint8_t			bit_depth_luma_minus8;//3bits
	uint8_t			reserved5;//5bits
	uint8_t			bit_depth_chroma_minus8;//3bits
	uint8_t			num_of_sequence_parameter_set_ext;
	uint16_t*		sequence_parameter_set_ext_length;
	uint8_t**		sequence_parameter_set_ext_nal_unit;
}atom_avcc_t;

typedef struct _atom_stts
{
	atom_full_t		header;
	uint32_t		entry_count;
	uint32_t*		sample_count;
	uint32_t*		sample_delta;
}atom_stts_t;

typedef struct _atom_stss
{
	atom_full_t		header;
	uint32_t		entry_count;
	uint32_t*		sample_number;
}atom_stss_t;

typedef struct _atom_stsc
{
	atom_full_t		header;
	uint32_t		entry_count;
	uint32_t*		first_chunk;
	uint32_t*		samples_per_chunk;
	uint32_t*		sample_description_index;
}atom_stsc_t;

typedef struct _atom_stsz
{
	atom_full_t		header;
	uint32_t		sample_size;
	uint32_t		sample_count;
	uint32_t*		entry_size;
}atom_stsz_t;

typedef struct _atom_stco
{
	atom_full_t		header;
	uint32_t		entry_count;
	uint32_t*		chunk_offset;
}atom_stco_t;

typedef struct _atom_co64
{
	atom_full_t		header;
	uint32_t		entry_count;
	uint64_t*		chunk_offset;
}atom_co64_t;

typedef struct _atom_smhd
{
	atom_full_t		header;
	int16_t			balance;
	uint16_t		reserved;
}atom_smhd_t;

typedef struct _atom_meta
{
	atom_full_t		header;
}atom_meta_t;

typedef struct _mp4_box_node
{
	atom_t*			data;
	struct _mp4_box_node*	parent;
	struct _mp4_box_node*	child;
	struct _mp4_box_node*	sibling;
}mp4_box_node_t;

typedef struct _atom_box_handle
{
	mp4_box_node_t* (*read_box)(stream_t* stream, atom_t* header);
	void (*free_box)(atom_t* atom);
	void (*print_box)(atom_t* atom);
}atom_box_handle_t;

typedef struct _atom_map
{
	uint32_t		type;
	atom_box_handle_t	handler;
}atom_map_t;

typedef struct _frame_head
{
	uint32_t	type;
	uint32_t	track_id;
	uint32_t	frame_count;
}frame_head_t;

typedef struct _frame_info
{
	uint8_t		key_frame;
	uint64_t	pkt_pts;
	double		pkt_pts_time;
	uint64_t	pkt_dts;
	double		pkt_dts_time;
	uint64_t	pkt_duration;
	double		pkt_duration_time;
	uint64_t	pkt_offset;
	uint32_t	pkt_size;
}frame_info_t;

typedef struct _video_frame_info
{
	frame_head_t	header;
	uint16_t	width_integer;
	uint16_t	width_fraction;
	uint16_t	height_integer;
	uint16_t	height_fraction;

	//TODO: frame_type, pix_fmt
	frame_info_t*	frames;
}video_frame_info_t;

typedef struct _audio_frame_info
{
	frame_head_t	header;
	uint16_t	channel_count;

	frame_info_t*	frames;
}audio_frame_info_t;

void get_mp4_box_tree(const char* file_path, mp4_box_node_t* root);
void show_mp4_box_tree(mp4_box_node_t* root);
void free_mp4_box_tree(mp4_box_node_t* root);
void extract_mp4_track_data(mp4_box_node_t* root);
void show_frames(frame_head_t* frames);
atom_t* find_atom(mp4_box_node_t* root, uint32_t type);//only return the first one
mp4_box_node_t* pack_mp4_box_node(atom_t* data);

atom_box_handle_t* find_atom_box_handle(uint32_t type);

void mp4_read_box(stream_t* stream, mp4_box_node_t* node, uint64_t left_size);

MP4_BOX_HANDLER(container)
MP4_BOX_HANDLER(ftyp)
MP4_BOX_HANDLER(free)
MP4_BOX_HANDLER(mdat)
MP4_BOX_HANDLER(mvhd)
MP4_BOX_HANDLER(tkhd)
MP4_BOX_HANDLER(dref)
MP4_BOX_HANDLER(url)
MP4_BOX_HANDLER(urn)
MP4_BOX_HANDLER(elst)
MP4_BOX_HANDLER(mdhd)
MP4_BOX_HANDLER(hdlr)
MP4_BOX_HANDLER(vmhd)
MP4_BOX_HANDLER(stsd)
MP4_BOX_HANDLER(sample_vide)
MP4_BOX_HANDLER(sample_soun)
MP4_BOX_HANDLER(sample_soun_v1)
MP4_BOX_HANDLER(srat)
MP4_BOX_HANDLER(avcc)
MP4_BOX_HANDLER(stts)
MP4_BOX_HANDLER(stss)
MP4_BOX_HANDLER(stsc)
MP4_BOX_HANDLER(stsz)
MP4_BOX_HANDLER(stco)
MP4_BOX_HANDLER(co64)
MP4_BOX_HANDLER(smhd)
MP4_BOX_HANDLER(meta)
MP4_BOX_HANDLER(common)
MP4_BOX_HANDLER(unknown)

#endif //__MP4_PARSE_H__
