#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mp4_parse.h"
#include "stream.h"
#include "trace.h"

#define FOURCC(c1, c2, c3, c4) (c4 << 24 | c3 << 16 | c2 << 8 | c1)

#define MP4_ENTER_BOX(type_t) \
	TRACE_FUNC_HEAD\
	type_t* box = (type_t*)calloc(1, sizeof(type_t));\
	((atom_t*)box)->size = header->size;\
	((atom_t*)box)->type = header->type;\
	((atom_t*)box)->large_size = header->large_size;\
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
		}\
		TRACE_FUNC_TAIL

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

#define MP4_SKIP_DATA(stream, offset) \
			{\
				stream_seek(stream, offset, SEEK_CUR);\
				left_size -= offset;\
			}

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
			TRACE_LOG("size:%d, type:%s", header.size, tmp);\
		}

#define MP4_GET_BOX_HEADER(stream, buf) \
		({\
			MP4_PARSE_ERROR_CODE_T ret = MP4_PARSE_ERROR_OK;\
			int32_t read_size = 0;\
			int32_t size = sizeof(atom_t);\
			uint32_t *data = (uint32_t*)buf;\
			read_size = stream_read(stream, buf, size);\
			if(read_size != size) \
			{\
				TRACE_ERROR("Donot have enough data for the header, need %d, read %d", size, read_size);\
				ret = MP4_PARSE_ERROR_DATA_TRUNCATED;\
			}\
			if(size)\
			{\
				stream_seek(stream, 0-sizeof(uint64_t), SEEK_CUR);\
				*data = be32toh(*data);\
			}\
			else\
			{\
				*(data+1) = be64toh(*(data+1));\
			}\
			ret;\
		})

void get_mp4_box_tree(const char* file_path, mp4_box_node_t* node)
{
	TRACE_LOG("path:%s", file_path);
	struct stat stat_buf;
	stream_t *stream = create_file_stream();
	stream_open_file(stream, file_path, "r");
	if(stat(file_path, &stat_buf) != 0)
	{
		TRACE_ERROR("failed to parse file:%s", file_path);
	}
	mp4_read_box_container(stream, node, stat_buf.st_size);
	stream_close(stream);
	destroy_file_stream(stream);
}

void mp4_read_box_container(stream_t* stream, mp4_box_node_t* node, uint64_t left_size)
{
	mp4_box_node_t* new_node = NULL, *tmp_node = NULL;
	atom_t header;
	atom_t* result = NULL;
	while(left_size)
	{
		memset(&header, 0, sizeof(atom_t));
		MP4_GET_BOX_HEADER(stream, &header);
		MP4_PRINT_HEADER(header);
		switch(header.type)
		{
		case FOURCC('f','t', 'y', 'p'):
			{
				result = mp4_read_box_ftyp(stream, &header);
				new_node = pack_mp4_box_node(result);
				if(node->child == NULL)
				{
					node->child = new_node;
				}
				else
				{
					tmp_node->sibling = new_node;
				}
				tmp_node = new_node;
				mp4_print_box_ftyp(result);
				break;
			}
		case FOURCC('f','r', 'e', 'e'):
			{
				result = mp4_read_box_free(stream, &header);
				new_node = pack_mp4_box_node(result);
				if(node->child == NULL)
				{
					node->child = new_node;
				}
				else
				{
					tmp_node->sibling = new_node;
				}
				tmp_node = new_node;
				break;
			}
		case FOURCC('m','d', 'a', 't'):
			{
				result = mp4_read_box_mdat(stream, &header);
				new_node = pack_mp4_box_node(result);
				if(node->child == NULL)
				{
					node->child = new_node;
				}
				else
				{
					tmp_node->sibling = new_node;
				}
				tmp_node = new_node;
				break;
			}
		case FOURCC('m','o', 'o', 'v'):
			{
				new_node = pack_mp4_box_node(&header);
				if(node->child == NULL)
				{
					node->child = new_node;
				}
				else
				{
					tmp_node->sibling = new_node;
				}
				tmp_node = new_node;
				if(header.size)
				{
					mp4_read_box_container(stream, new_node, (uint64_t)header.size - sizeof(atom_t) + sizeof(uint64_t));
				}
				else
				{
					mp4_read_box_container(stream, new_node, header.large_size - sizeof(atom_t));
				}
				break;
			}
		case FOURCC('m', 'v', 'h', 'd'):
			{
				result = mp4_read_box_mvhd(stream, &header);
				new_node = pack_mp4_box_node(result);
				mp4_print_box_mvhd(result);
				break;
			}
		default:
			{
				MP4_PRINT_FOURCC("Unimplemented type:", &header.type);
				result = mp4_read_box_unknown(stream, &header);
				new_node = pack_mp4_box_node(result);
				break;
			}
		}
		if(node->child == NULL)
		{
			node->child = new_node;
		}
		else
		{
			tmp_node->sibling = new_node;
		}
		tmp_node = new_node;
		if(header.size)
		{
			left_size -= header.size;
		}
		else
		{
			left_size -= header.large_size;
		}
		TRACE_LOG("left_size:%llu", left_size);
	}
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
	MP4_SKIP_DATA(stream, left_size);
	MP4_EXIT_BOX();
	return (atom_t*)box;
}

