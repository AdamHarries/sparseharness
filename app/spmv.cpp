// [standard includes]
#include <algorithm>
#include <atomic>
#include <bitset>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <thread>
#include <typeinfo>
#include <vector>

// [tools]
#include "common.h"
#include "csds_timer.h"
#include "csv_utils.h"
#include "options.h"

// [ocl tools]
#include "harness.h"
#include "kernel_config.h"
#include "kernel_utils.h"

// [application specific]
#include "run.h"
#include "sparse_matrix.h"
#include "vector_generator.h"

// [OpenCL]
#ifdef __APPLE__
#include "OpenCL/opencl.h"
#else
#include "CL/cl.h"
#endif

class HarnessSPMV : public Harness<SqlStat, float> {
public:
  HarnessSPMV(std::string &kernel_source, unsigned int platform,
              unsigned int device, ArgContainer<float> args,
              unsigned int trials, double timeout, double delta)
      : Harness(kernel_source, platform, device, args, trials, timeout, delta) {
  }
  std::vector<SqlStat> benchmark(Run run) {

    start_timer(benchmark, HarnessSPMV);
    allocateBuffers();

    // run the kernel!
    std::vector<SqlStat> runtimes;
    for (unsigned int t = 0; t < _trials; t++) {
      start_timer(benchmark_iteration, HarnessSPMV);

      // get pointers to the input + output mem args
      // cl_mem *input_mem_ptr = &(_mem_manager._x_vect);
      // cl_mem *output_mem_ptr = &(_mem_manager._output);
      LOG_DEBUG_INFO("Host vectors before");
      printCharVector<float>("Input ", _mem_manager._input_host_buffer);
      printCharVector<float>("Output ", _mem_manager._output_host_buffer);

      resetTempBuffers();
      // run the kernel
      runtimes.push_back(executeRun(run, t));

      // copy the output back down
      readFromGlobalArg(_mem_manager._output_host_buffer, _mem_manager._output);

      LOG_DEBUG_INFO("Host vectors after");
      printCharVector<float>("Input ", _mem_manager._input_host_buffer);
      printCharVector<float>("Output ", _mem_manager._output_host_buffer);

      // check to see that we've actually done something with our SPMV!
      assertBuffersNotEqual(_mem_manager._output_host_buffer,
                            _mem_manager._temp_out_buffer);
    }
    // sum the runtimes, and median it and report that
    std::sort(runtimes.begin(), runtimes.end(), SqlStat::compare);
    std::chrono::nanoseconds median_time =
        runtimes[runtimes.size() / 2].getTime();

    runtimes.push_back(SqlStat(median_time, NOT_CHECKED, run.global1,
                               run.local1, MEDIAN_RESULT));

    return runtimes;
  }

private:
  virtual SqlStat executeRun(Run run, unsigned int trial) {
    // get the runtime from a single kernel run
    std::chrono::nanoseconds time = executeKernel(run);
    return SqlStat(time, NOT_CHECKED, run.global1, run.local1, RAW_RESULT);
  }
};

int main(int argc, char *argv[]) {
  COMMON_MAIN_PREAMBLE

  // specialise the matrix for the kernel given
  auto cl_matrix = kernel.specialiseMatrix(matrix, 0.0f);
  // extract size variables from it
  int v_Width_cl = cl_matrix.getCLVWidth();
  int v_Height_cl = cl_matrix.getCLVHeight();
  int v_Length_cl = matrix.width();

  std::cout << "v_Width_cl = " << v_Width_cl << "\n";
  std::cout << "v_Height_cl = " << v_Height_cl << "\n";
  std::cout << "v_Length_cl = " << v_Length_cl << "\n";

  // size args of name/order:
  // v_MWidthC_1, v_MHeight_2, v_VLength_3
  // std::vector<int> size_args{v_Width_cl, v_Width_cl, v_Length_cl};

  // generate a vector

  ConstXVectorGenerator<float> onegen(1.0f);
  ConstYVectorGenerator<float> zerogen(0);

  // auto clkernel = executor::Kernel(kernel.getSource(), "KERNEL", "").build();
  // get some arguments
  auto args =
      executorEncodeMatrix(kernel, matrix, 0.0f, onegen, zerogen, v_Width_cl,
                           v_Height_cl, v_Length_cl, 1.0f, 0.0f);
  HarnessSPMV harness(kernel.getSource(), opt_platform->get(),
                      opt_device->get(), args, opt_trials->get(),
                      opt_timeout->get(), opt_float_delta->get());
  const std::string &kernel_name = kernel.getName();
  const std::string &host_name = hostname;
  const std::string &device_name = harness.getDeviceName();
  const std::string &experiment_id = experiment;
  for (auto run : runs) {
    start_timer(run_iteration, main);
    std::cout << "Benchmarking run: " << run << ENDL;
    std::vector<SqlStat> runtimes = harness.benchmark(run);
    std::cout << "runtimes: [";
    for (auto time : runtimes) {
      std::cout << "\n\t"
                << time.printStat(kernel_name, host_name, device_name,
                                  matrix_name, experiment_id);
    }
    std::cout << "\n]" << ENDL;
    std::string command =
        SqlStat::makeSqlCommand(runtimes, kernel_name, host_name, device_name,
                                matrix_name, experiment_id);
    std::cout << command << "\n";
  }
}
