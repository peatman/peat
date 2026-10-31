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
    cl_command_queue queue = (cl_command_queue)0x1000;
    cl_mem a = (cl_mem)0x2000, b = (cl_mem)0x2100;
    cl_kernel kernel = (cl_kernel)0x3000;
    cl_program program = (cl_program)0x4000;
    size_t gws = 1024, lws = 64;
    cl_int pattern = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

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
        if (it == iterations - 1) /* never waited for: counted by its enqueue call alone */
            errors += clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &gws, &lws, 0, NULL, NULL) != CL_SUCCESS;

        int one = 1, sum = 0;
        MPI_Allreduce(&one, &sum, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        if (sum != size)
            errors++;
    }
    if (rank == 0)
        printf("cl_sim_test: %s (%d errors)\n", errors ? "FAILED" : "PASSED", errors);
    MPI_Finalize();
    return errors != 0;
}
