/* OpenACC profiling interface simulator + MPI. The executable exports acc_prof_register()/acc_prof_unregister()
 * like an OpenACC runtime does and raises the device events itself, so the transfer / execution / wait
 * classification can be checked without an accelerator (a host-only libgomp raises none of them).
 * Needs no -fopenacc, only acc_prof.h.
 *   acc_sim_test [iterations] */
#include <acc_prof.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define UPLOAD_BYTES (1 << 20)
#define DOWNLOAD_BYTES (1 << 18)

static acc_prof_callback callbacks[acc_ev_last];

void acc_prof_register(acc_event_t event, acc_prof_callback cb, acc_register_t how)
{
    callbacks[event] = cb;
}
void acc_prof_unregister(acc_event_t event, acc_prof_callback cb, acc_register_t how)
{
    callbacks[event] = NULL;
}

static int raised;

static void raise_event(acc_event_t event, size_t bytes, int gangs, int workers, int vector, acc_construct_t parent)
{
    static char host[16], device[16];
    acc_prof_info prof = {0};
    acc_event_info info = {0};
    acc_api_info api = {0};
    if (callbacks[event] == NULL)
        return;
    prof.event_type = event;
    prof.valid_bytes = _ACC_PROF_INFO_VALID_BYTES;
    prof.version = _ACC_PROF_INFO_VERSION;
    prof.device_type = acc_device_nvidia;
    prof.async = prof.async_queue = 3;
    info.other_event.event_type = event;
    info.other_event.parent_construct = parent;
    info.other_event.valid_bytes = _ACC_OTHER_EVENT_INFO_VALID_BYTES;
    if (bytes) {
        info.data_event.bytes = bytes;
        info.data_event.host_ptr = host;
        info.data_event.device_ptr = device;
        info.data_event.valid_bytes = _ACC_DATA_EVENT_INFO_VALID_BYTES;
    }
    if (gangs) {
        info.launch_event.num_gangs = gangs;
        info.launch_event.num_workers = workers;
        info.launch_event.vector_length = vector;
        info.launch_event.valid_bytes = _ACC_LAUNCH_EVENT_INFO_VALID_BYTES;
    }
    api.device_api = acc_device_api_cuda;
    api.valid_bytes = _ACC_API_INFO_VALID_BYTES;
    callbacks[event](&prof, &info, &api);
    raised++;
}

int main(int argc, char **argv)
{
    int rank, size, iterations = argc > 1 ? atoi(argv[1]) : 5, errors = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    raise_event(acc_ev_device_init_start, 0, 0, 0, 0, acc_construct_init);
    raise_event(acc_ev_device_init_end, 0, 0, 0, 0, acc_construct_init);
    for (int it = 0; it < iterations; it++) {
        raise_event(acc_ev_enter_data_start, 0, 0, 0, 0, acc_construct_enter_data);
        raise_event(acc_ev_alloc, UPLOAD_BYTES, 0, 0, 0, acc_construct_enter_data);
        raise_event(acc_ev_create, UPLOAD_BYTES, 0, 0, 0, acc_construct_enter_data);
        raise_event(acc_ev_enqueue_upload_start, UPLOAD_BYTES, 0, 0, 0, acc_construct_enter_data);
        usleep(200);
        raise_event(acc_ev_enqueue_upload_end, UPLOAD_BYTES, 0, 0, 0, acc_construct_enter_data);
        raise_event(acc_ev_enter_data_end, 0, 0, 0, 0, acc_construct_enter_data);

        raise_event(acc_ev_compute_construct_start, 0, 0, 0, 0, acc_construct_parallel);
        raise_event(acc_ev_enqueue_launch_start, 0, 4, 1, 128, acc_construct_parallel);
        usleep(500);
        raise_event(acc_ev_enqueue_launch_end, 0, 4, 1, 128, acc_construct_parallel);
        raise_event(acc_ev_compute_construct_end, 0, 0, 0, 0, acc_construct_parallel);

        raise_event(acc_ev_wait_start, 0, 0, 0, 0, acc_construct_wait);
        usleep(100);
        raise_event(acc_ev_wait_end, 0, 0, 0, 0, acc_construct_wait);

        raise_event(acc_ev_exit_data_start, 0, 0, 0, 0, acc_construct_exit_data);
        raise_event(acc_ev_enqueue_download_start, DOWNLOAD_BYTES, 0, 0, 0, acc_construct_exit_data);
        usleep(200);
        raise_event(acc_ev_enqueue_download_end, DOWNLOAD_BYTES, 0, 0, 0, acc_construct_exit_data);
        raise_event(acc_ev_delete, UPLOAD_BYTES, 0, 0, 0, acc_construct_exit_data);
        raise_event(acc_ev_free, UPLOAD_BYTES, 0, 0, 0, acc_construct_exit_data);
        raise_event(acc_ev_exit_data_end, 0, 0, 0, 0, acc_construct_exit_data);

        int one = 1, sum = 0;
        MPI_Allreduce(&one, &sum, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        if (sum != size)
            errors++;
    }
    raise_event(acc_ev_runtime_shutdown, 0, 0, 0, 0, acc_construct_shutdown);

    /* nothing is raised without a registered tool: the test only means something under tractor */
    if (rank == 0)
        printf("acc_sim_test: %s (%d errors, %d events raised, %s)\n", errors ? "FAILED" : "PASSED", errors, raised,
               callbacks[acc_ev_enqueue_launch_start] ? "tool registered" : "no tool registered");
    MPI_Finalize();
    return errors != 0;
}
