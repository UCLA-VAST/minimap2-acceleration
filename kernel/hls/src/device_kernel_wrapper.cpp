#include <vector>
#include <string>
#include "device_kernel_wrapper.h"
#include "datatypes.h"
#include "common.h"
#include "memory_scheduler.h"
#include <sys/time.h>

void device_chain_kernel_wrapper(
        std::string binaryFile,
        std::vector<anchor_dt, aligned_allocator<anchor_dt> >& arg,
        std::vector<return_dt, aligned_allocator<return_dt> >& ret,
        int max_dist_x, int max_dist_y, int bw)
{
    ret.resize(arg.size() / DATA_BLOCK_PER_BATCH * RETURN_BLOCK_PER_BATCH, 0);

    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>();

    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    cl::Program program(context, devices, bins);
    cl::Kernel kernel(program, "device_chain_kernel");

    printf(" ****** INFO host backtrack depth is %d, tile size is %d\n", BACK_SEARCH_COUNT, TILE_SIZE );

    struct timeval begin, end;
    gettimeofday(&begin, NULL);

    // Allocate buffer in global memory
    cl_mem_ext_ptr_t arg_bank;
    cl_mem_ext_ptr_t ret_bank;

    arg_bank.flags = XCL_MEM_DDR_BANK0;
    ret_bank.flags = XCL_MEM_DDR_BANK1;

    arg_bank.obj = arg.data();
    arg_bank.param = 0;
    ret_bank.obj = ret.data();
    ret_bank.param = 0;

    cl_int err;
    // buffers
    cl::Buffer device_arg (context, CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX,
            arg.size() * sizeof(arg[0]), &arg_bank, &err);

    cl::Buffer device_ret (context, CL_MEM_USE_HOST_PTR|CL_MEM_WRITE_ONLY|CL_MEM_EXT_PTR_XILINX,
            ret.size() * sizeof(ret[0]), &ret_bank, &err);

    std::vector<cl::Memory> dm_in, dm_out;
        dm_in.push_back(device_arg);
        dm_out.push_back(device_ret);

    q.enqueueMigrateMemObjects(dm_in, 0/* 0 means from host*/);
    q.finish();

    gettimeofday(&end, NULL);
    printf(" ****** kernel took %f seconds to transfer in data\n",
            1.0*(end.tv_sec - begin.tv_sec) + 1.0*(end.tv_usec - begin.tv_usec)/1000000);
    auto krnl_device_chain = cl::KernelFunctor<cl::Buffer&, cl::Buffer&, size_t, int, int, int>(kernel);
    krnl_device_chain(
            cl::EnqueueArgs(q, cl::NDRange(1, 1, 1), cl::NDRange(1, 1, 1)),
            device_arg, device_ret,
            arg.size(),
            max_dist_x, max_dist_y, bw);
    q.finish();
    gettimeofday(&end, NULL);
    printf("  ***** kernel took %f seconds to transfer in and execute\n",
            1.0*(end.tv_sec - begin.tv_sec) + 1.0*(end.tv_usec - begin.tv_usec)/1000000);

    q.enqueueMigrateMemObjects(dm_out, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();
    gettimeofday(&end, NULL);
    printf("  ***** kernel took %f seconds for end-to-end\n",
            1.0*(end.tv_sec - begin.tv_sec) + 1.0*(end.tv_usec - begin.tv_usec)/1000000);
}

