// qst_cli.cpp - batch CLI for Qst calculation.
#include "qst_core.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>

using namespace qst;

namespace {

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

void print_help() {
    std::cout << R"(Qst Calculator CLI
Usage:
  qst_calc_cli --temp1 77 --csv1 H2-77K.csv --temp2 87 --csv2 H2-87K.csv
               --max-a 5 --max-b 2 --output-dir QstResults\H2 -o Sample-H2-Qst

Options:
  --temp1..5 VALUE     Temperature in K.
  --csv1..5 PATH       CSV file with Pressure(kPa),Loading(mmol/g).
  --max-a VALUE        Max temperature-dependent virial order (default: 5).
  --max-b VALUE        Max temperature-independent virial order (default: 2).
  --output-dir PATH    Output directory (default: current directory).
  -o, --prefix NAME    Output file prefix (default: Qst_result).
  --help               Show this help.

Output:
  {prefix}_Qst.csv
  {prefix}_Qst.svg
  {prefix}_Virial_Fit.svg
  {prefix}_log.txt
)";
}

std::map<std::string, std::string> parse_args(int argc, char* argv[]) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--help" || key == "-h") {
            args["--help"] = "1";
            continue;
        }
        if (key.rfind("--", 0) == 0 || key == "-o") {
            auto eq = key.find('=');
            if (eq != std::string::npos) {
                args[key.substr(0, eq)] = key.substr(eq + 1);
            } else if (i + 1 < argc) {
                args[key] = argv[++i];
            }
        }
    }
    return args;
}

std::string join_output_prefix(const std::string& output_dir, const std::string& prefix) {
    std::filesystem::path dir = output_dir.empty() ? std::filesystem::current_path()
                                                   : std::filesystem::u8path(output_dir);
    std::filesystem::create_directories(dir);
    std::filesystem::path file = std::filesystem::path(prefix).filename();
    return (dir / file).generic_string();
}

