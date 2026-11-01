/* Keep Shell and Workbench launches off the classic 4 KiB default stack. */
extern "C"
{
    unsigned long __stack = 4UL * 1024UL * 1024UL;
}
