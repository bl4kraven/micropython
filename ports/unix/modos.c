/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2018 Paul Sokolovsky
 * Copyright (c) 2017-2022 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "py/runtime.h"
#include "py/mphal.h"

static mp_obj_t os_pidfile(mp_obj_t path_name)
{
	const char *str_path_name = mp_obj_str_get_str(path_name);
	int r = 0;
	int fd = -1;
	struct flock fl = {
		.l_type = F_WRLCK,
		.l_start = 0,
		.l_whence = SEEK_SET,
		.l_len = 0,
	};

    MP_THREAD_GIL_EXIT();
	r = open(str_path_name, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (r >= 0)
	{
		fd = r;
		r = fcntl(fd, F_SETLK, &fl);
		if (r >= 0)
		{
			char buf[16];
			r = ftruncate(fd, 0);
			if (r >= 0)
			{
				snprintf(buf, 16, "%ld\n", (long)getpid());
				r = write(fd, buf, strlen(buf));
			}
		}
	}
    MP_THREAD_GIL_ENTER();

    RAISE_ERRNO(r, errno);

	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_pidfile_obj, os_pidfile);

static mp_obj_t os_mknod(mp_obj_t path_name)
{
	const char *str_path_name = mp_obj_str_get_str(path_name);
	int r;

    MP_THREAD_GIL_EXIT();
	r = mknod(str_path_name, 0600, 0);
    MP_THREAD_GIL_ENTER();

    RAISE_ERRNO(r, errno);

	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(os_mknod_obj, os_mknod);

static mp_obj_t path_isdir(mp_obj_t obj_path)
{
    struct stat sb;
    const char *path = mp_obj_str_get_str(obj_path);
	int r;

    MP_THREAD_GIL_EXIT();
    r = stat(path, &sb);
    MP_THREAD_GIL_ENTER();

	return (r != -1 && S_ISDIR(sb.st_mode))?mp_const_true: mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_isdir_fun_obj, path_isdir);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(path_isdir_obj, MP_ROM_PTR(&path_isdir_fun_obj));

static mp_obj_t path_join(mp_obj_t first, mp_obj_t second) 
{
	char path_name[64];

	if (strlen(mp_obj_str_get_str(first)) + \
			strlen(mp_obj_str_get_str(second)) + 1 >= 64)
        mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("string too long"));

	strncpy(path_name, mp_obj_str_get_str(first), 64-1);
	path_name[64-1] = '\0';

	if (path_name[strlen(path_name)-1] != '/')
		strncat(path_name, "/", 64-1-strlen(path_name));

	strncat(path_name, mp_obj_str_get_str(second), 64-1-strlen(path_name));
	return mp_obj_new_str(path_name, strlen(path_name));
}
static MP_DEFINE_CONST_FUN_OBJ_2(path_join_fun_obj, path_join);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(path_join_obj, MP_ROM_PTR(&path_join_fun_obj));

static mp_obj_t path_dirname(mp_obj_t path_name)
{
	char *cp = NULL;
	char dirname_buffer[64];

	if (strlen(mp_obj_str_get_str(path_name)) >= 64)
        mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("string too long"));

    strncpy(dirname_buffer, mp_obj_str_get_str(path_name), 64-1);
	dirname_buffer[64-1] = '\0';
    if ((cp = strrchr(dirname_buffer, '/')) == NULL)
        dirname_buffer[0] = 0;
    else
        *cp = 0;

    return mp_obj_new_str(dirname_buffer, strlen(dirname_buffer));
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_dirname_fun_obj, path_dirname);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(path_dirname_obj, MP_ROM_PTR(&path_dirname_fun_obj));
 
