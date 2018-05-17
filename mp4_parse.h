#ifndef __MP4_PARSE_H__
#define __MP4_PARSE_H__
#include <stdint.h>
#include <stdbool.h>
#include "stream.h"

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
	int32_t			width;
	int32_t			height;
}atom_tkhd_t;

typedef atom_t atom_free_t;
typedef atom_t atom_mdat_t;
typedef atom_t atom_moov_t;

typedef struct _atom_dref
{
	atom_full_t header;
	uint32_t entry_count;
}atom_dref_t;

typedef struct _atom_url
{
	atom_full_t header;
	char* location;
}atom_url_t;

typedef struct _atom_urn
{
	atom_full_t header;
	char* name;
	char* location;
}atom_urn_t;

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

void get_mp4_box_tree(const char* file_path, mp4_box_node_t* root);
void show_mp4_box_tree(mp4_box_node_t* root);
void free_mp4_box_tree(mp4_box_node_t* root);
mp4_box_node_t* pack_mp4_box_node(atom_t* data);

atom_box_handle_t* find_atom_box_handle(uint32_t type);

void mp4_read_box(stream_t* stream, mp4_box_node_t* node, uint64_t left_size);

mp4_box_node_t* mp4_read_box_container(stream_t* stream, atom_t* header);
void mp4_print_box_container(atom_t* atom);
void mp4_free_box_container(atom_t* atom);

mp4_box_node_t* mp4_read_box_ftyp(stream_t* stream, atom_t* header);
void mp4_print_box_ftyp(atom_t* atom);
void mp4_free_box_ftyp(atom_t* atom);
 
mp4_box_node_t* mp4_read_box_free(stream_t* stream, atom_t* header);
void mp4_print_box_free(atom_t* atom);
void mp4_free_box_free(atom_t* atom);

mp4_box_node_t* mp4_read_box_mdat(stream_t* stream, atom_t* header);
void mp4_print_box_mdat(atom_t* atom);
void mp4_free_box_mdat(atom_t* atom);

mp4_box_node_t* mp4_read_box_mvhd(stream_t* stream, atom_t* header);
void mp4_print_box_mvhd(atom_t* atom);
void mp4_free_box_mvhd(atom_t* atom);

mp4_box_node_t* mp4_read_box_tkhd(stream_t* stream, atom_t* header);
void mp4_print_box_tkhd(atom_t* atom);
void mp4_free_box_tkhd(atom_t* atom);

mp4_box_node_t* mp4_read_box_dref(stream_t* stream, atom_t* header);
void mp4_print_box_dref(atom_t* atom);
void mp4_free_box_dref(atom_t* atom);

mp4_box_node_t* mp4_read_box_url(stream_t* stream, atom_t* header);
void mp4_print_box_url(atom_t* atom);
void mp4_free_box_url(atom_t* atom);

mp4_box_node_t* mp4_read_box_urn(stream_t* stream, atom_t* header);
void mp4_print_box_urn(atom_t* atom);
void mp4_free_box_urn(atom_t* atom);

mp4_box_node_t* mp4_read_box_stsd(stream_t* stream, atom_t* header);
void mp4_print_box_stsd(atom_t* atom);
void mp4_free_box_stsd(atom_t* atom);

mp4_box_node_t* mp4_read_box_common(stream_t* stream, atom_t* header);
void mp4_print_box_common(atom_t* atom);
void mp4_free_box_common(atom_t* atom);

mp4_box_node_t* mp4_read_box_unknown(stream_t* stream, atom_t* header);
void mp4_print_box_unknown(atom_t* atom);
void mp4_free_box_unknown(atom_t* atom);

#endif //__MP4_PARSE_H__
