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
	case FOURCC('m','o', 'o', 'v'):
	case FOURCC('t', 'r','a', 'k'):
	case FOURCC('e', 'd','t', 's'):
	case FOURCC('m', 'd','i', 'a'):
	case FOURCC('d', 'i','n', 'f'):
	case FOURCC('m', 'i','n', 'f'):
	case FOURCC('s', 't','b', 'l'):
	//case FOURCC('s', 't','s', 'd'):
	case FOURCC('a', 'v','c', '1'):
	case FOURCC('m', 'p','4', 'a'):
	case FOURCC('m', 'e','t', 'a'):
	case FOURCC('i', 'l','s', 't'):
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

mp4_box_node_t* mp4_read_box_stsd(stream_t* stream, atom_t* header)
{
	/*MP4_ENTER_BOX(atom_stsd_t);

	MP4_GET_BYTE(stream, &box->header.version);
	MP4_GET_STREAM_BYTES(stream, box->header.flags, 3);

	MP4_EXIT_BOX();
	return pack_mp4_box_node((atom_t*)box);*/return NULL;
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
	MP4_PRINT_HEADER((*atom));
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
