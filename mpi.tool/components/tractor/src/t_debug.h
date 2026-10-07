#ifdef _TRACTOR_DIAG_
void print_backtrace();
#else
static void print_backtrace() {}
#endif //_TRACTOR_DIAG_