static mp_obj_t path_splitext(mp_obj_t path_name)
{
	const char *cp = NULL;
	const char *p_path_name = mp_obj_str_get_str(path_name);
	mp_obj_t tuple[2];
	if ((cp = strrchr(p_path_name, '.')) == NULL)
	{
		tuple[0] = path_name;
		tuple[1] = mp_obj_new_str("", 0);
	}
	else
	{
		tuple[0] = mp_obj_new_str(p_path_name, cp-p_path_name);
		tuple[1] = mp_obj_new_str(cp+1, strlen(cp+1));
	}
	return mp_obj_new_tuple(2, tuple);
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_splitext_fun_obj, path_splitext);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(path_splitext_obj, MP_ROM_PTR(&path_splitext_fun_obj));
 
static mp_obj_t path_exists(mp_obj_t path_name)
{
    struct stat sb;
    const char *path = mp_obj_str_get_str(path_name);

    MP_THREAD_GIL_EXIT();
	int r = stat(path, &sb);
    MP_THREAD_GIL_ENTER();

	return (r == 0)?mp_const_true: mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_1(path_exists_fun_obj, path_exists);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(path_exists_obj, MP_ROM_PTR(&path_exists_fun_obj));
 
static const mp_rom_map_elem_t os_path_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_join), MP_ROM_PTR(&path_join_obj) },
    { MP_ROM_QSTR(MP_QSTR_dirname), MP_ROM_PTR(&path_dirname_obj) },
    { MP_ROM_QSTR(MP_QSTR_splitext), MP_ROM_PTR(&path_splitext_obj) },
    { MP_ROM_QSTR(MP_QSTR_exists), MP_ROM_PTR(&path_exists_obj) },
    { MP_ROM_QSTR(MP_QSTR_isdir), MP_ROM_PTR(&path_isdir_obj) },
};
static MP_DEFINE_CONST_DICT(mp_os_path_locals_dict, os_path_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
	mp_type_os_path,
	MP_QSTR_PATH,
	MP_TYPE_FLAG_NONE,
	locals_dict, &mp_os_path_locals_dict
);

static mp_obj_t mp_os_getenv(size_t n_args, const mp_obj_t *args) {
    const char *s = getenv(mp_obj_str_get_str(args[0]));
    if (s == NULL) {
        if (n_args == 2) {
            return args[1];
        }
        return mp_const_none;
    }
    return mp_obj_new_str_from_cstr(s);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_getenv_obj, 1, 2, mp_os_getenv);

static mp_obj_t mp_os_putenv(mp_obj_t key_in, mp_obj_t value_in) {
    const char *key = mp_obj_str_get_str(key_in);
    const char *value = mp_obj_str_get_str(value_in);
    int ret;

    #if _WIN32
    ret = _putenv_s(key, value);
    #else
    ret = setenv(key, value, 1);
    #endif

    if (ret == -1) {
        mp_raise_OSError(errno);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_putenv_obj, mp_os_putenv);

static mp_obj_t mp_os_unsetenv(mp_obj_t key_in) {
    const char *key = mp_obj_str_get_str(key_in);
    int ret;

    #if _WIN32
    ret = _putenv_s(key, "");
    #else
    ret = unsetenv(key);
    #endif

    if (ret == -1) {
        mp_raise_OSError(errno);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_unsetenv_obj, mp_os_unsetenv);

static mp_obj_t mp_os_system(mp_obj_t cmd_in) {
    const char *cmd = mp_obj_str_get_str(cmd_in);

    MP_THREAD_GIL_EXIT();
    int r = system(cmd);
    MP_THREAD_GIL_ENTER();

    RAISE_ERRNO(r, errno);

    return MP_OBJ_NEW_SMALL_INT(r);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_system_obj, mp_os_system);

static mp_obj_t mp_os_urandom(mp_obj_t num) {
    mp_int_t n = mp_obj_get_int(num);
    vstr_t vstr;
    vstr_init_len(&vstr, n);
    mp_hal_get_random(n, vstr.buf);
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_urandom_obj, mp_os_urandom);

static mp_obj_t mp_os_errno(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return MP_OBJ_NEW_SMALL_INT(errno);
    }

    errno = mp_obj_get_int(args[0]);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_errno_obj, 0, 1, mp_os_errno);
