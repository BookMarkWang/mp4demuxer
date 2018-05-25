#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mp4_parse.h"
#include "stream.h"
#include "trace.h"

#define MAX_ARRAY_PRINT_SIZE 10

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
			 MP4_SKIP_DATA(stream, left_size);\
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

#define MP4_GET_STRING(stream, addr) \
		{\
			if(left_size)\
			{\
				uint64_t tmp_size = left_size;\
				uint8_t* data = (uint8_t*)calloc(1, left_size);\
				MP4_GET_STREAM_BYTES(stream, data, left_size);\
				uint64_t length = strnlen(data, tmp_size - 1);\
				if(length < tmp_size - 1)\
				{\
					left_size = tmp_size - (length + 1);\
					stream_seek(stream, 0-left_size, SEEK_CUR);\
				}\
				addr = strdup(data);\
				free(data);\
			}\
		}

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

#define MP4_PRINT_FULL_HEADER(full_header) \
		{\
			MP4_PRINT_HEADER(full_header.header);\
			TRACE_LOG("version:%d", full_header.version);\
			TRACE_LOG("flags:0x%02x%02x%02x", full_header.flags[0], full_header.flags[1], full_header.flags[2]);\
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
	TRACE_FUNC_HEAD
	TRACE_LOG("path:%s", file_path);
	struct stat stat_buf;
	stream_t *stream = create_file_stream();
	stream_open_file(stream, file_path, "r");
	if(stat(file_path, &stat_buf) != 0)
	{
		TRACE_ERROR("failed to parse file:%s", file_path);
	}
	mp4_read_box(stream, node, stat_buf.st_size);
	stream_close(stream);
	destroy_file_stream(stream);
	TRACE_FUNC_TAIL
}

void mp4_read_box(stream_t* stream, mp4_box_node_t* node, uint64_t left_size)
{
	TRACE_FUNC_HEAD
	TRACE_LOG("left_size:%llu", left_size);
	mp4_box_node_t* new_node = NULL, *tmp_node = node;
	atom_t header;
	atom_t* result = NULL;
	while(left_size)
	{
		memset(&header, 0, sizeof(atom_t));
		MP4_GET_BOX_HEADER(stream, &header);
		MP4_PRINT_HEADER(header);
		if(header.size != 0 && left_size >= header.size)
		{
			left_size -= header.size;
		}
		else if(header.size == 0 && left_size >= header.large_size)
		{
			left_size -= header.large_size;
		}
		else
		{
			TRACE_WARNING("left_size:%llu, data_size:%d, data_large_size:%llu", left_size, header.size, header.large_size);
			break;
		}
		atom_box_handle_t* handle = find_atom_box_handle(header.type);
		new_node = handle->read_box(stream, &header);
		free(handle);
		if(node->child == NULL)
		{
			node->child = new_node;
		}
		else
		{
			tmp_node->sibling = new_node;
		}
		tmp_node = new_node;
		TRACE_LOG("left_size:%llu", left_size);
	}
	TRACE_FUNC_TAIL
}

atom_box_handle_t* find_atom_box_handle(uint32_t type)
{
	atom_box_handle_t* handle = (atom_box_handle_t*)calloc(1, sizeof(atom_box_handle_t));
	switch(type)
	{
	case FOURCC('f','t', 'y', 'p'):
		handle->read_box = mp4_read_box_ftyp;
		handle->free_box = mp4_free_box_ftyp;
		handle->print_box = mp4_print_box_ftyp;
		break;
	case FOURCC('f','r', 'e', 'e'):
		handle->read_box = mp4_read_box_free;
		handle->free_box = mp4_free_box_free;
		handle->print_box = mp4_print_box_free;
		break;
	case FOURCC('m','d', 'a', 't'):
		handle->read_box = mp4_read_box_mdat;
		handle->free_box = mp4_free_box_mdat;
		handle->print_box = mp4_print_box_mdat;
		break;
	case FOURCC('d', 'r','e', 'f'):
		handle->read_box = mp4_read_box_dref;
		handle->free_box = mp4_free_box_dref;
		handle->print_box = mp4_print_box_dref;
		break;
	case FOURCC('u', 'r','l', ' '):
		handle->read_box = mp4_read_box_url;
		handle->free_box = mp4_free_box_url;
		handle->print_box = mp4_print_box_url;
		break;
	case FOURCC('u', 'r','n', ' '):
		handle->read_box = mp4_read_box_urn;
		handle->free_box = mp4_free_box_urn;
		handle->print_box = mp4_print_box_urn;
		break;
	case FOURCC('m', 'v','h', 'd'):
		handle->read_box = mp4_read_box_mvhd;
		handle->free_box = mp4_free_box_mvhd;
		handle->print_box = mp4_print_box_mvhd;
		break;
	case FOURCC('t', 'k','h', 'd'):
		handle->read_box = mp4_read_box_tkhd;
		handle->free_box = mp4_free_box_tkhd;
		handle->print_box = mp4_print_box_tkhd;
		break;
	case FOURCC('e', 'l','s', 't'):
		handle->read_box = mp4_read_box_elst;
		handle->free_box = mp4_free_box_elst;
		handle->print_box = mp4_print_box_elst;
		break;
	case FOURCC('m', 'd','h', 'd'):
		handle->read_box = mp4_read_box_mdhd;
		handle->free_box = mp4_free_box_mdhd;
		handle->print_box = mp4_print_box_mdhd;
		break;
	case FOURCC('h', 'd','l', 'r'):
		handle->read_box = mp4_read_box_hdlr;
		handle->free_box = mp4_free_box_hdlr;
		handle->print_box = mp4_print_box_hdlr;
		break;
	case FOURCC('v', 'm','h', 'd'):
		handle->read_box = mp4_read_box_vmhd;
		handle->free_box = mp4_free_box_vmhd;
		handle->print_box = mp4_print_box_vmhd;
		break;
	case FOURCC('s', 't','s', 'd'):
		handle->read_box = mp4_read_box_stsd;
		handle->free_box = mp4_free_box_stsd;
		handle->print_box = mp4_print_box_stsd;
		break;
	case FOURCC('s', 't','t', 's'):
		handle->read_box = mp4_read_box_stts;
		handle->free_box = mp4_free_box_stts;
		handle->print_box = mp4_print_box_stts;
		break;
	case FOURCC('s', 't','s', 's'):
		handle->read_box = mp4_read_box_stss;
		handle->free_box = mp4_free_box_stss;
		handle->print_box = mp4_print_box_stss;
		break;
	case FOURCC('s', 't','s', 'c'):
		handle->read_box = mp4_read_box_stsc;
		handle->free_box = mp4_free_box_stsc;
		handle->print_box = mp4_print_box_stsc;
		break;
	case FOURCC('s', 't','s', 'z'):
		handle->read_box = mp4_read_box_stsz;
		handle->free_box = mp4_free_box_stsz;
		handle->print_box = mp4_print_box_stsz;
		break;
	case FOURCC('s', 't','c', 'o'):
		handle->read_box = mp4_read_box_stco;
		handle->free_box = mp4_free_box_stco;
		handle->print_box = mp4_print_box_stco;
		break;
	case FOURCC('c', 'o', '6', '4'):
		handle->read_box = mp4_read_box_co64;
		handle->free_box = mp4_free_box_co64;
		handle->print_box = mp4_print_box_co64;
		break;
	case FOURCC('s', 'm', 'h', 'd'):
		handle->read_box = mp4_read_box_smhd;
		handle->free_box = mp4_free_box_smhd;
		handle->print_box = mp4_print_box_smhd;
		break;
	case FOURCC('a', 'v','c', '1'):
		handle->read_box = mp4_read_box_sample_vide;
		handle->free_box = mp4_free_box_sample_vide;
		handle->print_box = mp4_print_box_sample_vide;
		break;
	case FOURCC('a', 'v','c', 'C'):
		handle->read_box = mp4_read_box_avcc;
		handle->free_box = mp4_free_box_avcc;
		handle->print_box = mp4_print_box_avcc;
		break;
	case FOURCC('s', 'r','a', 't'):
		handle->read_box = mp4_read_box_srat;
		handle->free_box = mp4_free_box_srat;
		handle->print_box = mp4_print_box_srat;
		break;
	case FOURCC('m', 'p','4', 'a'):
		handle->read_box = mp4_read_box_sample_soun;
		handle->free_box = mp4_free_box_sample_soun;
		handle->print_box = mp4_print_box_sample_soun;
		break;
	case FOURCC('m', 'e','t', 'a'):
		handle->read_box = mp4_read_box_meta;
		handle->free_box = mp4_free_box_meta;
		handle->print_box = mp4_print_box_meta;
		break;
	case FOURCC('m','o', 'o', 'v'):
	case FOURCC('t', 'r','a', 'k'):
	case FOURCC('e', 'd','t', 's'):
	case FOURCC('m', 'd','i', 'a'):
	case FOURCC('d', 'i','n', 'f'):
	case FOURCC('m', 'i','n', 'f'):
	case FOURCC('s', 't','b', 'l'):
	case FOURCC('i', 'l','s', 't'):
	case FOURCC('u', 'd','t', 'a'):
		handle->read_box = mp4_read_box_container;
		handle->free_box = mp4_free_box_container;
		handle->print_box = mp4_print_box_container;
		break;
	default:
		handle->read_box = mp4_read_box_unknown;
		handle->free_box = mp4_free_box_unknown;
		handle->print_box = mp4_print_box_unknown;
		break;
	}
	return handle;
}

