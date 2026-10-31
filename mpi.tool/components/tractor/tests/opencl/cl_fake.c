/* Minimal stand-in for an OpenCL library: the functions the test calls, taking some time and doing nothing else.
 * Tractor wraps the real entry points with dlsym(RTLD_NEXT), so it needs a library behind it; this one lets the
 * transfer / execution / sync classification be checked on a machine without an OpenCL device. */
#include <CL/cl.h>
#include <unistd.h>

#define NOTHING(us) (usleep(us), CL_SUCCESS)

/* every enqueue that is asked for an event gets a distinct made-up handle */
static unsigned long event_counter = 0x6000;
#define GIVE_EVENT(e) ((e) ? (*(e) = (cl_event)(event_counter += 0x10), 0) : 0)

cl_int clEnqueueWriteBuffer(cl_command_queue q, cl_mem b, cl_bool blocking, size_t offset, size_t size, const void *ptr, cl_uint n, const cl_event *wl, cl_event *e)
{
    GIVE_EVENT(e);
    return NOTHING(200);
}
cl_int clEnqueueReadBuffer(cl_command_queue q, cl_mem b, cl_bool blocking, size_t offset, size_t size, void *ptr, cl_uint n, const cl_event *wl, cl_event *e)
{
    GIVE_EVENT(e);
    return NOTHING(200);
}
cl_int clEnqueueCopyBuffer(cl_command_queue q, cl_mem s, cl_mem d, size_t so, size_t dof, size_t size, cl_uint n, const cl_event *wl, cl_event *e)
{
    GIVE_EVENT(e);
    return NOTHING(150);
}
cl_int clEnqueueSVMMemcpy(cl_command_queue q, cl_bool blocking, void *dst, const void *src, size_t size, cl_uint n, const cl_event *wl, cl_event *e)
{
    GIVE_EVENT(e);
    return NOTHING(150);
}
cl_int clEnqueueFillBuffer(cl_command_queue q, cl_mem b, const void *pattern, size_t psize, size_t offset, size_t size, cl_uint n, const cl_event *wl, cl_event *e)
{
    GIVE_EVENT(e);
    return NOTHING(100);
}
cl_int clEnqueueSVMMemFill(cl_command_queue q, void *ptr, const void *pattern, size_t psize, size_t size, cl_uint n, const cl_event *wl, cl_event *e)
{
    GIVE_EVENT(e);
    return NOTHING(100);
}
cl_int clEnqueueNDRangeKernel(cl_command_queue q, cl_kernel k, cl_uint dim, const size_t *off, const size_t *gws, const size_t *lws, cl_uint n, const cl_event *wl, cl_event *e)
{
    GIVE_EVENT(e);
    return NOTHING(400);
}
cl_int clEnqueueTask(cl_command_queue q, cl_kernel k, cl_uint n, const cl_event *wl, cl_event *e)
{
    GIVE_EVENT(e);
    return NOTHING(300);
}
cl_int clFinish(cl_command_queue q)
{
    return NOTHING(250);
}
cl_int clWaitForEvents(cl_uint n, const cl_event *list)
{
    return NOTHING(100);
}
cl_int clBuildProgram(cl_program p, cl_uint n, const cl_device_id *devs, const char *opts, void (*notify)(cl_program, void *), void *user)
{
    return NOTHING(500);
}

/* queues get distinct handles; the properties are ignored */
cl_command_queue clCreateCommandQueueWithProperties(cl_context c, cl_device_id d, const cl_queue_properties *p, cl_int *err)
{
    static unsigned long queue_counter = 0x1000;
    if (err)
        *err = CL_SUCCESS;
    return (cl_command_queue)(queue_counter += 0x100);
}
cl_int clReleaseCommandQueue(cl_command_queue q)
{
    return CL_SUCCESS;
}
