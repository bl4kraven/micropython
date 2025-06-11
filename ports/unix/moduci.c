#include <string.h>
#include <stdlib.h>

#include <uci.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/mperrno.h"

static struct uci_context *ctx = NULL; 
static char *g_dup_key = NULL;

// uci的key在uci_lookup_ptr中会被修改，并且在调用完所有uci api后才能释放
// 所以为简单起见，作为全局变量，每次调用前都需要释放
static char *get_dup_key(mp_obj_t obj_key)
{
    if (g_dup_key != NULL)
    {
        free(g_dup_key);
        g_dup_key = NULL;
    }

    const char *key = mp_obj_str_get_str(obj_key);
    g_dup_key = strdup(key);
    return g_dup_key;
}

static void uci_init()
{
    if (ctx != NULL)
        return;

    ctx = uci_alloc_context();
    if (!ctx) 
        mp_raise_OSError(MP_EIO);
}

static mp_obj_t mp_uci_free()
{
    if (ctx != NULL)
    {
        uci_free_context(ctx);
        ctx = NULL;
    }

    if (g_dup_key != NULL)
    {
        free(g_dup_key);
        g_dup_key = NULL;
    }

	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_uci_free_obj, mp_uci_free);

static mp_obj_t mp_uci_get_value(mp_obj_t obj_key)
{
    uci_init();

    struct uci_ptr ptr;
    char *key_dup = get_dup_key(obj_key);
    if (uci_lookup_ptr(ctx, &ptr, key_dup, true) == UCI_OK)
    {
        struct uci_element *e = ptr.last;
        if ((ptr.flags & UCI_LOOKUP_COMPLETE) && e->type == UCI_TYPE_OPTION)
        {
            if (ptr.o->type == UCI_TYPE_STRING)
            {
                return mp_obj_new_str(ptr.o->v.string, strlen(ptr.o->v.string));
            }
            else if (ptr.o->type == UCI_TYPE_LIST)
            {
                mp_obj_t list_obj = mp_obj_new_list(0, NULL);
                struct uci_element *_e;
                uci_foreach_element(&ptr.o->v.list, _e)
                {
                    mp_obj_list_append(list_obj, mp_obj_new_str(_e->name, strlen(_e->name)));
                }
                return list_obj;
            }
        }
    }
    mp_raise_ValueError(MP_ERROR_TEXT("uci_get_value fail"));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_uci_get_value_obj, mp_uci_get_value);

static mp_obj_t mp_uci_get_section(mp_obj_t obj_key)
{
    uci_init();

	struct uci_ptr ptr;
    char *key_dup = get_dup_key(obj_key);
	if (uci_lookup_ptr(ctx, &ptr, key_dup, true) == UCI_OK)
    {
        struct uci_element *e = ptr.last;
        if ((ptr.flags & UCI_LOOKUP_COMPLETE) && 
            e->type == UCI_TYPE_SECTION)
        {
            mp_obj_t dict_obj = mp_obj_new_dict(0);
            uci_foreach_element(&ptr.s->options, e)
            {
                struct uci_option *o = uci_to_option(e);
				if (o->type == UCI_TYPE_STRING)
                {
                    mp_obj_dict_store(dict_obj, mp_obj_new_str(o->e.name, strlen(o->e.name)), mp_obj_new_str(o->v.string, strlen(o->v.string)));
                }
				else if (o->type == UCI_TYPE_LIST)
				{
					struct uci_element *_e;
                    mp_obj_t list_obj = mp_obj_new_list(0, NULL);
					uci_foreach_element(&o->v.list, _e)
					{
                        mp_obj_list_append(list_obj, mp_obj_new_str(_e->name, strlen(_e->name)));
					}
                    mp_obj_dict_store(dict_obj, mp_obj_new_str(o->e.name, strlen(o->e.name)), list_obj);
				}
                else
                {
                    mp_raise_ValueError(MP_ERROR_TEXT("uci_get_value value type only support str and list"));
                }
            }
            return dict_obj;
        }
    }
    mp_raise_ValueError(MP_ERROR_TEXT("uci_get_section fail"));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_uci_get_section_obj, mp_uci_get_section);