mp4_box_node_t* mp4_read_box_container(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_ftyp_t);
	mp4_box_node_t* node = pack_mp4_box_node((atom_t*)box);
	mp4_read_box(stream, node, left_size);
	left_size -= left_size;

	MP4_EXIT_BOX();
	return node;
}

mp4_box_node_t* mp4_read_box_ftyp(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_ftyp_t);

	MP4_GET_FOURCC(stream, &box->major_brand);
	MP4_GET_4BYTES(stream, &box->minor_version);
	box->compatible_brands_count = left_size / sizeof(uint32_t);
	box->compatible_brands = (uint32_t*)calloc( box->compatible_brands_count, sizeof(uint32_t) );
	for(uint32_t i = 0; i < box->compatible_brands_count; i++)
	{
		MP4_GET_FOURCC(stream, (box->compatible_brands + i));
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_free(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_free_t);

	MP4_SKIP_DATA(stream, left_size);

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_mdat(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_mdat_t);
	MP4_SKIP_DATA(stream, left_size);
	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_mvhd(stream_t* stream, atom_t* header)
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
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_tkhd(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_tkhd_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);

	if(box->header.version == 0)
	{
		MP4_GET_4BYTES(stream, &box->creation_time);
		MP4_GET_4BYTES(stream, &box->modification_time);
		MP4_GET_4BYTES(stream, &box->track_id);
		MP4_GET_4BYTES(stream, &box->reserved);
		MP4_GET_4BYTES(stream, &box->duration);
	}
	else if(box->header.version == 1)
	{
		MP4_GET_8BYTES(stream, &box->creation_time);
		MP4_GET_8BYTES(stream, &box->modification_time);
		MP4_GET_4BYTES(stream, &box->track_id);
		MP4_GET_4BYTES(stream, &box->reserved);
		MP4_GET_8BYTES(stream, &box->duration);
	}
	else
	{
		TRACE_WARNING("Unknown tkhd version:%d", box->header.version);
	}
	MP4_GET_4BYTES(stream, &box->reserved2[0]);
	MP4_GET_4BYTES(stream, &box->reserved2[1]);
	MP4_GET_2BYTES(stream, &box->layer);
	MP4_GET_2BYTES(stream, &box->alternate_group);
	MP4_GET_2BYTES(stream, &box->volume);
	MP4_GET_2BYTES(stream, &box->reserved3);
	for(uint8_t i = 0; i < 9; i++)
	{
		MP4_GET_4BYTES(stream, &box->matrix[i]);
	}
	MP4_GET_4BYTES(stream, &box->width);
	MP4_GET_4BYTES(stream, &box->height);

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_dref(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_dref_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->entry_count);

	mp4_box_node_t* node = pack_mp4_box_node((atom_t*)box);
	mp4_read_box(stream, node, left_size);
	left_size -= left_size;

	MP4_EXIT_BOX();
	return node;
}

