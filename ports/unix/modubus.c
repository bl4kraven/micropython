#include <stdio.h>
#include <stdint.h>

#include <libubox/blobmsg_json.h>
#include <libubus.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/mperrno.h"

#ifndef UBUS_UNIX_SOCKET
#define UBUS_UNIX_SOCKET "/var/run/ubus/ubus.sock"
#endif

struct ubus_context *ctx = NULL;
#define CONNECTED (ctx != NULL)

static void dispose_connection()
{
	if (ctx != NULL)
	{
		ubus_free(ctx);
		ctx = NULL;
	}
}

static mp_obj_t mp_ubus_connect()
{
	if (CONNECTED)
        mp_raise_OSError(MP_EBUSY);

	// Connect to ubus
	ctx = ubus_connect(UBUS_UNIX_SOCKET);
	if (!ctx) 
        mp_raise_OSError(MP_EIO);

	return mp_const_true;
}

static MP_DEFINE_CONST_FUN_OBJ_0(mp_ubus_connect_obj, mp_ubus_connect);

static mp_obj_t mp_ubus_disconnect()
{
	if (!CONNECTED)
        mp_raise_OSError(MP_EBUSY);

	// Connect to ubus
	ctx = ubus_connect(UBUS_UNIX_SOCKET);
	if (!ctx) 
        mp_raise_OSError(MP_EIO);

	dispose_connection();
	return mp_const_true;
}

static MP_DEFINE_CONST_FUN_OBJ_0(mp_ubus_disconnect_obj, mp_ubus_disconnect);

static void mp_ubus_python_call_handler(struct ubus_request *req, int type, struct blob_attr *msg)
{
	mp_obj_t list_result = (mp_obj_t)req->priv;

	if (!msg)
		return;

	// convert message do python json object
	char *str = blobmsg_format_json(msg, true);
	if (!str)
		return;

    mp_obj_t json_module = mp_import_name(MP_QSTR_json, mp_const_none, MP_OBJ_NEW_SMALL_INT(0));
    mp_obj_t json_loads = mp_load_attr(json_module, MP_QSTR_loads);
    mp_obj_t res_obj = mp_call_function_1(json_loads,  mp_obj_new_str(str, strlen(str)));

	// append to results
	mp_obj_list_append(list_result, res_obj);
}

/*
* Calls object's method on ubus.
*:param obj_object: name of the object
*:param obj_method: name of the method
*:param obj_arguments: arguments of the method (should be JSON serialisable)
*:param obj_timeout: timeout in ms (0 = wait forever)
*/
static mp_obj_t mp_ubus_call(size_t n_args, const mp_obj_t *args) 
{
	if (!CONNECTED)
        mp_raise_OSError(MP_EIO);

	mp_obj_t obj_object = args[0];
	mp_obj_t obj_method = args[1];
	mp_obj_t obj_arguments = args[2];
	mp_obj_t obj_timeout = args[3];

    const char *str_object = mp_obj_str_get_str(obj_object);
    const char *str_method = mp_obj_str_get_str(obj_method);
	mp_int_t n_timeout = mp_obj_get_int(obj_timeout);

	if (n_timeout < 0)
		mp_raise_ValueError(MP_ERROR_TEXT("invalid timeout value"));


	uint32_t id = 0;
	int retval = ubus_lookup_id(ctx, str_object, &id);
	if (retval != UBUS_STATUS_OK)
		mp_raise_ValueError(MP_ERROR_TEXT("ubus_lookup_id fail"));

    mp_obj_t json_module = mp_import_name(MP_QSTR_json, mp_const_none, MP_OBJ_NEW_SMALL_INT(0));
    mp_obj_t json_dumps = mp_load_attr(json_module, MP_QSTR_dumps);
    mp_obj_t json_str_obj = mp_call_function_1(json_dumps, obj_arguments);

	// put data into buffer
	struct blob_buf python_buf;
	memset(&python_buf, 0, sizeof(python_buf));
	blob_buf_init(&python_buf, 0);
	if (!blobmsg_add_json_from_string(&python_buf, mp_obj_str_get_str(json_str_obj)))
	{
		blob_buf_free(&python_buf);
		mp_raise_ValueError(MP_ERROR_TEXT("blobmsg_add_json_from_string fail"));
	}

	mp_obj_t list_results = mp_obj_new_list(0, NULL);
	retval = ubus_invoke(ctx, id, str_method, python_buf.head, mp_ubus_python_call_handler, list_results, n_timeout);
	if (retval != UBUS_STATUS_OK)
	{
		blob_buf_free(&python_buf);
		if (retval == UBUS_STATUS_TIMEOUT)
			mp_raise_OSError(MP_ETIMEDOUT);
		else
			mp_raise_ValueError(MP_ERROR_TEXT("ubus_invoke fail"));
	}

	blob_buf_free(&python_buf);
	// Note that results might be NULL indicating that something went wrong in the handler
	return list_results;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_ubus_call_obj, 4, 4, mp_ubus_call);

static const mp_rom_map_elem_t ubus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ubus) },
    { MP_ROM_QSTR(MP_QSTR_disconnect), MP_ROM_PTR(&mp_ubus_disconnect_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&mp_ubus_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_call), MP_ROM_PTR(&mp_ubus_call_obj) },
};

static MP_DEFINE_CONST_DICT(ubus_module_globals, ubus_module_globals_table);

const mp_obj_module_t mp_module_ubus = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ubus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ubus, mp_module_ubus);
