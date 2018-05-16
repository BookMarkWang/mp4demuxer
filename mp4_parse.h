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

typedef atom_t atom_free_t;
typedef atom_t atom_mdat_t;
typedef atom_t atom_moov_t;

typedef struct _mp4_box_node
{
	atom_t*			data;
	struct _mp4_box_node*	parent;
	struct _mp4_box_node*	child;
	struct _mp4_box_node*	sibling;
}mp4_box_node_t;

void get_mp4_box_tree(const char* file_path, mp4_box_node_t* root);
mp4_box_node_t* pack_mp4_box_node(atom_t* data);

void mp4_read_box_container(stream_t* stream, mp4_box_node_t* node, uint64_t left_size);

atom_t* mp4_read_box_ftyp(stream_t* stream, atom_t* header);
void mp4_print_box_ftyp(atom_t* atom);
void mp4_free_box_ftyp(atom_t* atom);
 
atom_t* mp4_read_box_free(stream_t* stream, atom_t* header);
void mp4_free_box_free(atom_t* atom);

atom_t* mp4_read_box_mdat(stream_t* stream, atom_t* header);
void mp4_free_box_mdat(atom_t* atom);

atom_t* mp4_read_box_mvhd(stream_t* stream, atom_t* header);
void mp4_print_box_mvhd(atom_t* atom);
void mp4_free_box_mvhd(atom_t* atom);

atom_t* mp4_read_box_unknown(stream_t* stream, atom_t* header);
void mp4_free_box_unknown(atom_t* atom);

#endif //__MP4_PARSE_H__