static mp_obj_t mp_uci_get_package(mp_obj_t obj_key)
{
    uci_init();

	struct uci_ptr ptr;
    char *key_dup = get_dup_key(obj_key);
	if (uci_lookup_ptr(ctx, &ptr, key_dup, true) == UCI_OK)
    {
        struct uci_element *e = ptr.last;
        if ((ptr.flags & UCI_LOOKUP_COMPLETE) && 
            e->type == UCI_TYPE_PACKAGE)
        {
            mp_obj_t list_obj = mp_obj_new_list(0, NULL);
            uci_foreach_element(&ptr.p->sections, e)
            {
                struct uci_section *s = uci_to_section(e);
                mp_obj_list_append(list_obj, mp_obj_new_str(s->e.name, strlen(s->e.name)));
            }
            return list_obj;
        }
    }
    mp_raise_ValueError(MP_ERROR_TEXT("uci_get_package fail"));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_uci_get_package_obj, mp_uci_get_package);

static mp_obj_t mp_uci_set_value(mp_obj_t obj_key, mp_obj_t obj_value)
{
    uci_init();

	struct uci_ptr ptr;
    char *key_dup = get_dup_key(obj_key);
	if (uci_lookup_ptr(ctx, &ptr, key_dup, true) == UCI_OK)
    {
        if (mp_obj_is_str(obj_value))
        {
            const char *value = mp_obj_str_get_str(obj_value);
            ptr.value = value;
            if (uci_set(ctx, &ptr) == UCI_OK &&
                uci_save(ctx, ptr.p) == UCI_OK)
            {
                return mp_const_none;
            }
        }
        else if (mp_obj_is_type(obj_value, &mp_type_list))
        {
            size_t list_len;
            mp_obj_t *list_items;
            mp_obj_list_get(obj_value, &list_len, &list_items);
            for (size_t i = 0; i < list_len; i++ )
            {
                const char *value = mp_obj_str_get_str(list_items[i]);
                ptr.value = value;

                if (uci_add_list(ctx, &ptr) != UCI_OK ||
                    uci_save(ctx, ptr.p) != UCI_OK)
                {
                    mp_raise_ValueError(MP_ERROR_TEXT("uci_set_value fail"));
                }
            }
            return mp_const_none;
        }
        else
        {
            mp_raise_ValueError(MP_ERROR_TEXT("uci_set_value value type only support str and list"));
        }
    }
    mp_raise_ValueError(MP_ERROR_TEXT("uci_set_value fail"));
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_uci_set_value_obj, mp_uci_set_value);

static mp_obj_t mp_uci_del_key(mp_obj_t obj_key) 
{
    uci_init();

	struct uci_ptr ptr;
    char *key_dup = get_dup_key(obj_key);
	if (uci_lookup_ptr(ctx, &ptr, key_dup, true) == UCI_OK)
    {
        // 只支持删除value
        struct uci_element *e = ptr.last;
        if (e->type == UCI_TYPE_OPTION &&
            uci_delete(ctx, &ptr) == UCI_OK &&
            uci_save(ctx, ptr.p) == UCI_OK)
        {
            return mp_const_none;
        }
    }
    mp_raise_ValueError(MP_ERROR_TEXT("uci_del_key fail"));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_uci_del_key_obj, mp_uci_del_key);

static mp_obj_t mp_uci_commit_value(mp_obj_t obj_package)
{
    uci_init();

	struct uci_ptr ptr;
    char *package_dup = get_dup_key(obj_package);
	if (uci_lookup_ptr(ctx, &ptr, package_dup, true) == UCI_OK &&
        uci_commit(ctx, &ptr.p, false) == UCI_OK) 
    {
        return mp_const_none;
    }

    mp_raise_ValueError(MP_ERROR_TEXT("uci_commit_value fail"));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_uci_commit_value_obj, mp_uci_commit_value);

static mp_obj_t mp_uci_clear_buffer()
{
	if (ctx)
	{
		uci_free_context(ctx);
		ctx = uci_alloc_context();
	}
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_uci_clear_buffer_obj, mp_uci_clear_buffer);

static const mp_rom_map_elem_t uci_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_uci) },
    { MP_ROM_QSTR(MP_QSTR_get_value), MP_ROM_PTR(&mp_uci_get_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_section), MP_ROM_PTR(&mp_uci_get_section_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_package), MP_ROM_PTR(&mp_uci_get_package_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_value), MP_ROM_PTR(&mp_uci_set_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_del_key), MP_ROM_PTR(&mp_uci_del_key_obj) },
    { MP_ROM_QSTR(MP_QSTR_commit_value), MP_ROM_PTR(&mp_uci_commit_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_free), MP_ROM_PTR(&mp_uci_free_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear_buffer), MP_ROM_PTR(&mp_uci_clear_buffer_obj) },
};

static MP_DEFINE_CONST_DICT(uci_module_globals, uci_module_globals_table);

const mp_obj_module_t mp_module_uci = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&uci_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_uci, mp_module_uci);