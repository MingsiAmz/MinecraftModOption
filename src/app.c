#include "app.h"
#include "network.h"
#include "mod_list.h"
#include <string.h>

static AppState app_state;

AppState *app_get_state(void)
{
    return &app_state;
}

int app_init(void)
{
    memset(&app_state, 0, sizeof(AppState));

    // 加载配置
    if (config_load(&app_state.config) != 0) {
        config_set_defaults(&app_state.config);
    }

    // 初始化网络
    if (network_init() != 0) {
        g_warning("网络初始化失败");
    }

    // 初始化模组列表
    mod_list_init();

    return 0;
}

void app_cleanup(void)
{
    mod_list_free();
    network_cleanup();
}