mp4_box_node_t* mp4_read_box_url(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_url_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_STRING(stream, box->location);
	
	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_urn(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_urn_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_STRING(stream, box->name);
	MP4_GET_STRING(stream, box->location);
	
	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_elst(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_elst_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->entry_count);

	box->entries = (atom_edit_content_t*)calloc(box->entry_count, sizeof(atom_edit_content_t));
	for(int i = 0; i < box->entry_count; i++)
	{
		if(box->header.version == 0)
		{
			MP4_GET_4BYTES(stream, &box->entries[i].segment_duration);
			MP4_GET_4BYTES(stream, &box->entries[i].media_time);
		}
		else if(box->header.version == 1)
		{
			MP4_GET_8BYTES(stream, &box->entries[i].segment_duration);
			MP4_GET_8BYTES(stream, &box->entries[i].media_time);
		}
		MP4_GET_2BYTES(stream, &box->entries[i].media_rate_integer);
		MP4_GET_2BYTES(stream, &box->entries[i].media_rate_fraction);
	}
	
	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_mdhd(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_mdhd_t);

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
		TRACE_WARNING("Unknown mdhd version:%d", box->header.version);
	}

	uint16_t tmp = 0;	
	MP4_GET_STREAM_BYTES(stream, &tmp, 2);
	box->pad = (tmp & 0x8000) >> 15;
	box->language[0] = (tmp & 0x7C00) >> 10;
	box->language[1] = (tmp & 0x3E0) >> 5;
	box->language[2] = tmp & 0x1F;
	MP4_GET_2BYTES(stream, &box->pre_defined);

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_hdlr(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_hdlr_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);

	MP4_GET_4BYTES(stream, &box->pre_defined);
	MP4_GET_FOURCC(stream, &box->handler_type);
	MP4_GET_4BYTES(stream, &box->reserved[0]);
	MP4_GET_4BYTES(stream, &box->reserved[1]);
	MP4_GET_4BYTES(stream, &box->reserved[2]);
	MP4_GET_STRING(stream, box->name);

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_vmhd(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_vmhd_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_2BYTES(stream, &box->graphicsmode);
	MP4_GET_2BYTES(stream, &box->opcolor[0]);
	MP4_GET_2BYTES(stream, &box->opcolor[1]);
	MP4_GET_2BYTES(stream, &box->opcolor[2]);

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_stsd(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_stsd_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);

	MP4_GET_4BYTES(stream, &box->entry_count);

	mp4_box_node_t* node = pack_mp4_box_node((atom_t*)box);
	mp4_read_box(stream, node, left_size);
	left_size -= left_size;

	MP4_EXIT_BOX();
	return node;
}

mp4_box_node_t* mp4_read_box_sample_vide(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_sample_vide_t);
	MP4_GET_STREAM_BYTES(stream, box->header.reserved, 6);
	MP4_GET_2BYTES(stream, &box->header.data_reference_index);
	MP4_GET_2BYTES(stream, &box->pre_defined);
	MP4_GET_2BYTES(stream, &box->reserved);
	MP4_GET_4BYTES(stream, &box->pre_defined2[0]);
	MP4_GET_4BYTES(stream, &box->pre_defined2[1]);
	MP4_GET_4BYTES(stream, &box->pre_defined2[2]);
	MP4_GET_2BYTES(stream, &box->width);
	MP4_GET_2BYTES(stream, &box->height);
	MP4_GET_4BYTES(stream, &box->horizresolution);
	MP4_GET_4BYTES(stream, &box->vertresolution);
	MP4_GET_4BYTES(stream, &box->reserved2);
	MP4_GET_2BYTES(stream, &box->frame_count);
	MP4_GET_STREAM_BYTES(stream, box->compressorname, 32);
	MP4_GET_2BYTES(stream, &box->depth);
	MP4_GET_2BYTES(stream, &box->pre_defined3);

	if(left_size)
	{
		mp4_box_node_t* node = pack_mp4_box_node((atom_t*)box);
		mp4_read_box(stream, node, left_size);
		left_size -= left_size;
		MP4_EXIT_BOX();
		return node;
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_sample_soun(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_sample_soun_t);
	MP4_GET_STREAM_BYTES(stream, box->header.reserved, 6);
	MP4_GET_2BYTES(stream, &box->header.data_reference_index);
	MP4_GET_4BYTES(stream, &box->reserved[0]);
	MP4_GET_4BYTES(stream, &box->reserved[1]);
	MP4_GET_2BYTES(stream, &box->channel_count);
	MP4_GET_2BYTES(stream, &box->sample_size);
	MP4_GET_2BYTES(stream, &box->pre_defined);
	MP4_GET_2BYTES(stream, &box->reserved2);
	MP4_GET_4BYTES(stream, &box->sample_rate);

	if(left_size)
	{
		mp4_box_node_t* node = pack_mp4_box_node((atom_t*)box);
		mp4_read_box(stream, node, left_size);
		left_size -= left_size;
		MP4_EXIT_BOX();
		return node;
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_sample_soun_v1(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_sample_soun_v1_t);
	MP4_GET_STREAM_BYTES(stream, box->header.reserved, 6);
	MP4_GET_2BYTES(stream, &box->header.data_reference_index);
	MP4_GET_2BYTES(stream, &box->entry_version);
	MP4_GET_4BYTES(stream, &box->reserved[0]);
	MP4_GET_4BYTES(stream, &box->reserved[1]);
	MP4_GET_4BYTES(stream, &box->reserved[2]);
	MP4_GET_2BYTES(stream, &box->channel_count);
	MP4_GET_2BYTES(stream, &box->sample_size);
	MP4_GET_2BYTES(stream, &box->pre_defined);
	MP4_GET_2BYTES(stream, &box->reserved2);
	MP4_GET_4BYTES(stream, &box->sample_rate);

	if(left_size)
	{
		mp4_box_node_t* node = pack_mp4_box_node((atom_t*)box);
		mp4_read_box(stream, node, left_size);
		left_size -= left_size;
		MP4_EXIT_BOX();
		return node;
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_avcc(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_avcc_t);
	MP4_GET_BYTE(stream, &box->configuration_version);
	MP4_GET_BYTE(stream, &box->avc_profile_indication);
	MP4_GET_BYTE(stream, &box->profile_compatibility);
	MP4_GET_BYTE(stream, &box->avc_level_indication);

	uint8_t	tmp = 0;
	MP4_GET_BYTE(stream, &tmp);
	box->reserved = (tmp & 0xFC) >> 2;
	box->length_size_minus_one = tmp & 0x3;
	MP4_GET_BYTE(stream, &tmp);
	box->reserved2 = (tmp & 0xE0) >> 5;
	box->num_of_sequence_parameter_sets = tmp & 0x1F;

	if(box->num_of_sequence_parameter_sets)
	{
		box->sequence_parameter_set_length = calloc(box->num_of_sequence_parameter_sets, sizeof(uint16_t));
		box->sequence_parameter_set_nal_unit = calloc(box->num_of_sequence_parameter_sets, sizeof(uint8_t*));
	}
	for(uint8_t i = 0; i < box->num_of_sequence_parameter_sets; i++)
	{
		MP4_GET_2BYTES(stream, &box->sequence_parameter_set_length[i]);
		box->sequence_parameter_set_nal_unit[i] = calloc(box->sequence_parameter_set_length[i], sizeof(uint8_t));
		MP4_GET_STREAM_BYTES(stream, box->sequence_parameter_set_nal_unit[i], box->sequence_parameter_set_length[i]);
	}
	MP4_GET_BYTE(stream, &box->num_of_picture_parameter_sets);
	if(box->num_of_picture_parameter_sets)
	{
		box->picture_parameter_set_length = calloc(box->num_of_picture_parameter_sets, sizeof(uint16_t));
		box->picture_parameter_set_nal_unit = calloc(box->num_of_picture_parameter_sets, sizeof(uint8_t*));
	}
	for(uint8_t i = 0; i < box->num_of_picture_parameter_sets; i++)
	{
		MP4_GET_2BYTES(stream, &box->picture_parameter_set_length[i]);
		box->picture_parameter_set_nal_unit[i] = calloc(box->picture_parameter_set_length[i], sizeof(uint8_t));
		MP4_GET_STREAM_BYTES(stream, box->picture_parameter_set_nal_unit[i], box->picture_parameter_set_length[i]);
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_stts(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_stts_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->entry_count);
	if(box->entry_count)
	{
		box->sample_count = calloc(box->entry_count, sizeof(uint32_t));
		box->sample_delta = calloc(box->entry_count, sizeof(uint32_t));
	}
	for(uint32_t i = 0; i < box->entry_count; i++)
	{
		MP4_GET_4BYTES(stream, &box->sample_count[i]);
		MP4_GET_4BYTES(stream, &box->sample_delta[i]);
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_stss(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_stss_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->entry_count);
	if(box->entry_count)
	{
		box->sample_number = calloc(box->entry_count, sizeof(uint32_t));
	}
	for(uint32_t i = 0; i < box->entry_count; i++)
	{
		MP4_GET_4BYTES(stream, &box->sample_number[i]);
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_stsc(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_stsc_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->entry_count);
	if(box->entry_count)
	{
		box->first_chunk = calloc(box->entry_count, sizeof(uint32_t));
		box->samples_per_chunk = calloc(box->entry_count, sizeof(uint32_t));
		box->sample_description_index = calloc(box->entry_count, sizeof(uint32_t));
	}
	for(uint32_t i = 0; i < box->entry_count; i++)
	{
		MP4_GET_4BYTES(stream, &box->first_chunk[i]);
		MP4_GET_4BYTES(stream, &box->samples_per_chunk[i]);
		MP4_GET_4BYTES(stream, &box->sample_description_index[i]);
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_stsz(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_stsz_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->sample_size);
	MP4_GET_4BYTES(stream, &box->sample_count);
	if(box->sample_size == 0 && box->sample_count)
	{
		box->entry_size = calloc(box->sample_count, sizeof(uint32_t));
		for(uint32_t i = 0; i < box->sample_count; i++)
		{
			MP4_GET_4BYTES(stream, &box->entry_size[i]);
		}
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_stco(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_stco_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->entry_count);
	if(box->entry_count)
	{
		box->chunk_offset = calloc(box->entry_count, sizeof(uint32_t));
	}
	for(uint32_t i = 0; i < box->entry_count; i++)
	{
		MP4_GET_4BYTES(stream, &box->chunk_offset[i]);
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_co64(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_co64_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->entry_count);
	if(box->entry_count)
	{
		box->chunk_offset = calloc(box->entry_count, sizeof(uint64_t));
	}
	for(uint32_t i = 0; i < box->entry_count; i++)
	{
		MP4_GET_8BYTES(stream, &box->chunk_offset[i]);
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_smhd(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_smhd_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_2BYTES(stream, &box->balance);
	MP4_GET_2BYTES(stream, &box->reserved);

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_srat(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_srat_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);
	MP4_GET_4BYTES(stream, &box->sampling_rate);

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_meta(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_meta_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);

	if(left_size)
	{
		mp4_box_node_t* node = pack_mp4_box_node((atom_t*)box);
		mp4_read_box(stream, node, left_size);
		left_size -= left_size;
		MP4_EXIT_BOX();
		return node;
	}

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_common(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_t);
	MP4_SKIP_DATA(stream, left_size);
	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

mp4_box_node_t* mp4_read_box_unknown(stream_t* stream, atom_t* header)
{
	MP4_ENTER_BOX(atom_t);
	MP4_SKIP_DATA(stream, left_size);
	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);
}

void mp4_free_box_container(atom_t* atom)
{
	free(atom);
}

void mp4_free_box_ftyp(atom_t* atom)
{
	atom_ftyp_t *ftyp = (atom_ftyp_t*)atom;
	free(ftyp->compatible_brands);
	free(ftyp);
}

void mp4_free_box_free(atom_t* atom)
{
	free((atom_free_t*)atom);
}

void mp4_free_box_mdat(atom_t* atom)
{
	free((atom_mdat_t*)atom);
}

void mp4_free_box_mvhd(atom_t* atom)
{
	atom_mvhd_t *mvhd = (atom_mvhd_t*)atom;
	free(mvhd);
}

void mp4_free_box_tkhd(atom_t* atom)
{
	atom_tkhd_t *tkhd = (atom_tkhd_t*)atom;
	free(tkhd);
}

void mp4_free_box_dref(atom_t* atom)
{
	free((atom_dref_t*)atom);
}

void mp4_free_box_stsd(atom_t* atom)
{
	free((atom_stsd_t*)atom);
}

void mp4_free_box_elst(atom_t* atom)
{
	atom_elst_t* elst = (atom_elst_t*)atom;
	free(elst->entries);
	free(elst);
}

void mp4_free_box_mdhd(atom_t* atom)
{
	free((atom_mdhd_t*)atom);
}

void mp4_free_box_hdlr(atom_t* atom)
{
	atom_hdlr_t* hdlr = (atom_hdlr_t*)atom;
	if(hdlr->name)free(hdlr->name);
	free(hdlr);
}

void mp4_free_box_url(atom_t* atom)
{
	atom_url_t* url = (atom_url_t*)atom;
	if(url->location)free(url->location);
	free(url);
}

void mp4_free_box_urn(atom_t* atom)
{
	atom_urn_t* urn = (atom_urn_t*)atom;
	if(urn->name)free(urn->name);
	if(urn->location)free(urn->location);
	free(urn);
}

void mp4_free_box_vmhd(atom_t* atom)
{
	free((atom_vmhd_t*)atom);
}

void mp4_free_box_sample_vide(atom_t* atom)
{
	free((atom_sample_vide_t*)atom);
}

void mp4_free_box_sample_soun(atom_t* atom)
{
	free((atom_sample_soun_t*)atom);
}

void mp4_free_box_sample_soun_v1(atom_t* atom)
{
	free((atom_sample_soun_v1_t*)atom);
}

void mp4_free_box_avcc(atom_t* atom)
{
	atom_avcc_t* avcc = (atom_avcc_t*)atom;
	for(uint8_t i = 0; i <  avcc->num_of_sequence_parameter_sets; i++)
	{
		if(avcc->sequence_parameter_set_nal_unit[i])free(avcc->sequence_parameter_set_nal_unit[i]);
	}
	if(avcc->sequence_parameter_set_length)free(avcc->sequence_parameter_set_length);
	if(avcc->sequence_parameter_set_nal_unit)free(avcc->sequence_parameter_set_nal_unit);

	for(uint8_t i = 0; i <  avcc->num_of_picture_parameter_sets; i++)
	{
		if(avcc->picture_parameter_set_nal_unit[i])free(avcc->picture_parameter_set_nal_unit[i]);
	}
	if(avcc->picture_parameter_set_length)free(avcc->picture_parameter_set_length);
	if(avcc)free(avcc);
}

void mp4_free_box_stts(atom_t* atom)
{
	atom_stts_t* stts = (atom_stts_t*)atom;
	if(stts->sample_count)free(stts->sample_count);
	if(stts->sample_delta)free(stts->sample_delta);
	if(stts)free(stts);
}

void mp4_free_box_stss(atom_t* atom)
{
	atom_stss_t* stss = (atom_stss_t*)atom;
	if(stss->sample_number)free(stss->sample_number);
	if(stss)free(stss);
}

void mp4_free_box_stsc(atom_t* atom)
{
	atom_stsc_t* stsc = (atom_stsc_t*)atom;
	if(stsc->first_chunk)free(stsc->first_chunk);
	if(stsc->samples_per_chunk)free(stsc->samples_per_chunk);
	if(stsc->sample_description_index)free(stsc->sample_description_index);
	if(stsc)free(stsc);
}

void mp4_free_box_stsz(atom_t* atom)
{
	atom_stsz_t* stsz = (atom_stsz_t*)atom;
	if(stsz->entry_size)free(stsz->entry_size);
	if(stsz)free(stsz);
}

void mp4_free_box_stco(atom_t* atom)
{
	atom_stco_t* stco = (atom_stco_t*)atom;
	if(stco->chunk_offset)free(stco->chunk_offset);
	if(stco)free(stco);
}

void mp4_free_box_co64(atom_t* atom)
{
	atom_co64_t* co64 = (atom_co64_t*)atom;
	if(co64->chunk_offset)free(co64->chunk_offset);
	if(co64)free(co64);
}

void mp4_free_box_smhd(atom_t* atom)
{
	atom_smhd_t* smhd = (atom_smhd_t*)atom;
	free(smhd);
}

void mp4_free_box_srat(atom_t* atom)
{
	atom_srat_t* srat = (atom_srat_t*)atom;
	free(srat);
}

void mp4_free_box_meta(atom_t* atom)
{
	atom_meta_t* meta = (atom_meta_t*)atom;
	free(meta);
}

void mp4_free_box_common(atom_t* atom)
{
	free(atom);
}

void mp4_free_box_unknown(atom_t* atom)
{
	free(atom);
}

void mp4_print_box_container(atom_t* atom)
{
	MP4_PRINT_HEADER((*atom));
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

void mp4_print_box_free(atom_t* atom)
{
	MP4_PRINT_HEADER((*atom));
}

void mp4_print_box_mdat(atom_t* atom)
{
	MP4_PRINT_HEADER((*atom));
}

void mp4_print_box_mvhd(atom_t* atom)
{
	atom_mvhd_t *mvhd = (atom_mvhd_t*)atom;
	MP4_PRINT_FULL_HEADER(mvhd->header);
	TRACE_LOG("creation_time:%llu", mvhd->creation_time);
	TRACE_LOG("modification_time:%llu", mvhd->modification_time);
	TRACE_LOG("timescale:%d", mvhd->timescale);
	TRACE_LOG("duration:%llu", mvhd->duration);
}

void mp4_print_box_tkhd(atom_t* atom)
{
	atom_tkhd_t *tkhd = (atom_tkhd_t*)atom;
	MP4_PRINT_FULL_HEADER(tkhd->header);
	TRACE_LOG("creation_time:%llu", tkhd->creation_time);
	TRACE_LOG("modification_time:%llu", tkhd->modification_time);
	TRACE_LOG("track id:%d", tkhd->track_id);
	TRACE_LOG("reserved:%d", tkhd->reserved);
	TRACE_LOG("duration:%llu", tkhd->duration);
	TRACE_LOG("reserved2[0]:%d, reserved2[1]:%d", tkhd->reserved2[0], tkhd->reserved2[1]);
	TRACE_LOG("layer:%d", tkhd->layer);
	TRACE_LOG("alternate_group:%d", tkhd->alternate_group);
	TRACE_LOG("volume:%d.%d", (tkhd->volume & 0xFF00)>>8, tkhd->volume & 0xFF);
	TRACE_LOG("reserved3:%d", tkhd->reserved3);
	TRACE_LOG("width:%d.%d", (tkhd->width & 0xFFFF0000)>>16, tkhd->width & 0xFFFF);
	TRACE_LOG("height:%d.%d", (tkhd->height & 0xFFFF0000)>>16, tkhd->height & 0xFFFF);
}

void mp4_print_box_dref(atom_t* atom)
{
	MP4_PRINT_HEADER((*atom));
	TRACE_LOG("entry_count:%d", ((atom_dref_t*)atom)->entry_count);
}

void mp4_print_box_stsd(atom_t* atom)
{
	atom_stsd_t *mdhd = (atom_stsd_t*)atom;
	MP4_PRINT_FULL_HEADER(mdhd->header);
	TRACE_LOG("entry_count:%d", mdhd->entry_count);
}

void mp4_print_box_elst(atom_t* atom)
{
	atom_elst_t *elst = (atom_elst_t*)atom;
	MP4_PRINT_FULL_HEADER(elst->header);
	TRACE_LOG("entry_count:%d", elst->entry_count);
	for(uint32_t i = 0 ; i < elst->entry_count; i++)
	{
		TRACE_LOG("entries[%d].segment_duration:%llu", i, elst->entries[i].segment_duration);
		TRACE_LOG("entries[%d].media_time:%lld", i, elst->entries[i].media_time);
		TRACE_LOG("entries[%d].media_rate:%d.%d", i, elst->entries[i].media_rate_integer, elst->entries[i].media_rate_fraction);
	}
}

void mp4_print_box_mdhd(atom_t* atom)
{
	atom_mdhd_t *mdhd = (atom_mdhd_t*)atom;
	MP4_PRINT_FULL_HEADER(mdhd->header);
	TRACE_LOG("creation_time:%llu", mdhd->creation_time);
	TRACE_LOG("modification_time:%llu", mdhd->modification_time);
	TRACE_LOG("timescale:%d", mdhd->timescale);
	TRACE_LOG("duration:%llu", mdhd->duration);
	TRACE_LOG("pad:%d", mdhd->pad);
	TRACE_LOG("language0:%u", mdhd->language[0]);
	TRACE_LOG("language1:%u", mdhd->language[1]);
	TRACE_LOG("language2:%u", mdhd->language[2]);
	TRACE_LOG("pre_defined:%u", mdhd->pre_defined);
}

void mp4_print_box_url(atom_t* atom)
{
	atom_url_t *url = (atom_url_t*)atom;
	MP4_PRINT_FULL_HEADER(url->header);
	TRACE_LOG("location:%s", url->location?url->location:"NULL");
}

void mp4_print_box_urn(atom_t* atom)
{
	atom_urn_t *urn = (atom_urn_t*)atom;
	MP4_PRINT_FULL_HEADER(urn->header);
	TRACE_LOG("name:%s", urn->name?urn->name:"NULL");
	TRACE_LOG("location:%s", urn->location?urn->location:"NULL");
}

void mp4_print_box_hdlr(atom_t* atom)
{
	atom_hdlr_t *hdlr = (atom_hdlr_t*)atom;
	MP4_PRINT_FULL_HEADER(hdlr->header);
	TRACE_LOG("pre_defined:%u", hdlr->pre_defined);
	MP4_PRINT_FOURCC("handler_type:", &hdlr->handler_type);
	TRACE_LOG("reserved[0]:%u", hdlr->reserved[0]);
	TRACE_LOG("reserved[1]:%u", hdlr->reserved[1]);
	TRACE_LOG("reserved[2]:%u", hdlr->reserved[2]);
	TRACE_LOG("name:%s", hdlr->name);
}

void mp4_print_box_vmhd(atom_t* atom)
{
	atom_vmhd_t *vmhd = (atom_vmhd_t*)atom;
	MP4_PRINT_FULL_HEADER(vmhd->header);
	TRACE_LOG("graphicsmode:%d", vmhd->graphicsmode);
	TRACE_LOG("opcolor[0]:%d", vmhd->opcolor[0]);
	TRACE_LOG("opcolor[1]:%d", vmhd->opcolor[1]);
	TRACE_LOG("opcolor[2]:%d", vmhd->opcolor[2]);
}

void mp4_print_box_sample_vide(atom_t* atom)
{
	atom_sample_vide_t* sample_video = (atom_sample_vide_t*)atom;
	MP4_PRINT_HEADER(sample_video->header.header);
	for(uint8_t i = 0; i < 6; i++)
	{
		TRACE_LOG("reserved[%d]=0x%x", i, sample_video->header.reserved[i]);
	}
	TRACE_LOG("data_reference_index:%d", sample_video->header.data_reference_index);
	TRACE_LOG("pre_defined:%u", sample_video->pre_defined);
	TRACE_LOG("reserved:%u", sample_video->reserved);
	TRACE_LOG("pre_defined2[0]:%u", sample_video->pre_defined2[0]);
	TRACE_LOG("pre_defined2[1]:%u", sample_video->pre_defined2[1]);
	TRACE_LOG("pre_defined2[2]:%u", sample_video->pre_defined2[2]);
	TRACE_LOG("width:%u", sample_video->width);
	TRACE_LOG("height:%u", sample_video->height);
	TRACE_LOG("horizresolution:%u", sample_video->horizresolution);
	TRACE_LOG("vertresolution:%u", sample_video->vertresolution);
	TRACE_LOG("reserved2:%u", sample_video->reserved2);
	TRACE_LOG("frame_count:%u", sample_video->frame_count);
	TRACE_LOG("compressorname:%s", sample_video->compressorname);
	TRACE_LOG("depth:%u", sample_video->depth);
	TRACE_LOG("pre_defined3:%d", sample_video->pre_defined3);
}

void mp4_print_box_sample_soun(atom_t* atom)
{
	atom_sample_soun_t* sample_soun = (atom_sample_soun_t*)atom;
	MP4_PRINT_HEADER(sample_soun->header.header);
	for(uint8_t i = 0; i < 6; i++)
	{
		TRACE_LOG("reserved[%d]=0x%x", i, sample_soun->header.reserved[i]);
	}
	TRACE_LOG("data_reference_index:%d", sample_soun->header.data_reference_index);
	TRACE_LOG("reserved[0]=%u, reserved[1]=%u", sample_soun->reserved[0], sample_soun->reserved[1]);
	TRACE_LOG("channel_count:%u", sample_soun->channel_count);
	TRACE_LOG("sample_size:%u", sample_soun->sample_size);
	TRACE_LOG("pre_defined:%u", sample_soun->pre_defined);
	TRACE_LOG("reserved2:%u", sample_soun->reserved2);
	TRACE_LOG("sample_rate:%u.%u", (sample_soun->sample_rate & 0xFFFF0000) >> 16, sample_soun->sample_rate & 0xFFFF);
}

void mp4_print_box_sample_soun_v1(atom_t* atom)
{
	atom_sample_soun_v1_t* sample_soun = (atom_sample_soun_v1_t*)atom;
	MP4_PRINT_HEADER(sample_soun->header.header);
	for(uint8_t i = 0; i < 6; i++)
	{
		TRACE_LOG("reserved[%d]=0x%x", i, sample_soun->header.reserved[i]);
	}
	TRACE_LOG("data_reference_index:%d", sample_soun->header.data_reference_index);
	TRACE_LOG("entry_version:%u", sample_soun->entry_version);
	TRACE_LOG("reserved[0]=%u, reserved[1]=%u, reserved[2]", sample_soun->reserved[0], sample_soun->reserved[1], sample_soun->reserved[2]);
	TRACE_LOG("channel_count:%u", sample_soun->channel_count);
	TRACE_LOG("sample_size:%u", sample_soun->sample_size);
	TRACE_LOG("pre_defined:%u", sample_soun->pre_defined);
	TRACE_LOG("reserved2:%u", sample_soun->reserved2);
	TRACE_LOG("sample_rate:%u.%u", (sample_soun->sample_rate & 0xFFFF0000) >> 16, sample_soun->sample_rate & 0xFFFF);
}

void mp4_print_box_avcc(atom_t* atom)
{
	atom_avcc_t* avcc = (atom_avcc_t*)atom;
	MP4_PRINT_HEADER(avcc->header);
	TRACE_LOG("configuration_version:%u", avcc->configuration_version);
	TRACE_LOG("avc_profile_indication:%u", avcc->avc_profile_indication);
	TRACE_LOG("profile_compatibility:%u", avcc->profile_compatibility);
	TRACE_LOG("avc_level_indication:%u", avcc->avc_level_indication);
	TRACE_LOG("reserved:%u", avcc->reserved);
	TRACE_LOG("length_size_minus_one:%u", avcc->length_size_minus_one);
	TRACE_LOG("reserved2:%u", avcc->reserved2);
	TRACE_LOG("num_of_sequence_parameter_sets:%u", avcc->num_of_sequence_parameter_sets);
	for(uint8_t i = 0; i < avcc->num_of_sequence_parameter_sets; i++)
	{
		TRACE_LOG("sequence_parameter_set_length[%u]=%u", i, avcc->sequence_parameter_set_length[i]);
		for(uint16_t j = 0; j < avcc->sequence_parameter_set_length[i]; j++)
		{
			TRACE_LOG("sequence_parameter_set_nal_unit[%u][%u] = %u", i, j, avcc->sequence_parameter_set_nal_unit[i][j]);
		}
	}
	TRACE_LOG("num_of_picture_parameter_sets:%u", avcc->num_of_picture_parameter_sets);
	for(uint8_t i = 0; i < avcc->num_of_picture_parameter_sets; i++)
	{
		TRACE_LOG("picture_parameter_set_length[%u]=%u", i, avcc->picture_parameter_set_length[i]);
		for(uint16_t j = 0; j < avcc->picture_parameter_set_length[i]; j++)
		{
			TRACE_LOG("picture_parameter_set_nal_unit[%u][%u] = %u", i, j, avcc->picture_parameter_set_nal_unit[i][j]);
		}
	}
}

void mp4_print_box_stts(atom_t* atom)
{
	atom_stts_t* stts = (atom_stts_t*)atom;

	MP4_PRINT_FULL_HEADER(stts->header);
	TRACE_LOG("entry_count:%u", stts->entry_count);
	for(uint32_t i = 0; i < stts->entry_count; i++)
	{
		TRACE_LOG("sample_count[%u]=%u, sample_delta[%u]=%u", i, stts->sample_count[i], i, stts->sample_delta[i]);
	}
}

void mp4_print_box_stss(atom_t* atom)
{
	atom_stss_t* stss = (atom_stss_t*)atom;
	MP4_PRINT_FULL_HEADER(stss->header);
	TRACE_LOG("entry_count:%u", stss->entry_count);
	for(uint32_t i = 0; i < stss->entry_count; i++)
	{
		TRACE_LOG("sample_number[%u]=%u", i, stss->sample_number[i]);
	}
}

void mp4_print_box_stsc(atom_t* atom)
{
	atom_stsc_t* stsc = (atom_stsc_t*)atom;
	MP4_PRINT_FULL_HEADER(stsc->header);
	TRACE_LOG("entry_count:%u", stsc->entry_count);
	for(uint32_t i = 0; i < stsc->entry_count && i < MAX_ARRAY_PRINT_SIZE; i++)
	{
		TRACE_LOG("first_chunk[%u]=%u", i, stsc->first_chunk[i]);
	}
	if(stsc->entry_count > MAX_ARRAY_PRINT_SIZE)
	{
		TRACE_LOG("first_chunk...");
	}
	for(uint32_t i = 0; i < stsc->entry_count && i < MAX_ARRAY_PRINT_SIZE; i++)
	{
		TRACE_LOG("samples_per_chunk[%u]=%u", i, stsc->samples_per_chunk[i]);
	}
	if(stsc->entry_count > MAX_ARRAY_PRINT_SIZE)
	{
		TRACE_LOG("samples_per_chunk...");
	}
	for(uint32_t i = 0; i < stsc->entry_count && i < MAX_ARRAY_PRINT_SIZE; i++)
	{
		TRACE_LOG("sample_description_index[%u]=%u", i, stsc->sample_description_index[i]);
	}
	if(stsc->entry_count > MAX_ARRAY_PRINT_SIZE)
	{
		TRACE_LOG("sample_description_index...");
	}
}

void mp4_print_box_stsz(atom_t* atom)
{
	atom_stsz_t* stsz = (atom_stsz_t*)atom;
	MP4_PRINT_FULL_HEADER(stsz->header);
	TRACE_LOG("sample_size:%u, sample_count:%u", stsz->sample_size, stsz->sample_count);
	if(stsz->sample_size == 0 && stsz->sample_count && stsz->entry_size)
	{
		for(uint32_t i = 0; i < stsz->sample_count && i < MAX_ARRAY_PRINT_SIZE; i++)
		{
			TRACE_LOG("entry_size[%u]=%u", i, stsz->entry_size[i]);
		}
		if( stsz->sample_count > MAX_ARRAY_PRINT_SIZE)
		{
			TRACE_LOG("entry_size...");
		}
	}
}

void mp4_print_box_stco(atom_t* atom)
{
	atom_stco_t* stco = (atom_stco_t*)atom;
	MP4_PRINT_FULL_HEADER(stco->header);
	TRACE_LOG("entry_count:%u", stco->entry_count);
	for(uint32_t i = 0; i < stco->entry_count && i < MAX_ARRAY_PRINT_SIZE; i++)
	{
		TRACE_LOG("chunk_offset[%u]=%u", i, stco->chunk_offset[i]);
	}
	if( stco->entry_count > MAX_ARRAY_PRINT_SIZE)
	{
		TRACE_LOG("chunk_offset...");
	}
}

void mp4_print_box_co64(atom_t* atom)
{
	atom_co64_t* co64 = (atom_co64_t*)atom;
	MP4_PRINT_FULL_HEADER(co64->header);
	TRACE_LOG("entry_count:%u", co64->entry_count);
	for(uint32_t i = 0; i < co64->entry_count && i < MAX_ARRAY_PRINT_SIZE; i++)
	{
		TRACE_LOG("chunk_offset[%u]=%llu", i, co64->chunk_offset[i]);
	}
	if( co64->entry_count > MAX_ARRAY_PRINT_SIZE)
	{
		TRACE_LOG("chunk_offset...");
	}
}

void mp4_print_box_smhd(atom_t* atom)
{
	atom_smhd_t* smhd = (atom_smhd_t*)atom;
	MP4_PRINT_FULL_HEADER(smhd->header);
	int8_t left = (smhd->balance & 0xFF00) >> 8;
	int8_t right = smhd->balance & 0xFF;
	TRACE_LOG("balance:%d.%d", left, right);
	TRACE_LOG("reserved:%u", smhd->reserved);
}

void mp4_print_box_srat(atom_t* atom)
{
	atom_srat_t* srat = (atom_srat_t*)atom;
	MP4_PRINT_FULL_HEADER(srat->header);
	TRACE_LOG("sampling_rate:%u", srat->sampling_rate);
}

void mp4_print_box_meta(atom_t* atom)
{
	atom_meta_t* meta = (atom_meta_t*)atom;
	MP4_PRINT_FULL_HEADER(meta->header);
}

void mp4_print_box_common(atom_t* atom)
{
	MP4_PRINT_HEADER((*atom));
}

void mp4_print_box_unknown(atom_t* atom)
{
	MP4_PRINT_HEADER((*atom));
}

mp4_box_node_t* pack_mp4_box_node(atom_t* data)
{
	mp4_box_node_t* node = (mp4_box_node_t*)calloc(1, sizeof(mp4_box_node_t));
	node->data = data;
	return node;
}

void show_mp4_box_tree(mp4_box_node_t* root)
{
	for(mp4_box_node_t* next = root->child; next != NULL; next = next->sibling)
	{
		atom_box_handle_t* handle = find_atom_box_handle(next->data->type);
		handle->print_box(next->data);
		free(handle);
		show_mp4_box_tree(next);
	}
}

void extract_mp4_track_data(mp4_box_node_t* root)
{
	for(mp4_box_node_t* next = root->child; next != NULL; next = next->sibling)
	{
		if(next->data->type == FOURCC('m','o','o','v'))
		{
			for(mp4_box_node_t* track = next->child; track != NULL; track = track->sibling)
			{
				if( track->data->type == FOURCC( 't', 'r', 'a', 'k' ) )
				{
					atom_tkhd_t* tkhd = (atom_tkhd_t*)find_atom(track, FOURCC('t', 'k', 'h', 'd'));//Exactly one in a trak
					atom_hdlr_t* hdlr = (atom_hdlr_t*)find_atom(track, FOURCC('h', 'd', 'l', 'r'));//Exactly one in a trak
					atom_stss_t* stss = (atom_stss_t*)find_atom(track, FOURCC('s', 't', 's', 's'));// zero or one
					atom_stts_t* stts = (atom_stts_t*)find_atom(track, FOURCC('s', 't', 't', 's'));//Exactly one
					atom_stco_t* stco = (atom_stco_t*)find_atom(track, FOURCC('s', 't', 'c', 'o'));// stco/co64 exactly one
					atom_co64_t* co64 = (atom_co64_t*)find_atom(track, FOURCC('c', 'o', '6', '4'));// stco/co64 exactly one
					atom_stsc_t* stsc = (atom_stsc_t*)find_atom(track, FOURCC('s', 't', 's', 'c'));//Exactly one
					atom_mdhd_t* mdhd = (atom_mdhd_t*)find_atom(track, FOURCC('m', 'd', 'h', 'd'));//Exactly one
					atom_stsz_t* stsz = (atom_stsz_t*)find_atom(track, FOURCC('s', 't', 's', 'z'));//stsz/stz2 Exactly one variant must be present
					if(hdlr && tkhd && stts && stsc && mdhd)
					{
						switch(hdlr->handler_type)
						{
						case FOURCC('v', 'i', 'd', 'e'):
							{
								video_frame_info_t* video_frames = (video_frame_info_t*)calloc(1, sizeof(video_frame_info_t));
								video_frames->header.type = hdlr->handler_type;
								video_frames->header.track_id = tkhd->track_id;
								if(stsz)
								{
									video_frames->header.frame_count = stsz->sample_count;
								}
								video_frames->width_integer = (tkhd->width & 0xFFFF0000) >> 16;
								video_frames->width_fraction = tkhd->width & 0xFFFFF;
								video_frames->height_integer = (tkhd->height & 0xFFFF0000) >> 16;
								video_frames->height_fraction = tkhd->height & 0xFFFF;
								if(video_frames->header.frame_count)
								{
									video_frames->frames = (frame_info_t*)calloc(video_frames->header.frame_count, sizeof(frame_info_t));
									if(stss)
									{
										for(uint32_t i = 0; i < stss->entry_count; i++)
										{
											video_frames->frames[stss->sample_number[i] - 1].key_frame = 1;
										}
									}
									else
									{
										for(uint32_t i = 0; i < video_frames->header.frame_count; i++)
										{
											video_frames->frames[i].key_frame = 1;
										}
									}
									for(uint32_t i = 0, j = 0; i < stts->entry_count; i++)
									{
										for(uint32_t k = 0; k < stts->sample_count[i]; k++)
										{
											video_frames->frames[j].pkt_duration = stts->sample_delta[i];
											video_frames->frames[j].pkt_duration_time = (stts->sample_delta[i] * 1.0 ) / mdhd->timescale;
											if(j > 0)
											{
												video_frames->frames[j].pkt_dts = video_frames->frames[j - 1].pkt_dts + stts->sample_delta[i];
												video_frames->frames[j].pkt_dts_time =  (video_frames->frames[j].pkt_dts * 1.0) / mdhd->timescale;
											}
											j++;
										}
									}
								}
								if(stsz && video_frames->frames)
								{
									if(stsz->sample_size)
									{
										for(uint32_t i = 0; i < video_frames->header.frame_count; i++)
										{
											video_frames->frames[i].pkt_size = stsz->sample_size;
										}
									}
									else
									{
										for(uint32_t i = 0; i < stsz->sample_count; i++)
										{
											video_frames->frames[i].pkt_size = stsz->entry_size[i];
										}
									}
								}
								if(stco)
								{
									for(uint32_t i = 0, sample_index = 0; i < stsc->entry_count; i++)
									{
										if( (i + 1) < stsc->entry_count)
										{
											for(uint32_t j = stsc->first_chunk[i]; j < stsc->first_chunk[i+1];j++)
											{
												video_frames->frames[sample_index++].pkt_offset = stco->chunk_offset[j - 1];
												for(uint32_t a = 1; a < stsc->samples_per_chunk[i]; a++)
												{
													video_frames->frames[sample_index].pkt_offset = video_frames->frames[sample_index - 1].pkt_offset + video_frames->frames[sample_index - 1].pkt_size;
													sample_index++;
												}
											}
										}
										else
										{
											for(uint32_t j = stsc->first_chunk[i]; j <= stco->entry_count; j++)
											{
												video_frames->frames[sample_index++].pkt_offset = stco->chunk_offset[j - 1];
												for(uint32_t a = 1; a < stsc->samples_per_chunk[i]; a++)
												{
													video_frames->frames[sample_index].pkt_offset = video_frames->frames[sample_index - 1].pkt_offset + video_frames->frames[sample_index - 1].pkt_size;
													sample_index++;
												}
												
											}
										}
									}
								}
								if(co64)
								{
									for(uint32_t i = 0, sample_index = 0; i < stsc->entry_count; i++)
									{
										if( (i + 1) < stsc->entry_count)
										{
											for(uint32_t j = stsc->first_chunk[i]; j < stsc->first_chunk[i+1];j++)
											{
												video_frames->frames[sample_index++].pkt_offset = co64->chunk_offset[j - 1];
												for(uint32_t a = 1; a < stsc->samples_per_chunk[i]; a++)
												{
													video_frames->frames[sample_index].pkt_offset = video_frames->frames[sample_index - 1].pkt_offset + video_frames->frames[sample_index - 1].pkt_size;
													sample_index++;
												}
											}
										}
										else
										{
											for(uint32_t j = stsc->first_chunk[i]; j <= co64->entry_count; j++)
											{
												video_frames->frames[sample_index++].pkt_offset = co64->chunk_offset[j - 1];
												for(uint32_t a = 1; a < stsc->samples_per_chunk[i]; a++)
												{
													video_frames->frames[sample_index].pkt_offset = video_frames->frames[sample_index - 1].pkt_offset + video_frames->frames[sample_index - 1].pkt_size;
													sample_index++;
												}
												
											}
										}
									}
								}
								show_frames(video_frames);
								free(video_frames->frames);
								free(video_frames);
								break;
							}
						case FOURCC('s', 'o', 'u', 'n'):
							{
								audio_frame_info_t* audio_frames = (audio_frame_info_t*)calloc(1, sizeof(audio_frame_info_t));
								audio_frames->header.type = hdlr->handler_type;
								audio_frames->header.track_id = tkhd->track_id;
								if(stsz)
								{
									audio_frames->header.frame_count = stsz->sample_count;
								}
								if(audio_frames->header.frame_count)
								{
									audio_frames->frames = (frame_info_t*)calloc(audio_frames->header.frame_count, sizeof(frame_info_t));
									if(stss)
									{
										for(uint32_t i = 0; i < stss->entry_count; i++)
										{
											audio_frames->frames[stss->sample_number[i] - 1].key_frame = 1;
										}
									}
									else
									{
										for(uint32_t i = 0; i < audio_frames->header.frame_count; i++)
										{
											audio_frames->frames[i].key_frame = 1;
										}
									}
									for(uint32_t i = 0, j = 0; i < stts->entry_count; i++)
									{
										for(uint32_t k = 0; k < stts->sample_count[i]; k++)
										{
											audio_frames->frames[j].pkt_duration = stts->sample_delta[i];
											audio_frames->frames[j].pkt_duration_time = (stts->sample_delta[i] * 1.0 ) / mdhd->timescale;
											if(j > 0)
											{
												audio_frames->frames[j].pkt_dts = audio_frames->frames[j - 1].pkt_dts + stts->sample_delta[i];
												audio_frames->frames[j].pkt_dts_time =  (audio_frames->frames[j].pkt_dts * 1.0) / mdhd->timescale;
											}
											j++;
										}
									}
								}
								if(stsz && audio_frames->frames)
								{
									if(stsz->sample_size)
									{
										for(uint32_t i = 0; i < audio_frames->header.frame_count; i++)
										{
											audio_frames->frames[i].pkt_size = stsz->sample_size;
										}
									}
									else
									{
										for(uint32_t i = 0; i < stsz->sample_count; i++)
										{
											audio_frames->frames[i].pkt_size = stsz->entry_size[i];
										}
									}
								}
								if(stco)
								{
									for(uint32_t i = 0, sample_index = 0; i < stsc->entry_count; i++)
									{
										if( (i + 1) < stsc->entry_count)
										{
											for(uint32_t j = stsc->first_chunk[i]; j < stsc->first_chunk[i+1];j++)
											{
												audio_frames->frames[sample_index++].pkt_offset = stco->chunk_offset[j - 1];
												for(uint32_t a = 1; a < stsc->samples_per_chunk[i]; a++)
												{
													audio_frames->frames[sample_index].pkt_offset = audio_frames->frames[sample_index - 1].pkt_offset + audio_frames->frames[sample_index - 1].pkt_size;
													sample_index++;
												}
											}
										}
										else
										{
											for(uint32_t j = stsc->first_chunk[i]; j <= stco->entry_count; j++)
											{
												audio_frames->frames[sample_index++].pkt_offset = stco->chunk_offset[j - 1];
												for(uint32_t a = 1; a < stsc->samples_per_chunk[i]; a++)
												{
													audio_frames->frames[sample_index].pkt_offset = audio_frames->frames[sample_index - 1].pkt_offset + audio_frames->frames[sample_index - 1].pkt_size;
													sample_index++;
												}
												
											}
										}
									}
								}
								if(co64)
								{
									for(uint32_t i = 0, sample_index = 0; i < stsc->entry_count; i++)
									{
										if( (i + 1) < stsc->entry_count)
										{
											for(uint32_t j = stsc->first_chunk[i]; j < stsc->first_chunk[i+1];j++)
											{
												audio_frames->frames[sample_index++].pkt_offset = co64->chunk_offset[j - 1];
												for(uint32_t a = 1; a < stsc->samples_per_chunk[i]; a++)
												{
													audio_frames->frames[sample_index].pkt_offset = audio_frames->frames[sample_index - 1].pkt_offset + audio_frames->frames[sample_index - 1].pkt_size;
													sample_index++;
												}
											}
										}
										else
										{
											for(uint32_t j = stsc->first_chunk[i]; j <= co64->entry_count; j++)
											{
												audio_frames->frames[sample_index++].pkt_offset = co64->chunk_offset[j - 1];
												for(uint32_t a = 1; a < stsc->samples_per_chunk[i]; a++)
												{
													audio_frames->frames[sample_index].pkt_offset = audio_frames->frames[sample_index - 1].pkt_offset + audio_frames->frames[sample_index - 1].pkt_size;
													sample_index++;
												}
												
											}
										}
									}
								}
								show_frames(audio_frames);
								free(audio_frames->frames);
								free(audio_frames);
								break;
							}
						default:
							TRACE_LOG("unexpected handler type:%x", hdlr->handler_type);
							break;
						}
					}
				}
			}
			break;
		}
	}
}

void show_frames(frame_head_t* frames)
{
	if(frames->type == FOURCC('v', 'i', 'd', 'e'))
	{
		video_frame_info_t* video_frames = (video_frame_info_t*)frames;
		TRACE_LOG("frame type:video");
		TRACE_LOG("track_id:%d", video_frames->header.track_id);
		TRACE_LOG("frame_count:%d", video_frames->header.frame_count);
		TRACE_LOG("width:%u.%u", video_frames->width_integer, video_frames->width_fraction);
		TRACE_LOG("height:%u.%u", video_frames->height_integer, video_frames->height_fraction);
		stream_t* r_stream = create_file_stream();
		stream_t* w_stream = create_file_stream();
		stream_open_file(r_stream, "1.mp4", "r");
		stream_open_file(w_stream, "1.h264", "w");
		for(uint32_t i = 0; i < video_frames->header.frame_count; i++)
		{
			TRACE_LOG(" frame index:%u", i);
			TRACE_LOG(" key_frame:%u", video_frames->frames[i].key_frame);
			TRACE_LOG(" pkt_pts:%llu", video_frames->frames[i].pkt_pts);
			TRACE_LOG(" pkt_pts_time:%f", video_frames->frames[i].pkt_pts_time);
			TRACE_LOG(" pkt_dts:%llu", video_frames->frames[i].pkt_dts);
			TRACE_LOG(" pkt_dts_time:%f", video_frames->frames[i].pkt_dts_time);
			TRACE_LOG(" pkt_duration:%llu", video_frames->frames[i].pkt_duration);
			TRACE_LOG(" pkt_duration_time:%f", video_frames->frames[i].pkt_duration_time);
			TRACE_LOG(" pkt_offset:%llu", video_frames->frames[i].pkt_offset);
			TRACE_LOG(" pkt_size:%llu", video_frames->frames[i].pkt_size);
			TRACE_LOG("                                                ");
			uint8_t* data = calloc(1, video_frames->frames[i].pkt_size);
			stream_seek(r_stream, video_frames->frames[i].pkt_offset, SEEK_SET);
			stream_read(r_stream, data, video_frames->frames[i].pkt_size);
			stream_write(w_stream, data, video_frames->frames[i].pkt_size);
			free(data);
		}
		stream_close(r_stream);
		stream_close(w_stream);
	}
	else if(frames->type == FOURCC('s', 'o', 'u', 'n'))
	{
		audio_frame_info_t* audio_frames = (audio_frame_info_t*)frames;
		TRACE_LOG("frame type:audio");
		TRACE_LOG("track_id:%d", audio_frames->header.track_id);
		TRACE_LOG("frame_count:%d", audio_frames->header.frame_count);
		TRACE_LOG("channel:%u", audio_frames->channel_count );
		stream_t* r_stream = create_file_stream();
		stream_t* w_stream = create_file_stream();
		stream_open_file(r_stream, "1.mp4", "r");
		stream_open_file(w_stream, "1.aac", "w");
		for(uint32_t i = 0; i < audio_frames->header.frame_count; i++)
		{
			TRACE_LOG(" frame index:%u", i);
			TRACE_LOG(" key_frame:%u", audio_frames->frames[i].key_frame);
			TRACE_LOG(" pkt_pts:%llu", audio_frames->frames[i].pkt_pts);
			TRACE_LOG(" pkt_pts_time:%f", audio_frames->frames[i].pkt_pts_time);
			TRACE_LOG(" pkt_dts:%llu", audio_frames->frames[i].pkt_dts);
			TRACE_LOG(" pkt_dts_time:%f", audio_frames->frames[i].pkt_dts_time);
			TRACE_LOG(" pkt_duration:%llu", audio_frames->frames[i].pkt_duration);
			TRACE_LOG(" pkt_duration_time:%f", audio_frames->frames[i].pkt_duration_time);
			TRACE_LOG(" pkt_offset:%llu", audio_frames->frames[i].pkt_offset);
			TRACE_LOG(" pkt_size:%llu", audio_frames->frames[i].pkt_size);
			TRACE_LOG("                                                ");
			uint8_t* data = calloc(1, audio_frames->frames[i].pkt_size);
			stream_seek(r_stream, audio_frames->frames[i].pkt_offset, SEEK_SET);
			stream_read(r_stream, data, audio_frames->frames[i].pkt_size);
			stream_write(w_stream, data, audio_frames->frames[i].pkt_size);
			free(data);
		}
		stream_close(r_stream);
		stream_close(w_stream);
	}
}

atom_t* find_atom(mp4_box_node_t* root, uint32_t type)
{
	if(root == NULL)return NULL;
	for(mp4_box_node_t* next = root->child; next != NULL; next = next->sibling)
	{
		if(next->data->type == type)
		{
			return next->data;
		}
		atom_t* data = find_atom(next, type);
		if(data)return data;
	}
	return NULL;
}

void free_mp4_box_tree(mp4_box_node_t* root)
{
	if(root == NULL)return;

	for(mp4_box_node_t* next = root->child; next != NULL; )
	{
		mp4_box_node_t* tmp = next;
		next = next->sibling;
		free_mp4_box_tree(next);
		atom_box_handle_t* handle = find_atom_box_handle(tmp->data->type);
		handle->free_box(tmp->data);
		free(tmp);
		free(handle);
	}
}