void log_line(std::ostringstream& log, const std::string& line) {
    log << line << "\n";
    std::cout << line << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    auto args = parse_args(argc, argv);
    if (args.count("--help")) {
        print_help();
        return 0;
    }

    int max_a = args.count("--max-a") ? std::stoi(args["--max-a"]) : 5;
    int max_b = args.count("--max-b") ? std::stoi(args["--max-b"]) : 2;
    std::string output_dir = args.count("--output-dir") ? args["--output-dir"] : ".";
    std::string prefix_name = args.count("-o") ? args["-o"] :
                              (args.count("--prefix") ? args["--prefix"] : "Qst_result");
    std::string prefix = join_output_prefix(output_dir, prefix_name);

    std::ostringstream log;
    log_line(log, "=== Qst Calculation Started ===");
    log_line(log, "Time: " + timestamp());

    std::vector<DataSet> datasets;
    for (int i = 1; i <= 5; ++i) {
        std::string suffix = std::to_string(i);
        std::string temp_key = "--temp" + suffix;
        std::string csv_key = "--csv" + suffix;
        bool has_temp = args.count(temp_key) > 0;
        bool has_csv = args.count(csv_key) > 0;
        if (!has_temp && !has_csv) continue;
        if (!has_temp || !has_csv) {
            std::cerr << "ERROR: " << temp_key << " and " << csv_key << " must be provided together.\n";
            return 1;
        }

        double temp = std::stod(args[temp_key]);
        auto ds = read_isotherm_csv(args[csv_key], temp);
        if (ds.N.empty()) {
            std::cerr << "ERROR: empty or invalid CSV: " << args[csv_key] << "\n";
            return 1;
        }
        datasets.push_back(ds);

        std::ostringstream line;
        line << "Dataset " << i << ": T=" << temp << " K, " << args[csv_key]
             << ", points=" << ds.N.size();
        log_line(log, line.str());
    }

    if (datasets.size() < 2) {
        std::cerr << "ERROR: at least 2 temperature datasets are required.\n";
        return 1;
    }

    {
        std::ostringstream line;
        line << "Auto fitting virial orders up to: a=" << max_a << " b=" << max_b;
        log_line(log, line.str());
    }

    auto t0 = std::chrono::steady_clock::now();
    AutoFitResult auto_fit = auto_virial_fit(datasets, max_a, max_b);
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    FitResult fit = auto_fit.result;
    int m_order = auto_fit.selected_m;
    int n_order = auto_fit.selected_n;

    log_line(log, "--- Candidate Orders ---");
    for (const auto& c : auto_fit.candidates) {
        std::ostringstream line;
        line << "  m=" << c.m_order << " n=" << c.n_order
             << " R2=" << std::fixed << std::setprecision(6) << c.result.r_squared
             << " chi2=" << std::scientific << c.result.chi_squared
             << " BIC=" << c.bic
             << " penalty=" << c.penalty
             << " score=" << c.score
             << " unstable=" << (c.unstable_qst ? "yes" : "no");
        log_line(log, line.str());
    }
    for (const auto& diagnostic : auto_fit.diagnostics) {
        log_line(log, "DIAG: " + diagnostic);
    }

    {
        std::ostringstream line;
        line << "Selected virial: m=" << m_order << " n=" << n_order
             << "; done in " << ms << " ms, iter=" << fit.iterations
             << ", R2=" << std::fixed << std::setprecision(6) << fit.r_squared
             << ", converged=" << (fit.converged ? "yes" : "no");
        log_line(log, line.str());
    }

    log_line(log, "--- Parameters ---");
    for (int i = 0; i <= m_order; ++i) {
        std::ostringstream line;
        line << "a_" << i << " = " << std::showpos << std::scientific << fit.params[i];
        log_line(log, line.str());
    }
    for (int i = 0; i <= n_order; ++i) {
        std::ostringstream line;
        line << "b_" << i << " = " << std::showpos << std::scientific << fit.params[m_order + 1 + i];
        log_line(log, line.str());
    }

    if (!fit.param_errors.empty()) {
        log_line(log, "--- Std Errors ---");
        for (int i = 0; i <= m_order; ++i) {
            std::ostringstream line;
            line << "a_" << i << " = " << std::scientific << fit.param_errors[i];
            log_line(log, line.str());
        }
        for (int i = 0; i <= n_order; ++i) {
            std::ostringstream line;
            line << "b_" << i << " = " << std::scientific << fit.param_errors[m_order + 1 + i];
            log_line(log, line.str());
        }
    }

    if (!auto_fit.range.valid) {
        std::cerr << "ERROR: no shared loading range across the selected temperatures.\n";
        return 1;
    }

    std::vector<double> loadings;
    std::vector<double> qst_values;
    std::vector<double> qst_errors;
    for (double n = auto_fit.range.from; n <= auto_fit.range.to + 1e-9; n += auto_fit.range.step) {
        loadings.push_back(n);
        qst_values.push_back(qst_at_loading(n, fit.params.data(), m_order));
        qst_errors.push_back(qst_error_at_loading(n, fit, m_order));
    }

    std::string fit_path = prefix + "_Virial_Fit.svg";
    std::string qst_svg_path = prefix + "_Qst.svg";
    std::string qst_csv_path = prefix + "_Qst.csv";
    std::string log_path = prefix + "_log.txt";

    svg_virial_fit(fit_path, datasets, fit, m_order, n_order);
    svg_qst_curve(qst_svg_path, loadings, qst_values);
    write_qst_csv(qst_csv_path, loadings, qst_values, qst_errors);

    log_line(log, "SVG: " + fit_path + ", " + qst_svg_path);
    log_line(log, "CSV: " + qst_csv_path);
    log_line(log, "--- Qst vs Loading ---");
    for (size_t i = 0; i < loadings.size(); ++i) {
        std::ostringstream line;
        line << "N=" << std::fixed << std::setprecision(6) << loadings[i]
             << " Qst=" << std::setprecision(4) << qst_values[i]
             << " +/- " << qst_errors[i] << " kJ/mol";
        log_line(log, line.str());
    }
    log_line(log, "=== Done ===");

    std::ofstream out(log_path);
    if (!out.is_open()) {
        std::cerr << "ERROR: cannot write log: " << log_path << "\n";
        return 1;
    }
    out << log.str();
    return 0;
}
