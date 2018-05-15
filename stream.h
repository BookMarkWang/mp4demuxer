#ifndef __STREAM_H__
#define __STREAM_H__

#define ERR_STREAM_OK 0
#define ERR_STREAM_IS_NULL (-1)
#define ERR_STREAM_FILE_NOT_OPEN (-2)
#define ERR_STREAM_SYSTEM_ERROR (-3)

typedef struct _stream stream_t;

stream_t* create_file_stream();
void destroy_file_stream(stream_t* stream);

int stream_open_file(stream_t* stream, const char* file_path, const char*  mode);
int stream_read(stream_t *stream, void* buf, int size);
int stream_write(stream_t *stream, void *buf, int size);
int64_t stream_seek(stream_t *stream, int64_t offset, int whence);
int stream_close(stream_t *stream);

#endif //__STREAM_H__
