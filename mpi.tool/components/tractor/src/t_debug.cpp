#ifdef _TRACTOR_DIAG_
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <stdio.h>
#include <tractor.h>
#if 0
void print_backtrace() {
    unw_cursor_t cursor;
    unw_context_t context;
    
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);
    
    int frame = 0;
    
    while (unw_step(&cursor) > 0) {
        unw_word_t ip;
        char sym[256] = {0};
        unw_word_t offset = 0;
        
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_proc_name(&cursor, sym, sizeof(sym), &offset);
        
        Dl_info info;
        dladdr((void *)ip, &info);
        
        unw_word_t relative_ip = ip - (unw_word_t)info.dli_fbase;
        
        debug_printf("#%d: %s ", frame, sym);
        
        if (info.dli_fname) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd),
                     "addr2line -e %s -f -p 0x%lx 2>/dev/null",
                     info.dli_fname, relative_ip);
            
            FILE *fp = popen(cmd, "r");
            if (fp) {
                char line[256];
                if (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\n")] = 0;
                    debug_printf("at %s", line);
                }
                pclose(fp);
            }
        }
        debug_printf("\n");
        frame++;
    }
}
#else
void print_backtrace() {
    unw_cursor_t cursor;
    unw_context_t context;
    
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);
    
    int frame_num = 0;
    
    while (unw_step(&cursor) > 0) {
        unw_word_t ip, sp;
        char sym[256];
        unw_word_t offset = 0;
        
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        
        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
            debug_printf("#%d: %s (ip=0x%lx, sp=0x%lx, offset=0x%lx)\n",
                   frame_num, sym, ip, sp, offset);
        } else {
            debug_printf("#%d: [unknown] (ip=0x%lx, sp=0x%lx)\n",
                   frame_num, ip, sp);
        }
        
        frame_num++;
    }}
#endif 

#endif //_TRACTOR_DIAG_
