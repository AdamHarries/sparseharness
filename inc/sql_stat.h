#pragma once

#include <chrono>
#include <sstream>
#include <string>

enum Correctness {
  CORRECT,
  NOT_CHECKED,
  GENERIC_FAILURE,
  BAD_LENGTH,
  BAD_VALUES,
  GENERIC_BAD_VALUES,
  STATISTIC_VALUE
};

enum TrialType { RAW_RESULT, MULTI_ITERATION_SUM, MEDIAN_RESULT };

class SqlStat {

public:
  SqlStat(std::chrono::nanoseconds time, Correctness correctness,
          unsigned int global, unsigned int local, TrialType trial_type,
          unsigned int trial = 0, unsigned int iteration = 0)
      : _time(time), _correctness(correctness), _global(global), _local(local),
        _trial_type(trial_type), _trial(trial), _iteration(iteration) {}

  std::string printStat(const std::string &kernel_name,
                        const std::string &host_name,
                        const std::string &device_name,
                        const std::string &matrix_name,
                        const std::string &experiment_id) {
    std::ostringstream out;
    out << "(" << ((double)_time.count()) / 1000000.0 << ", \""
        << trialCorrectness() << "\", \"" << kernel_name << "\", " << _global
        << ", " << _local << ", \"" << host_name << "\", \"" << device_name
        << "\", \"" << matrix_name << "\"," << _iteration << "," << _trial
        << ",\""

        << trialType() << "\", \"" << experiment_id << "\")";
    return out.str();
  }

  static std::string printHeader() {
    std::ostringstream out;
    out << "INSERT INTO table_name (time, correct, kernel, "
        << "global, local, host, device, matrix, iteration, trial,"
        << "statistic, experiment_id) VALUES ";
    return out.str();
  }

  static bool compare(SqlStat a, SqlStat b) {
    return a.getTime() < b.getTime();
  }

  static std::chrono::nanoseconds add(SqlStat a, SqlStat b) {
    return a.getTime() + b.getTime();
  }

  static std::string makeSqlCommand(std::vector<SqlStat> stats,
                                    const std::string &kernel_name,
                                    const std::string &host_name,
                                    const std::string &device_name,
                                    const std::string &matrix_name,
                                    const std::string &experiment_id) {
    std::ostringstream out;
    out << printHeader();
    int i = 0;
    for (auto stat : stats) {
      if (i != 0) {
        out << ", ";
      }
      out << stat.printStat(kernel_name, host_name, device_name, matrix_name,
                            experiment_id);
      i++;
    }
    out << ";";
    return out.str();
  }

  std::chrono::nanoseconds getTime() { return _time; }

private:
  std::string trialType() {
    switch (_trial_type) {
    case RAW_RESULT:
      return "RAW_RESULT";
      break;
    case MULTI_ITERATION_SUM:
      return "MULTI_ITERATION_SUM";
      break;
    case MEDIAN_RESULT:
      return "MEDIAN_RESULT";
      break;
    default:
      return "ERROR";
    };
  }

  std::string trialCorrectness() {
    switch (_correctness) {
    case CORRECT:
      return "correct";
      break;
    case NOT_CHECKED:
      return "notchecked";
      break;
    case GENERIC_FAILURE:
      return "genericfailure";
      break;
    case BAD_LENGTH:
      return "badlength";
      break;
    case BAD_VALUES:
      return "badvalues";
      break;
    case GENERIC_BAD_VALUES:
      return "badvalues";
      break;
    case STATISTIC_VALUE:
      return "statisticvalue";
    default:
      return "ERROR";
    }
  }
  std::chrono::nanoseconds _time;
  Correctness _correctness;
  unsigned int _global;
  unsigned int _local;
  TrialType _trial_type;
  unsigned int _trial;
  unsigned int _iteration;
};