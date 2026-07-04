#include "mod_list.h"
#include <string.h>
#include <stdlib.h>

static GPtrArray *mod_list = NULL;

// GPtrArray 元素释放函数：释放 ModInfo 内存
static void mod_info_free(gpointer data)
{
    ModInfo *info = (ModInfo *)data;
    free(info);
}

void mod_list_init(void)
{
    if (!mod_list) {
        mod_list = g_ptr_array_new_with_free_func(mod_info_free);
    }
}

GPtrArray *mod_list_get_all(void)
{
    return mod_list;
}

void mod_list_add(const ModInfo *info)
{
    if (!mod_list) mod_list_init();
    ModInfo *copy = g_new(ModInfo, 1);
    memcpy(copy, info, sizeof(ModInfo));
    g_ptr_array_add(mod_list, copy);
}

void mod_list_clear(void)
{
    if (mod_list) {
        g_ptr_array_set_size(mod_list, 0);
    }
}

int mod_list_count(void)
{
    return mod_list ? (int)mod_list->len : 0;
}

ModInfo *mod_list_get(int index)
{
    if (!mod_list || index < 0 || index >= (int)mod_list->len)
        return NULL;
    return (ModInfo *)g_ptr_array_index(mod_list, index);
}

int mod_list_update_count(void)
{
    int count = 0;
    for (int i = 0; i < mod_list_count(); i++) {
        ModInfo *mod = mod_list_get(i);
        if (mod && mod->status == MOD_STATUS_UPDATE_AVAIL)
            count++;
    }
    return count;
}

void mod_list_remove_at(int index)
{
    if (!mod_list || index < 0 || index >= (int)mod_list->len)
        return;
    g_ptr_array_remove_index(mod_list, index);
}

void mod_list_free(void)
{
    if (mod_list) {
        g_ptr_array_free(mod_list, TRUE);
        mod_list = NULL;
    }
}
