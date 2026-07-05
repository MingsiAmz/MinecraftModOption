#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/scanner.h"
#include "src/mod_list.h"
#include <gtk/gtk.h>

int main() {
    mod_list_init();
    
    const char *test_dir = "C:\\Custom\\Games\\Minecraft\\Server\\PhantomTech-1.21.1-Server\\mods";
    int found = scanner_scan_directory(test_dir, mod_list_get_all());
    printf("scanner_scan_directory returned: %d\n", found);
    printf("mod_list_count: %d\n", mod_list_count());
    
    for (int i = 0; i < mod_list_count() && i < 5; i++) {
        ModInfo *m = mod_list_get(i);
        if (m) printf("  [%d] name=%s mod_id=%s status=%d\n", i, m->name, m->mod_id, m->status);
    }
    
    if (mod_list_count() > 5) {
        printf("  ... and %d more\n", mod_list_count() - 5);
        // 打印最后几个
        for (int i = mod_list_count() - 3; i < mod_list_count(); i++) {
            ModInfo *m = mod_list_get(i);
            if (m) printf("  [%d] name=%s mod_id=%s\n", i, m->name, m->mod_id);
        }
    }
    
    mod_list_free();
    return 0;
}
