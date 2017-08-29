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

class HarnessEigenvector
    : public IterativeHarness<std::chrono::milliseconds, float> {
public:
  HarnessEigenvector(std::string &kernel_source, unsigned int platform,
                     unsigned int device, ArgContainer<float> args)
      : IterativeHarness(kernel_source, platform, device, args) {}
  std::vector<std::chrono::milliseconds>
  benchmark(Run run, int iterations, double timeout, double delta) {

    start_timer(benchmark, HarnessEigenvector);

    // set up the kernel
    cl_uint arg_index = 0;
    cl_uint input_idx = 2;
    cl_uint output_idx = 0;

    // build the matrix arguments
    std::cerr << "Setting matrix arguments" << ENDL;
    cl_mem matrix_idxs_ma = createAndUploadGlobalArg(_args.m_idxs);
    setGlobalArg(arg_index++, &matrix_idxs_ma);

    cl_mem matrix_vals_ma = createAndUploadGlobalArg(_args.m_vals);
    setGlobalArg(arg_index++, &matrix_vals_ma);

    // build the vector arguments
    std::cerr << "Setting vector arguments" << ENDL;
    cl_mem x_vect_ma = createAndUploadGlobalArg(_args.x_vect, true);
    setGlobalArg(arg_index++, &x_vect_ma);

    cl_mem y_vect_ma = createAndUploadGlobalArg(_args.y_vect, true);
    setGlobalArg(arg_index++, &y_vect_ma);

    // build the constant arguments
    std::cerr << "Setting constant arguments" << ENDL;
    setValueArg<float>(arg_index++, &(_args.alpha));
    setValueArg<float>(arg_index++, &(_args.beta));

    // set the output arg
    std::cerr << "Setting the output argument" << ENDL;
    output_idx = arg_index;
    cl_mem output_mem = createGlobalArg(_args.output);
    setGlobalArg(arg_index++, &output_mem);
    // build temp globals
    // do we need to save them at all???
    // std::vector<cl_mem> temp_globals(_args.temp_globals.size());
    // THIS MIGHT SEGFAULT AWFULLY :D
    std::cerr << "Setting temp global arguments" << ENDL;
    for (auto size : _args.temp_globals) {
      cl_mem temp_arg = createGlobalArg(size);
      setGlobalArg(arg_index++, &temp_arg);
    }

    // build temp locals
    std::cerr << "Setting temp local arguments" << ENDL;
    for (auto size : _args.temp_locals) {
      setLocalArg(arg_index++, size);
    }

    // set the size arguments
    std::cerr << "Setting size arguments" << ENDL;
    for (auto size : _args.size_args) {
      setValueArg<unsigned int>(arg_index++, &(size));
    }

    // finally, create buffers to copy the input and output into
    std::vector<char> input_host_buffer(_args.x_vect.begin(),
                                        _args.x_vect.end());
    std::vector<char> output_host_buffer(_args.output, 0);

    std::vector<char> blank_output_buffer(_args.output, 0);

    // run the kernel!
    std::vector<std::chrono::milliseconds> runtimes;
    for (int i = 0; i < iterations; i++) {
      start_timer(benchmark_iteration, HarnessEigenvector);

      // get pointers to the input + output mem args
      cl_mem *input_mem_ptr = &x_vect_ma;
      cl_mem *output_mem_ptr = &output_mem;

      // and pointers to the input + output host args
      std::vector<char> *input_host_ptr = &input_host_buffer;
      std::vector<char> *output_host_ptr = &output_host_buffer;

      bool should_terminate = false;
      int itcnt = 0;
      do {
        std::cout << "Host vectors before: \n\tInput:";
        printCharVector<float>(*input_host_ptr);
        std::cout << "\tOutput:";
        printCharVector<float>(*output_host_ptr);

        // run the kernel
        runtimes.push_back(executeKernel(run));

        // copy the output back down
        readFromGlobalArg(*output_host_ptr, *output_mem_ptr);

        std::cout << "Host vectors after: \n\tInput:";
        printCharVector<float>(*input_host_ptr);
        std::cout << "\tOutput:";
        printCharVector<float>(*output_host_ptr);

        should_terminate = should_terminate_iteration(*input_host_ptr,
                                                      *output_host_ptr, delta);
        // swap the pointers over
        std::swap(input_mem_ptr, output_mem_ptr);
        std::swap(input_host_ptr, output_host_ptr);

        // set the kernel args
        setGlobalArg(input_idx, input_mem_ptr);
        setGlobalArg(output_idx, output_mem_ptr);

        itcnt++;
      } while (!should_terminate && itcnt < 10);
    }

    // // we need a cache for the input vector
    // // this seems _super_ hacky. I don't like casting around like this, even
    // if
    // // it is legal. I'm worried.
    // std::vector<char> input_cache;
    // // copy_from_arg(_args.args[_args.input], input_cache);

    // // run the benchmark for that many iterations
    // for (int i = 0; i < iterations; i++) {
    //   start_timer(benchmark_iteration, HarnessEigenvector);
    //   // std::cout << "Iteration: " << i << '\n';

    //   // Run the algorithm
    //   double runtime = 0.0f;
    //   { // copy the cached input into the input arg:
    //     // copy_into_arg(input_cache, _args.args[_args.input]);
    //     // _args.args[_args.input]->clear();
    //     // _args.args[_args.input]->upload();

    //     bool should_terminate = false;
    // run the kernel
    // do {
    //   std::cout << " ------------------- VALUES BEFORE RUN\n";
    //   // print_arg<float>(_args.args[_args.input]);
    //   // print_arg<float>(_args.args[_args.output]);
    //   std::cout << "--------------- EXECUTING KERNEL\n";
    //   runtime += executeKernel(run);
    //   std::cout << " ------------------- VALUES after RUN\n";
    //   // print_arg<float>(_args.args[_args.input]);
    //   // print_arg<float>(_args.args[_args.output]);
    //   std::cout << "--------------- PERFORMING CHECK\n";
    //   should_terminate = should_terminate_iteration(
    //       _args.args[_args.input], _args.args[_args.output], delta);
    //   // swap the pointers in the arg list
    //   std::cout << "---------------- SWAPPING \n";

    //   executor::KernelArg *tmp = _args.args[_args.input];
    //   _args.args[_args.input] = _args.args[_args.output];
    //   _args.args[_args.output] = tmp;
    //   // std::cout << "preswap: in: " << _args.input
    //   //           << " out: " << _args.output << "\n";
    //   // auto tmp = _args.input;
    //   // _args.input = _args.output;
    //   // _args.output = tmp;
    //   // std::cout << "postswap: in: " << _args.input
    //   //           << " out: " << _args.output << "\n";

    //   // copy the output buffer into the input
    //   // copy_args(_args.args[_args.output], _args.args[_args.input]);

    //   // reset the kernel args
    //   // _args.args[_args.output]->clear();

    //   // _args.args[_args.input]->upload();
    //   // _args.args[_args.output]->upload();

    //   _args.args[_args.input]->setAsKernelArg(_kernel, _args.input);
    //   _args.args[_args.output]->setAsKernelArg(_kernel, _args.output);
    // } while (!should_terminate);
    // get the underlying vectors from the args that we care about

    //   runtimes[i] = runtime;

    //   if (timeout != 0.0 && runtime >= timeout) {
    //     runtimes.resize(i + 1);
    //     return runtimes;
    //   }
    // }
    return runtimes;
  }

private:
  std::chrono::milliseconds executeRun(Run run) { return executeKernel(run); }

  virtual bool should_terminate_iteration(std::vector<char> &input,
                                          std::vector<char> &output,
                                          double delta) {
    start_timer(should_terminate_iteration, HarnessEigenvector);

    // reinterpret the args as double pointers, and get the lengths
    auto input_ptr = reinterpret_cast<float *>(input.data());
    auto output_ptr = reinterpret_cast<float *>(output.data());
    auto input_length = input.size() / sizeof(float);
    auto output_length = output.size() / sizeof(float);
    // perform a comparison across the two of them, based on pointers
    bool equal = true;
    for (unsigned int i = 0;
         equal == true && i < input_length && i < output_length; i++) {
      equal = fabs(input_ptr[i] - output_ptr[i]) < delta;
      std::cout << "Comparing: (" << input_ptr[i] << "," << output_ptr[i]
                << "), result: " << equal << "\n";
    }

    return equal;
  }
};

