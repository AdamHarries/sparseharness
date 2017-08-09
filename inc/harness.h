#pragma once
#include "executor/Executor.h"
#include "executor/GlobalArg.h"
#include "executor/LocalArg.h"
#include "executor/ValueArg.h"
#include "run.h"

class Harness {
public:
  Harness(cl::Kernel kernel, std::vector<KernelArg *> args)
      : _kernel(kernel), _args(args) {}

  virtual std::vector<double> benchmark(Run run, int iterations,
                                        double timeout) = 0;

protected:
  virtual double executeKernel(Run run) = 0;
  cl::Kernel _kernel;
  std::vector<KernelArg *> _args;
};