atom_t* mp4_read_box_mvhd(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_mvhd_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);

	if(box->header.version == 0)
	{
		MP4_GET_4BYTES(stream, &box->creation_time);
		MP4_GET_4BYTES(stream, &box->modification_time);
		MP4_GET_4BYTES(stream, &box->timescale);
		MP4_GET_4BYTES(stream, &box->duration);
	}
	else if(box->header.version == 1)
	{
		MP4_GET_8BYTES(stream, &box->creation_time);
		MP4_GET_8BYTES(stream, &box->modification_time);
		MP4_GET_4BYTES(stream, &box->timescale);
		MP4_GET_8BYTES(stream, &box->duration);
	}
	else
	{
		TRACE_WARNING("Unknown mvhd version:%d", box->header.version);
	}
	MP4_GET_4BYTES(stream, &box->rate);
	MP4_GET_2BYTES(stream, &box->volume);
	MP4_GET_2BYTES(stream, &box->reserved);
	MP4_GET_4BYTES(stream, &box->reserved2[0]);
	MP4_GET_4BYTES(stream, &box->reserved2[1]);
	for(uint8_t i = 0; i < 9; i++)
	{
		MP4_GET_4BYTES(stream, &box->matrix[i]);
	}
	for(uint8_t i = 0; i < 6; i++)
	{
		MP4_GET_4BYTES(stream, &box->pre_defined[i]);
	}
	MP4_GET_4BYTES(stream, &box->next_track_id);
	
	MP4_EXIT_BOX();
	return (atom_t*)box;
}

atom_t* mp4_read_box_unknown(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_t);
	MP4_SKIP_DATA(stream, left_size);
	MP4_EXIT_BOX();
	return (atom_t*)box;
}

void mp4_free_box_ftyp(atom_t* atom)
{
	atom_ftyp_t *ftyp = (atom_ftyp_t*)atom;
	free(ftyp->compatible_brands);
	free(ftyp);
}

void mp4_free_box_mvhd(atom_t* atom)
{
	atom_mvhd_t *mvhd = (atom_mvhd_t*)atom;
	free(mvhd);
}

void mp4_free_box_unknown(atom_t* atom)
{
	free(atom);
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

void mp4_print_box_mvhd(atom_t* atom)
{
	atom_mvhd_t *mvhd = (atom_mvhd_t*)atom;
	MP4_PRINT_HEADER(mvhd->header.header);
	TRACE_LOG("version:%d, flags:0x%x%x%x", mvhd->header.version, mvhd->header.flags[0], mvhd->header.flags[1], mvhd->header.flags[2]);
	TRACE_LOG("creation_time:%llu", mvhd->creation_time);
	TRACE_LOG("modification_time:%llu", mvhd->modification_time);
	TRACE_LOG("timescale:%d", mvhd->timescale);
	TRACE_LOG("duration:%llu", mvhd->duration);
}

mp4_box_node_t* pack_mp4_box_node(atom_t* data)
{
	mp4_box_node_t* node = (mp4_box_node_t*)calloc(1, sizeof(mp4_box_node_t));
	node->data = data;
	return node;
}