int main(int argc, char *argv[]) {
  start_timer(main, global);
  OptParser op(
      "Harness for SPMV sparse matrix dense vector multiplication benchmarks");

  auto opt_platform = op.addOption<unsigned>(
      {'p', "platform", "OpenCL platform index (default 0).", 0});
  auto opt_device = op.addOption<unsigned>(
      {'d', "device", "OpenCL device index (default 0).", 0});
  auto opt_iterations = op.addOption<unsigned>(
      {'i', "iterations",
       "Execute each kernel 'iterations' times (default 10).", 10});

  //   auto opt_input_file = op.addOption<std::string>({'f', "file", "Input
  //   file"});

  auto opt_matrix_file =
      op.addOption<std::string>({'m', "matrix", "Input matrix"});
  auto opt_kernel_file =
      op.addOption<std::string>({'k', "kernel", "Input kernel"});
  auto opt_run_file =
      op.addOption<std::string>({'r', "runfile", "Run configuration file"});
  auto opt_host_name = op.addOption<std::string>(
      {'n', "hostname", "Host the harness is running on"});
  auto opt_experiment_id = op.addOption<std::string>(
      {'e', "experiment", "An experiment ID for data reporting"});
  auto opt_float_delta = op.addOption<double>(
      {'t', "delta", "Delta for floating point comparisons", 0.0001});

  auto opt_timeout = op.addOption<float>(
      {'t', "timeout", "Timeout to avoid multiple executions (default 100ms).",
       100.0f});

  op.parse(argc, argv);

  using namespace std;

  const std::string matrix_filename = opt_matrix_file->require();
  const std::string kernel_filename = opt_kernel_file->require();
  const std::string runs_filename = opt_run_file->require();
  const std::string hostname = opt_host_name->require();
  const std::string experiment = opt_experiment_id->require();

  std::cerr << "matrix_filename " << matrix_filename << ENDL;
  std::cerr << "kernel_filename " << kernel_filename << ENDL;

  // initialise a matrix, kernel, and set of run parameters from files
  SparseMatrix<float> matrix(matrix_filename);
  KernelConfig<float> kernel(kernel_filename);
  auto csvlines = CSV::load_csv(runs_filename);
  std::vector<Run> runs;
  std::transform(csvlines.begin(), csvlines.end(), std::back_inserter(runs),
                 [](CSV::csv_line line) -> Run { return Run(line); });

  for (auto run : runs)
    std::cerr << run << ENDL;

  // check the matrix
  if (matrix.height() != matrix.width()) {
    std::cout << "Matrix is not square. Failing computation." << ENDL;
    std::cerr << "Matrix is not square. Failing computation." << ENDL;
    std::exit(2);
  } else {
    std::cout << " Matrix is square - width = " << matrix.width()
              << " and height = " << matrix.height() << "\n";
  }

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
  HarnessEigenvector harness(kernel.getSource(), opt_platform->get(),
                             opt_device->get(), args);
  for (auto run : runs) {
    start_timer(run_iteration, main);
    std::cout << "Benchmarking run: " << run << ENDL;
    std::vector<std::chrono::milliseconds> runtimes = harness.benchmark(
        run, opt_iterations->get(), opt_timeout->get(), opt_float_delta->get());
    std::cout << "runtimes: [";
    for (auto time : runtimes) {
      std::cout << "," << time.count();
    }
    std::cout << "]" << ENDL;
  }
}