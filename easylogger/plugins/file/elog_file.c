/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015-2019, Qintl, <qintl_linux@163.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Save log to file.
 * Created on: 2019-01-05
 */

 #define LOG_TAG    "elog.file"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#if (_WIN32||_WIN64)
#include <windows.h>
#endif

#include "elog_file.h"

#ifdef ELOG_FILE_ENABLE

/* initialize OK flag */
static bool init_ok = false;
static FILE *fp = NULL;
static ElogFileCfg local_cfg;

#ifndef DEFALUT_LOG_FILE_PATH
#define DEFALUT_LOG_FILE_PATH "./logs"
#endif
void elog_setFilePath(const char* filepath, size_t len)
{
	if (!filepath)
	{
		sprintf(local_cfg.path, DEFALUT_LOG_FILE_PATH);
	}
	else
	{
		if (len < sizeof(local_cfg.path) / sizeof(local_cfg.path[0]))
		{
			strncpy(local_cfg.path, filepath, 127);
		}
	}
}

ElogErrCode elog_file_init(void)
{
	ElogErrCode result = ELOG_NO_ERR;
	ElogFileCfg cfg;

	if (init_ok)
		goto __exit;

	elog_file_port_init();
	if (strlen(local_cfg.path) == 0)
	{
		sprintf(local_cfg.path, DEFALUT_LOG_FILE_PATH);
	}

	char filename[256] = { 0 };
#if (_WIN32||_WIN64) 
	SYSTEMTIME sys = { 0 };
	GetLocalTime(&sys);
	if (access(local_cfg.path, 0) == -1)
	{
		mkdir(local_cfg.path);
	}
	snprintf(filename, 255, "%s/log_%4d-%02d-%02d.log", local_cfg.path, sys.wYear, sys.wMonth, sys.wDay);
#else
	struct tm tm = { 0 };
	struct timeval tv = { 0 };
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	if (access(local_cfg.path, 0) == -1)
	{
		mkdir(local_cfg.path, 0775);
	}
	snprintf(filename, 255, "%s/log_%4d-%02d-%02d.log", local_cfg.path, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
#endif
	
	strncpy(cfg.path, local_cfg.path, 255);
	strncpy(cfg.name, filename, 255);
	cfg.max_size = ELOG_FILE_MAX_SIZE;
	cfg.max_rotate = ELOG_FILE_MAX_ROTATE;

	elog_file_config(&cfg);

	init_ok = true;
__exit:
	return result;
}

/*
 * rotate the log file xxx.log.n-1 => xxx.log.n, and xxx.log => xxx.log.0
 */
static bool elog_file_rotate(void)
{
#define SUFFIX_LEN                     10
    /* mv xxx.log.n-1 => xxx.log.n, and xxx.log => xxx.log.0 */
    int n, err = 0;
    char oldpath[256]= {0}, newpath[256] = {0};
    size_t base = strlen(local_cfg.name);
    bool result = true;
    FILE *tmp_fp;

    memcpy(oldpath, local_cfg.name, base);
    memcpy(newpath, local_cfg.name, base);

    fclose(fp);

    for (n = local_cfg.max_rotate - 1; n >= 0; --n) {
        snprintf(oldpath + base, SUFFIX_LEN, n ? ".%d" : "", n - 1);
        snprintf(newpath + base, SUFFIX_LEN, ".%d", n);
        /* remove the old file */
        if ((tmp_fp = fopen(newpath , "r")) != NULL) {
            fclose(tmp_fp);
            remove(newpath);
        }
        /* change the new log file to old file name */
        if ((tmp_fp = fopen(oldpath , "r")) != NULL) {
            fclose(tmp_fp);
            err = rename(oldpath, newpath);
        }

        if (err < 0) {
            result = false;
            goto __exit;
        }
    }

__exit:
    /* reopen the file */
    fp = fopen(local_cfg.name, "a+");

    return result;
}


/* add static fun compare Data*/
static void filecompData()
{
	char filename[256] = { 0 };
#if (_WIN32||_WIN64)
	SYSTEMTIME sys = { 0 };
	GetLocalTime(&sys);
	snprintf(filename, 255, "%s/log_%4d-%02d-%02d.log",
		local_cfg.path, sys.wYear, sys.wMonth, sys.wDay);
#else
	struct tm tm = { 0 };
	struct timeval tv = { 0 };
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	snprintf(filename, 255, "%s/log_%4d-%02d-%02d.log",
		local_cfg.path, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
#endif // (_WIN32||_WIN64)
	
	if (strcmp(filename, local_cfg.name) == 0)
	{
		return;
	}

	fclose(fp);
	strncpy(local_cfg.name, filename, strlen(filename));
	fp = fopen(filename, "a+");
	return;
}

void elog_file_write(const char* log, size_t size)
{
	size_t file_size = 0;

	ELOG_ASSERT(init_ok);
	ELOG_ASSERT(log);

	elog_file_port_lock();

	fseek(fp, 0L, SEEK_END);
	file_size = ftell(fp);

	/*重新写入新的日志时间*/
	filecompData();

	if (unlikely(file_size > local_cfg.max_size)) {
#if ELOG_FILE_MAX_ROTATE > 0
		if (!elog_file_rotate()) {
			goto __exit;
		}
#else
		goto __exit;
#endif
	}

	fwrite(log, size, 1, fp);

#ifdef ELOG_FILE_FLUSH_CACHE_ENABLE
	fflush(fp);
#endif

__exit:
	elog_file_port_unlock();
}

void elog_file_deinit(void)
{
    ELOG_ASSERT(init_ok);

    ElogFileCfg cfg = {NULL, 0, 0};

    elog_file_config(&cfg);

    elog_file_port_deinit();

    init_ok = false;
}

void elog_file_config(ElogFileCfg *cfg)
{
    elog_file_port_lock();

    if (fp) {
        fclose(fp);
        fp = NULL;
    }

	if (cfg != NULL) {
		if (cfg->path != NULL && strlen(cfg->path) > 0)
			strncpy(local_cfg.path, cfg->path, sizeof(local_cfg.path) - 1);
		if (cfg->name != NULL && strlen(cfg->name) > 0)
			strncpy(local_cfg.name, cfg->name, sizeof(local_cfg.name) - 1);
        local_cfg.max_size = cfg->max_size;
        local_cfg.max_rotate = cfg->max_rotate;

        if (local_cfg.name != NULL && strlen(local_cfg.name) > 0)
            fp = fopen(local_cfg.name, "a+");
    }

    elog_file_port_unlock();
}

#endif /* ELOG_FILE_ENABLE */
