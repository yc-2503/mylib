#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include "log.h"

#define EX_SINGLE_LOG_SIZE 1024
#define MAX_PATH_LEN 60
extern void add_handle(log_t **l);
extern void remove_handle(log_t **l);
static void _update_file(log_t *lh);

#if !defined(S_ISREG)
#define S_ISREG(x) (((x)&S_IFMT) == S_IFREG)
#endif

static uint64_t _get_file_size(const char *fullfilepath)
{
	int ret;
	struct stat sbuf;
	ret = stat(fullfilepath, &sbuf);
	if (ret || !S_ISREG(sbuf.st_mode))
		return 0;
	return (uint64_t)sbuf.st_size;
}

static void _log_lock(log_t *l)
{
	pthread_mutex_lock(&l->mutex);
}

static void _log_unlock(log_t *l)
{
	pthread_mutex_unlock(&l->mutex);
}

static int _iobuf_init(log_t *lh, size_t io_ms)
{
	lh->io_cap = io_ms;
	lh->io_buf = calloc(1, lh->io_cap);
	if (!lh->io_buf)
		return 1;
	return 0;
}

static int _file_init(log_t *lh, const char *lp, size_t ms, size_t mb)
{
	lh->max_file_size = ms;
	lh->file_path = strdup(lp); //strdup内部申请了动态内存，用完需要free
	if (!lh->file_path)
		return 1;
	lh->cur_file_size = _get_file_size(lp);
	lh->cur_bak_num = 0;
	lh->max_bak_num = mb;
	return 0;
}

int _chk_path(const char *log_file_path)
{
	int len = strlen(log_file_path) + 1;
	assert(len <= MAX_PATH_LEN);
	char path[MAX_PATH_LEN];
	strcpy(path, log_file_path);

	int status;
	for (int i = 0; i < len; i++)
	{
		if (path[i] == '/')
		{
			path[i] = '\0';
			if (access(path, F_OK) != 0)
			{
				status = mkdir(path, 0777);
				if (status)
					return status;
			}
			path[i] = '/';
		}
	}
	return 0;
}

log_t *log_create(const char *log_file_path, size_t max_file_size, size_t max_file_bak, size_t max_iobuf_size)
{
	assert(log_file_path);
	_chk_path(log_file_path);
	log_t *l = calloc(1, sizeof(log_t));
	if (!l)
		return NULL;

	int ret;
	ret = _file_init(l, log_file_path, max_file_size, max_file_bak);
	//printf("you are here %s %d %s \n",__FILE__,__LINE__,l->file_path);
	if (ret != 0)
	{
		free(l);
		return NULL;
	}

	ret = _iobuf_init(l, max_iobuf_size);
	if (ret != 0)
	{
		free(l->file_path);
		free(l);
		return NULL;
	}

	ret = pthread_mutex_init(&l->mutex, NULL);
	assert(ret == 0);

	add_handle(&l);

	return l;
}

static void _lock_uninit(log_t *l)
{
	pthread_mutex_destroy(&l->mutex);
}

void log_destory(log_t *lh)
{
	if (lh == NULL)
		return;

	_log_lock(lh);

	_update_file(lh);

	free(lh->io_buf);
	free(lh->wkey);

	remove_handle(&lh);

	_log_unlock(lh);

	_lock_uninit(lh);

	free(lh);
	lh = NULL;
}

static void _backup_file(log_t *lh)
{
	char new_filename[PATH_MAX] = {0};

	sprintf(new_filename, "%s.bak%lu", lh->file_path, lh->cur_bak_num % lh->max_bak_num);
	rename(lh->file_path, new_filename);
}

static void _update_file(log_t *lh)
{
	if (!lh->f_log)
		return;

	fclose(lh->f_log);
	lh->f_log = NULL;
	if (lh->max_bak_num == 0)
	{ //without backup
		remove(lh->file_path);
	}
	else
	{
		_backup_file(lh);
	}
	lh->cur_bak_num++;
	lh->cur_file_size = 0;
}

void log_flush(log_t *lh)
{
	if (lh == NULL)
		return;

	_log_lock(lh);

	_update_file(lh);

	_log_unlock(lh);
}

static int _file_handle_request(log_t *lh)
{
	if (lh->f_log)
		return 0;

	lh->f_log = fopen(lh->file_path, "a+");

	if (!lh->f_log)
		return 1;

	if (setvbuf(lh->f_log, lh->io_buf, _IOFBF, lh->io_cap) != 0) //设定文件流的缓冲区
		return 1;

	return 0;
}

static void _write_buf(log_t *lh, char *msg, size_t len)
{

	_log_lock(lh);

	if (0 != _file_handle_request(lh))
	{
		fprintf(stderr, "Failed to open the file, path: %s\n", lh->file_path);
		return;
	}

	size_t print_size = fwrite(msg, sizeof(char), len, lh->f_log);
	if (print_size != len)
	{
		fprintf(stderr, "Failed to write the file path(%s), len(%lu)\n", lh->file_path, print_size);
		return;
	}
	lh->cur_file_size += print_size;

	if (lh->cur_file_size > lh->max_file_size)
	{
		_update_file(lh);
	}

	_log_unlock(lh);
}

void _log_write(log_t *lh, const log_level_t level, const char *format, ...)
{
	//printf("i am here\n");
	if (lh == NULL)
	{
		fprintf(stderr, "logger handle is null!\n");
		return;
	}

	assert(format != NULL);

	char log_msg[EX_SINGLE_LOG_SIZE];
	memset(log_msg, 0, EX_SINGLE_LOG_SIZE);

	va_list args;
	va_start(args, format);

	static const char *const severity[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};

/* debug message */
#if (LOG_LEVEL == LOG_DEBUG)
	time_t rawtime = time(NULL);
	struct tm timeinfo;
	localtime_r(&rawtime, &timeinfo);
	pid_t tid = syscall(SYS_gettid);
	int info_len = snprintf(log_msg, SINGLE_LOG_SIZE, "%s%04d-%02d-%02d %02d:%02d:%02d %s<%s> %d: ", severity[level], timeinfo.tm_year + 1900,
							timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, va_arg(args, char *), va_arg(args, char *), tid);
#else
	int info_len = strlen(severity[level]);
	strncpy(log_msg, severity[level], info_len);
#endif

	int total_len = vsnprintf(log_msg + info_len, SINGLE_LOG_SIZE - info_len, format, args);
	va_end(args);

	if (total_len <= 0 || info_len <= 0 || total_len > SINGLE_LOG_SIZE - info_len)
	{
		fprintf(stderr, "Failed to vsnprintf a text entry: (total_len) %d\n", total_len);
		return;
	}

	total_len += info_len;
	//	printf("you are here %s %d \n",__FILE__,__LINE__);
	_write_buf(lh, log_msg, total_len);
    //printf("%s\n",log_msg);
	//printf("you are here %s %d \n",__FILE__,__LINE__);
}
