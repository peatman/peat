/* MPI + OpenCL calls against the fake library (cl_fake.c); handles are made-up numbers, nothing is dereferenced.
 *   cl_sim_test [iterations] */
#include <CL/cl.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define WRITE_BYTES (1 << 20)
#define READ_BYTES (1 << 18)
#define COPY_BYTES (1 << 16)
#define SVM_BYTES (1 << 12)

int main(int argc, char **argv)
{
    int rank, size, iterations = argc > 1 ? atoi(argv[1]) : 5, errors = 0;
    char host[16], svm[16];
    cl_command_queue queue, ooq, lone;
    cl_queue_properties out_of_order[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    cl_mem a = (cl_mem)0x2000, b = (cl_mem)0x2100;
    cl_kernel kernel = (cl_kernel)0x3000;
    cl_program program = (cl_program)0x4000;
    size_t gws = 1024, lws = 64;
    cl_int pattern = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    queue = clCreateCommandQueueWithProperties((cl_context)0x10, (cl_device_id)0x20, NULL, NULL);
    ooq = clCreateCommandQueueWithProperties((cl_context)0x10, (cl_device_id)0x20, out_of_order, NULL);
    lone = clCreateCommandQueueWithProperties((cl_context)0x10, (cl_device_id)0x20, NULL, NULL);
    errors += clBuildProgram(program, 0, NULL, NULL, NULL, NULL) != CL_SUCCESS;
    for (int it = 0; it < iterations; it++) {
        cl_event written = NULL, ran = NULL, ran_task = NULL;
        errors += clEnqueueWriteBuffer(queue, a, CL_FALSE, 0, WRITE_BYTES, host, 0, NULL, &written) != CL_SUCCESS;
        errors += clEnqueueFillBuffer(queue, b, &pattern, sizeof(pattern), 0, COPY_BYTES, 0, NULL, NULL) != CL_SUCCESS;
        errors += clEnqueueCopyBuffer(queue, a, b, 0, 0, COPY_BYTES, 0, NULL, NULL) != CL_SUCCESS;
        errors += clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &gws, &lws, 1, &written, &ran) != CL_SUCCESS;
        errors += clEnqueueTask(queue, kernel, 1, &ran, &ran_task) != CL_SUCCESS;
        errors += clEnqueueSVMMemcpy(queue, CL_TRUE, svm, host, SVM_BYTES, 0, NULL, NULL) != CL_SUCCESS;
        errors += clEnqueueSVMMemFill(queue, svm, &pattern, sizeof(pattern), SVM_BYTES, 0, NULL, NULL) != CL_SUCCESS;
        errors += clEnqueueReadBuffer(queue, b, CL_TRUE, 0, READ_BYTES, host, 0, NULL, NULL) != CL_SUCCESS;
        /* waiting for the task's event completes the task and, through the wait lists, the kernel and the write;
           the copy and the fill have no event and are completed by clFinish */
        errors += clWaitForEvents(1, &ran_task) != CL_SUCCESS;
        errors += clFinish(queue) != CL_SUCCESS;
        if (it == iterations - 1) /* nobody waits for this queue: counted by its enqueue call alone */
            errors += clEnqueueNDRangeKernel(lone, kernel, 1, NULL, &gws, &lws, 0, NULL, NULL) != CL_SUCCESS;

        /* in-order queue: waiting for the second command finishes the first one too */
        cl_event ev_a = NULL, ev_b = NULL, ev_c = NULL, ev_d = NULL;
        errors += clEnqueueCopyBuffer(queue, a, b, 0, 0, 1 << 14, 0, NULL, &ev_a) != CL_SUCCESS;
        errors += clEnqueueWriteBuffer(queue, a, CL_FALSE, 0, 1 << 10, host, 0, NULL, &ev_b) != CL_SUCCESS;
        errors += clWaitForEvents(1, &ev_b) != CL_SUCCESS;
        /* out-of-order queue: it does not, the first command waits for clFinish */
        errors += clEnqueueCopyBuffer(ooq, a, b, 0, 0, 1 << 13, 0, NULL, &ev_c) != CL_SUCCESS;
        errors += clEnqueueWriteBuffer(ooq, a, CL_FALSE, 0, 1 << 9, host, 0, NULL, &ev_d) != CL_SUCCESS;
        errors += clWaitForEvents(1, &ev_d) != CL_SUCCESS;
        /* out-of-order queue, wait lists: waiting for the task finishes the kernel it waits for */
        cl_event ev_k = NULL, ev_t = NULL;
        errors += clEnqueueNDRangeKernel(ooq, kernel, 1, NULL, &gws, &lws, 0, NULL, &ev_k) != CL_SUCCESS;
        errors += clEnqueueTask(ooq, kernel, 1, &ev_k, &ev_t) != CL_SUCCESS;
        errors += clWaitForEvents(1, &ev_t) != CL_SUCCESS;

        int one = 1, sum = 0;
        MPI_Allreduce(&one, &sum, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        if (sum != size)
            errors++;
    }
    errors += clFinish(ooq) != CL_SUCCESS;
    errors += clReleaseCommandQueue(ooq) != CL_SUCCESS;
    if (rank == 0)
        printf("cl_sim_test: %s (%d errors)\n", errors ? "FAILED" : "PASSED", errors);
    MPI_Finalize();
    return errors != 0;
}
