#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <endian.h>
#include "mp4_parse.h"
#include "stream.h"
#include "trace.h"

#define FOURCC(c1, c2, c3, c4) (c4 << 24 | c3 << 16 | c2 << 8 | c1)

#define MP4_ENTER_BOX(type_t) \
	type_t* box = (type_t*)calloc(1, sizeof(type_t));\
	((atom_t*)box)->size = be32toh(header->size);\
	((atom_t*)box)->type = header->type;\
	((atom_t*)box)->large_size = be64toh(header->large_size);\
	uint64_t left_size = 0;\
	if(((atom_t*)box)->size == 0)\
	{\
		left_size = ((atom_t*)box)->size - sizeof(atom_t);\
	}\
	else\
	{\
		left_size = ((atom_t*)box)->size - sizeof(atom_t) + sizeof(uint64_t);\
	}

#define MP4_EXIT_BOX() \
		if(left_size != 0)\
		{\
			TRACE_WARNING("The box do not match the format!%llu size data is discarded!!", left_size);\
			/*TODO:need to discard the left data*/\
		}

#define MP4_GET_STREAM_BYTES(stream, buf, size) \
		({\
			MP4_PARSE_ERROR_CODE_T ret = MP4_PARSE_ERROR_OK;\
			int32_t read_size = 0;\
			read_size = stream_read(stream, buf, size);\
			if(read_size != size) \
			{\
				TRACE_ERROR("Donot have enough data, need %d, read %d", size, read_size);\
				ret = MP4_PARSE_ERROR_DATA_TRUNCATED;\
			}\
			left_size -= read_size;\
			ret;\
		})

#define MP4_GET_BYTE(stream, buf) \
		{\
			MP4_GET_STREAM_BYTES(stream, buf, 1);\
		}

#define MP4_GET_2BYTES(stream, buf) \
		{\
			MP4_GET_STREAM_BYTES(stream, buf, 2);\
			*buf = be16toh(*buf);\
		}

#define MP4_GET_4BYTES(stream, buf) \
		{\
			MP4_GET_STREAM_BYTES(stream, buf, 4);\
			*buf = be32toh(*buf);\
		}

#define MP4_GET_8BYTES(stream, buf) \
		{\
			MP4_GET_STREAM_BYTES(stream, buf, 8);\
			*buf = be64toh(*buf);\
		}

#define MP4_GET_FOURCC(stream, buf) MP4_GET_STREAM_BYTES(stream, buf, 4)

#define MP4_SKIP_DATA(stream, offset) stream_seek(stream, offset, SEEK_CUR)

#define MP4_PRINT_FOURCC(info, buf) \
		{\
			char tmp[5] = {'\0'};\
			uint8_t* cc = (uint8_t*)buf;\
			snprintf(tmp, 5, "%c%c%c%c", cc[0], cc[1], cc[2], cc[3]);\
			TRACE_LOG(info"%s", tmp);\
		}

#define MP4_PRINT_HEADER(header) \
		{\
			char tmp[5] = {'\0'};\
			uint8_t* cc = (uint8_t*)(&header.type);\
			snprintf(tmp, 5, "%c%c%c%c", cc[0], cc[1], cc[2], cc[3]);\
			TRACE_LOG("size:%d, type:%s", be32toh(header.size), tmp);\
		}

#define MP4_GET_BOX_HEADER(stream, buf) \
		({\
			MP4_PARSE_ERROR_CODE_T ret = MP4_PARSE_ERROR_OK;\
			int32_t read_size = 0;\
			int32_t size = sizeof(atom_t);\
			read_size = stream_read(stream, buf, size);\
			if(read_size != size) \
			{\
				TRACE_ERROR("Donot have enough data for the header, need %d, read %d", size, read_size);\
				ret = MP4_PARSE_ERROR_DATA_TRUNCATED;\
			}\
			if(size)\
			{\
				stream_seek(stream, 0-sizeof(uint64_t), SEEK_CUR);\
			}\
			ret;\
		})

void get_mp4_box_tree(const char* file_path, mp4_box_node_t* node)
{
	TRACE_LOG("path:%s", file_path);
	atom_t header;
	atom_t* result = NULL;
	stream_t *stream = create_file_stream();
	stream_open_file(stream, file_path, "r");
	while(1)
	{
		memset(&header, 0, sizeof(atom_t));
		MP4_GET_BOX_HEADER(stream, &header);
		MP4_PRINT_HEADER(header);
		switch(header.type)
		{
		case FOURCC('f','t', 'y', 'p'):
			result = mp4_read_box_ftyp(stream, &header);
			mp4_print_box_ftyp(result);
			break;
		case FOURCC('f','r', 'e', 'e'):
			mp4_read_box_free(stream, &header);
			break;
		default:
			{
				MP4_PRINT_FOURCC("Unimplemented type:", &header.type);
				goto finish;
			}
		}
	}
finish:
	stream_close(stream);
	destroy_file_stream(stream);
}

atom_t* mp4_read_box_ftyp(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_ftyp_t);

	MP4_GET_FOURCC(stream, &box->major_brand);
	MP4_GET_4BYTES(stream, &box->minor_version);
	box->compatible_brands_count = left_size / sizeof(uint32_t);
	box->compatible_brands = (uint32_t*)calloc( box->compatible_brands_count, sizeof(uint32_t) );
	for(uint32_t i = 0; i < box->compatible_brands_count; i++)
	{
		MP4_GET_FOURCC(stream, box->compatible_brands++);
	}

	MP4_EXIT_BOX();
	return (atom_t*)box;
}

void mp4_print_box_ftyp(atom_t* atom)
{
	atom_ftyp_t *ftyp = (atom_ftyp_t*)atom;
	TRACE_LOG("size:%d", ftyp->header.size);
	MP4_PRINT_FOURCC("type:", &ftyp->header.type);
	MP4_PRINT_FOURCC( "major_brand:", &ftyp->major_brand );
	TRACE_LOG("minor_version:%d", ftyp->minor_version);
	TRACE_LOG("compatible_brands:");
	for(int i = 0; i < ftyp->compatible_brands_count; i++)
	{
		MP4_PRINT_FOURCC("", &ftyp->compatible_brands[i]);
	}
}

void mp4_free_box_ftyp(atom_t* atom)
{
	atom_ftyp_t *ftyp = (atom_ftyp_t*)atom;
	free(ftyp->compatible_brands);
	free(ftyp);
}

atom_t* mp4_read_box_free(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_free_t);

	MP4_SKIP_DATA(stream, left_size);

	MP4_EXIT_BOX();
	return (atom_t*)box;
}

atom_t* mp4_read_box_mdat(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_mdat_t);
	MP4_EXIT_BOX();
	return (atom_t*)box;
}

mp4_box_node_t* pack_mp4_box_node(atom_t* data)
{
	mp4_box_node_t* node = (mp4_box_node_t*)calloc(1, sizeof(mp4_box_node_t));
	node->data = data;
	return node;
}
