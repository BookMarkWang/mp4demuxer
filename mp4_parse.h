#ifndef __MP$_PARSE_H__
#define __MP$_PARSE_H__
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
	uint32_t 	size;
	uint32_t 	type;
	uint64_t 	large_size;
}atom_t;

typedef struct _atom_ftyp
{
	atom_t		header;
	uint32_t	major_brand;
	uint32_t	minor_version;
	uint32_t	compatible_brands_count;
	uint32_t*	compatible_brands;
}atom_ftyp_t;

typedef struct _atom_free
{
	atom_t		header;
}atom_free_t;

typedef struct _atom_mdat
{
	atom_t	header;
}atom_mdat_t;

typedef struct _mp4_box_node
{
	atom_t *data;
	struct _mp4_box_node* parent;
	struct _mp4_box_node* child;
	struct _mp4_box_node* sibling;
}mp4_box_node_t;

//MP4_PARSE_ERROR_CODE parse_mp4_box(char* file_name);
//void print_mp4_ftyp(atom_ftyp* ftyp_info);

void get_mp4_box_tree(const char* file_path, mp4_box_node_t* root);
mp4_box_node_t* pack_mp4_box_node(atom_t* data);

atom_t* mp4_read_box_ftyp(stream_t* stream, atom_t* header);
void mp4_print_box_ftyp(atom_t* atom);
void mp4_free_box_ftyp(atom_t* atom);
 
atom_t* mp4_read_box_free(stream_t* stream, atom_t* header);
void mp4_free_box_free(atom_t* atom);

atom_t* mp4_read_box_mdat(stream_t* stream, atom_t* header);
void mp4_free_box_mdat(atom_t* atom);

#endif //__MP$_PARSE_H__
