// iast_cli.cpp — Pure C++ IAST Calculation CLI Tool
// Compile: cl /O2 /EHsc /std:c++17 /Fe:iast_calc.exe iast_cli.cpp
//          (or: g++ -O3 -std=c++17 -o iast_calc iast_cli.cpp)
// Zero external dependencies. EXE typically < 500 KB.

#include "iast_core.h"
#include <map>
#include <chrono>
#include <ctime>

using namespace iast;

// ─── Helpers ─────────────────────────────────────────────────

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

// Parse "A1=2.34,A2=4.33" into param vector {A1,B1,C1,A2,B2,C2}
std::vector<double> parse_init_params(const std::string& s) {
    std::vector<double> p = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    if (s.empty()) return p;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) {
        size_t eq = token.find('=');
        if (eq == std::string::npos) continue;
        std::string key = token.substr(0, eq);
        double val = std::stod(token.substr(eq + 1));
        // Trim
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        if (key == "A1") p[0] = val;
        else if (key == "B1") p[1] = val;
        else if (key == "C1") p[2] = val;
        else if (key == "A2") p[3] = val;
        else if (key == "B2") p[4] = val;
        else if (key == "C2") p[5] = val;
    }
    return p;
}

// ─── Help ────────────────────────────────────────────────────

void print_help() {
    std::cout << R"(IAST Calculator (C++) - Ideal Adsorbed Solution Theory
========================================================

Usage (binary):
  iast_calc --gas1 C3H6 --gas2 C2H4 --csv1 C3H6.csv --csv2 C2H4.csv
            --model1 DSLF --model2 DSLF --y1 0.5 -o output_prefix

Usage (ternary):
  iast_calc --gas1 C2H6 --gas2 C2H4 --gas3 CH4
            --csv1 C2H6.csv --csv2 C2H4.csv --csv3 CH4.csv
            --model1 DSL --model2 DSL --model3 SSLF
            --y1 0.45 --y2 0.45 -o output_prefix

Options:
  --gas1/2/3 NAME         Gas component names
  --csv1/2/3 PATH         CSV files (column1=Pressure(kPa), column2=Loading(mmol/g))
  --model1/2/3 MODEL      Isotherm model: Auto, SSL, DSL, SSLF, DSLF (default: Auto)
  --params1/2/3 PARAMS    Initial params: "A1=2.34,A2=4.33" (default: A1=1.0)
  --y1,y2 FRAC            Gas-phase mole fractions (sum must be 1.0)
  -o PREFIX               Output file prefix (default: IAST_result)
  --output-dir PATH       Output directory (default: current directory)
  --pressures LIST        Comma-separated pressures (default: 1-10, 20-100, 105 kPa)
  --generate-config       Print sample config JSON
  --config PATH           JSON config file (NOT YET IMPLEMENTED in C++)
  --help                  Show this help

Output (written directly to --output-dir):
  {prefix}_Selectivity.csv      Numerical results
  {prefix}_Exp_isotherms.svg    Pure-component isotherm plot
  {prefix}_IAST_validation.svg  Mixed-gas sorption plot
  {prefix}_Selectivity.svg      Selectivity vs pressure
  {prefix}_Separation_Potential.svg  (binary only)

Models:
  SSL:  q = A*B*P / (1+B*P)                              (2 params)
  DSL:  q = A1*B1*P/(1+B1*P) + A2*B2*P/(1+B2*P)        (4 params)
  SSLF: q = A*B*P^C / (1+B*P^C)                          (3 params)
  DSLF: q = A1*B1*P^C1/(1+B1*P^C1) + A2*B2*P^C2/(1+B2*P^C2)  (6 params)
)";
}

void print_sample_config() {
    std::cout << R"({
  "gases": [
    {"name": "C3H6", "csv": "C3H6.csv", "model": "DSLF", "params_init": {"A1": 2.34, "A2": 4.33}},
    {"name": "C2H4", "csv": "C2H4.csv", "model": "DSLF", "params_init": {"A1": 3.98, "A2": 2.18}}
  ],
  "y": [0.5],
  "pressures": [1,2,3,4,5,6,7,8,9,10,20,30,40,50,60,70,80,90,100,105],
  "output_prefix": "IAST_result"
}
)";
}

// ─── Main ────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Parse CLI args
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { print_help(); return 0; }
        if (a == "--generate-config") { print_sample_config(); return 0; }
        if (a.rfind("--", 0) == 0 || a.rfind("-o", 0) == 0) {
            std::string key = a;
            if (key == "-o" && i + 1 < argc) {
                args[key] = argv[++i];
            } else if (key.find('=') != std::string::npos) {
                size_t eq = key.find('=');
                args[key.substr(0, eq)] = key.substr(eq + 1);
            } else if (i + 1 < argc && argv[i+1][0] != '-') {
                args[key] = argv[++i];
            }
        }
    }

    if (args.find("--config") != args.end()) {
        std::cerr << "ERROR: --config (JSON) not yet supported in C++ version.\n";
        std::cerr << "Please use command-line arguments instead.\n";
        return 1;
    }

    // Determine mixture type
    bool ternary = args.count("--gas3") > 0;
    int num_gases = ternary ? 3 : 2;
    
    std::vector<std::string> names;
    std::vector<std::string> csv_paths;
    std::vector<std::string> models;
    std::vector<std::vector<double>> init_params;
    
    for (int i = 1; i <= num_gases; ++i) {
        std::string s = std::to_string(i);
        auto name_it = args.find("--gas" + s);
        auto csv_it = args.find("--csv" + s);
        if (name_it == args.end() || csv_it == args.end()) {
            std::cerr << "ERROR: --gas" << i << " and --csv" << i << " are required\n";
            return 1;
        }
        names.push_back(name_it->second);
        csv_paths.push_back(csv_it->second);
        
        auto model_it = args.find("--model" + s);
        models.push_back(model_it != args.end() ? model_it->second : "Auto");
        
        auto params_it = args.find("--params" + s);
        init_params.push_back(parse_init_params(params_it != args.end() ? params_it->second : "A1=1.0"));
        
        // Validate model
        if (models.back() != "Auto" &&
            models.back() != "SSL" && models.back() != "DSL" &&
            models.back() != "SSLF" && models.back() != "DSLF") {
            std::cerr << "ERROR: Unknown model '" << models.back() << "' for gas " << i << "\n";
            return 1;
        }
    }

    // Parse mixture composition
    std::vector<double> y;
    for (int i = 1; i <= num_gases; ++i) {
        std::string s = std::to_string(i);
        auto y_it = args.find("--y" + s);
        if (y_it == args.end()) {
            if (i <= 2) y.push_back(0.5); // default for binary
            else y.push_back(0.0);
        } else {
            y.push_back(std::stod(y_it->second));
        }
    }

    double ysum = 0.0;
    for (double v : y) ysum += v;
    if (std::abs(ysum - 1.0) > 0.001) {
        std::cerr << "ERROR: Mole fractions must sum to 1.0 (got " << ysum << ")\n";
        return 1;
    }

    // Parse pressures
    std::vector<double> pressures;
    auto press_it = args.find("--pressures");
    if (press_it != args.end()) {
        std::stringstream ss(press_it->second);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) pressures.push_back(std::stod(token));
        }
    } else {
        // Default: 1-10 kPa, then 20-100 kPa in 10 kPa steps, plus 105 kPa.
        for (int i = 1; i <= 10; ++i) pressures.push_back((double)i);
        for (int i = 20; i <= 100; i += 10) pressures.push_back((double)i);
        pressures.push_back(105.0);
    }

    auto output_dir_it = args.find("--output-dir");
    if (output_dir_it != args.end()) set_output_base_dir(std::filesystem::u8path(output_dir_it->second));
    auto output_it = args.find("-o");
    std::string prefix = output_it != args.end() ? output_it->second : "IAST_result";
    prefix = final_output_prefix(prefix);

    // ═══════════════════════════════════════════════════════
    // Phase 1: Load data
    // ═══════════════════════════════════════════════════════
    std::cout << "============================================================\n";
    std::cout << "IAST Calculation: ";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) std::cout << " + ";
        std::cout << names[i];
    }
    std::cout << "\n";
    std::cout << "Gas phase composition: ";
    for (size_t i = 0; i < y.size(); ++i) {
        if (i > 0) std::cout << " ";
        std::cout << "y" << (i+1) << "=" << y[i];
    }
    std::cout << "\n";
    std::cout << "Total pressures: " << pressures.size() << " points\n";
    std::cout << "============================================================\n\n";

    std::cout << "[1/5] Loading pure-component isotherm data...\n";
    std::vector<std::vector<double>> gas_pressure, gas_uptake;
    for (int i = 0; i < num_gases; ++i) {
        auto [P, U] = read_isotherm_csv(csv_paths[i]);
        gas_pressure.push_back(P);
        gas_uptake.push_back(U);
        std::cout << "  " << names[i] << ": " << P.size() << " data points\n";
    }

    // ═══════════════════════════════════════════════════════
    // Phase 2: Fit isotherms
    // ═══════════════════════════════════════════════════════
    std::cout << "\n[2/5] Fitting pure-component isotherm models...\n";
    std::vector<std::vector<double>> fitted_params;
    std::vector<FitResult> fit_results;
    std::vector<std::string> selected_models;
    std::vector<std::vector<AutoFitCandidate>> fit_candidates;

    for (int i = 0; i < num_gases; ++i) {
        std::cout << "  " << names[i] << ": " << models[i] << " model...\n";
        AutoFitResult auto_fit = fit_model_auto(gas_pressure[i], gas_uptake[i], models[i], init_params[i]);
        FitResult res = auto_fit.result;
        fitted_params.push_back(res.params);
        fit_results.push_back(res);
        selected_models.push_back(auto_fit.selected_model);
        fit_candidates.push_back(auto_fit.candidates);
        
        std::cout << "    selected model=" << auto_fit.selected_model << "\n";
        std::cout << "    A1=" << res.params[0] << ", B1=" << res.params[1] << ", C1=" << res.params[2] << "\n";
        std::cout << "    A2=" << res.params[3] << ", B2=" << res.params[4] << ", C2=" << res.params[5] << "\n";
        std::cout << "    R^2=" << res.r_squared << ", chi^2=" << res.chi_squared
                  << ", iter=" << res.iterations << ", converged=" << (res.converged ? "yes" : "no") << "\n";
        if (auto_fit.candidates.size() > 1) {
            for (auto& c : auto_fit.candidates) {
                std::cout << "      candidate " << c.model << ": R^2=" << c.result.r_squared
                          << ", chi^2=" << c.result.chi_squared << ", BIC=" << c.score << "\n";
            }
        }
    }

    // ═══════════════════════════════════════════════════════
    // Phase 3: Plots (pure isotherms)
    // ═══════════════════════════════════════════════════════
    std::cout << "\n[3/5] Generating plots...\n";
    {
        std::vector<std::pair<std::string, std::pair<std::vector<double>, std::vector<double>>>> series;
        for (int i = 0; i < num_gases; ++i)
            series.push_back({names[i], {gas_pressure[i], gas_uptake[i]}});
        std::string fname = prefix + "_Exp_isotherms.svg";
        svg_scatter(fname, series, "Pressure (kPa)", "Gas uptake (mmol/g)",
                    "Pure-component " + names[0] + " and " + names[1] + " adsorption isotherms");
        std::cout << "  Saved: " << fname << "\n";
    }

    // ═══════════════════════════════════════════════════════
    // Phase 4: IAST
    // ═══════════════════════════════════════════════════════
    std::cout << "\n[4/5] Running IAST calculation...\n";
    std::string csv_fname = prefix + "_Selectivity.csv";

    std::string log_fname = prefix + "_log.txt";
    if (!ternary) {
        auto results = binary_iast(y[0], pressures, fitted_params[0], fitted_params[1]);
        
        std::vector<std::string> header = {
            "Total pressure (kPa)",
            names[0] + " Uptake (mmol/g)",
            names[1] + " Uptake (mmol/g)",
            names[0] + " mole fraction",
            names[1] + " mole fraction",
            "Selectivity",
            "Separation potential"
        };
        std::vector<std::vector<double>> rows;
        for (auto& r : results) {
            rows.push_back({r.P, r.uptake1, r.uptake2, r.x1, r.x2, r.selectivity, r.sep_potential});
        }
        write_csv(csv_fname, header, rows);
        std::cout << "  Saved: " << csv_fname << "\n";

        // Phase 5: IAST plots
        std::cout << "\n[5/5] Generating IAST result plots...\n";
        
        // Mixture uptake
        {
            std::vector<double> p_all, u1, u2;
            for (auto& r : results) { p_all.push_back(r.P); u1.push_back(r.uptake1); u2.push_back(r.uptake2); }
            std::vector<std::pair<std::string, std::pair<std::vector<double>, std::vector<double>>>> series;
            series.push_back({names[0] + " IAST", {p_all, u1}});
            series.push_back({names[1] + " IAST", {p_all, u2}});
            std::string fname = prefix + "_IAST_validation.svg";
            svg_scatter(fname, series, "Total pressure (kPa)", "Gas uptake (mmol/g)",
                        names[0] + " and " + names[1] + " mixed-gas sorption isotherms");
            std::cout << "  Saved: " << fname << "\n";
        }

        // Selectivity
        {
            std::vector<double> p_all, sel;
            for (auto& r : results) { p_all.push_back(r.P); sel.push_back(r.selectivity); }
            std::vector<std::pair<std::string, std::pair<std::vector<double>, std::vector<double>>>> series;
            series.push_back({"IAST selectivity", {p_all, sel}});
            std::string fname = prefix + "_Selectivity.svg";
            svg_scatter(fname, series, "Total pressure (kPa)", "Selectivity",
                        "Selectivity, IAST, " + names[0] + ":" + names[1] + "=" +
                        std::to_string(y[0]).substr(0,4) + ":" + std::to_string(1.0-y[0]).substr(0,4));
            std::cout << "  Saved: " << fname << "\n";
        }

        // Separation potential
        {
            std::vector<double> p_all, sp;
            for (auto& r : results) { p_all.push_back(r.P); sp.push_back(r.sep_potential); }
            std::vector<std::pair<std::string, std::pair<std::vector<double>, std::vector<double>>>> series;
            series.push_back({"Separation Potential", {p_all, sp}});
            std::string fname = prefix + "_Separation_Potential.svg";
            svg_scatter(fname, series, "Total pressure (kPa)", "Separation Potential",
                        "Separation Potential - " + names[0] + ":" + names[1]);
            std::cout << "  Saved: " << fname << "\n";
        }

    } else {
        auto results = ternary_iast(y, pressures, fitted_params[0], fitted_params[1], fitted_params[2]);
        
        std::vector<std::string> header = {
            "Total pressure (kPa)",
            names[0] + " Uptake (mmol/g)",
            names[1] + " Uptake (mmol/g)",
            names[2] + " Uptake (mmol/g)",
            names[0] + " mole fraction",
            names[1] + " mole fraction",
            names[2] + " mole fraction",
            "Selectivity"
        };
        std::vector<std::vector<double>> rows;
        for (auto& r : results) {
            rows.push_back({r.P, r.uptake1, r.uptake2, r.uptake3, r.x1, r.x2, r.x3, r.selectivity});
        }
        write_csv(csv_fname, header, rows);
        std::cout << "  Saved: " << csv_fname << "\n";

        // Phase 5: IAST plots (ternary)
        std::cout << "\n[5/5] Generating IAST result plots...\n";
        
        // Mixture uptake
        {
            std::vector<double> p_all, u1, u2, u3;
            for (auto& r : results) { p_all.push_back(r.P); u1.push_back(r.uptake1); u2.push_back(r.uptake2); u3.push_back(r.uptake3); }
            std::vector<std::pair<std::string, std::pair<std::vector<double>, std::vector<double>>>> series;
            series.push_back({names[0] + " IAST", {p_all, u1}});
            series.push_back({names[1] + " IAST", {p_all, u2}});
            series.push_back({names[2] + " IAST", {p_all, u3}});
            std::string fname = prefix + "_IAST_validation.svg";
            svg_scatter(fname, series, "Total pressure (kPa)", "Gas uptake (mmol/g)",
                        names[0] + ", " + names[1] + " and " + names[2] + " mixed-gas sorption");
            std::cout << "  Saved: " << fname << "\n";
        }

        // Selectivity
        {
            std::vector<double> p_all, sel;
            for (auto& r : results) { p_all.push_back(r.P); sel.push_back(r.selectivity); }
            std::vector<std::pair<std::string, std::pair<std::vector<double>, std::vector<double>>>> series;
            series.push_back({"IAST selectivity", {p_all, sel}});
            std::string fname = prefix + "_Selectivity.svg";
            svg_scatter(fname, series, "Total pressure (kPa)", "Selectivity",
                        "Selectivity, IAST, " + names[0] + ":" + names[1]);
            std::cout << "  Saved: " << fname << "\n";
        }
    }

    // ═══════════════════════════════════════════════════════
    // Write log
    // ═══════════════════════════════════════════════════════
    std::ofstream lf(std::filesystem::u8path(log_fname));
    lf << "============================================================\n";
    lf << "IAST Calculation Log (C++ version)\n";
    lf << "Date: " << timestamp() << "\n";
    lf << "Gas mixture: ";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) lf << " + ";
        lf << names[i];
    }
    lf << "\nComposition (y): ";
    for (size_t i = 0; i < y.size(); ++i) lf << " y" << (i+1) << "=" << y[i];
    lf << "\nPressures: " << pressures.front() << "-" << pressures.back()
       << " kPa (" << pressures.size() << " points)\n";
    lf << "============================================================\n\n";

    lf << "--- Pure-Component Isotherm Fitting ---\n\n";
    for (int i = 0; i < num_gases; ++i) {
        lf << "[" << (i+1) << "] " << names[i] << " - requested " << models[i]
           << ", selected " << selected_models[i] << ", " << gas_pressure[i].size() << " data points\n";
        if (fit_candidates[i].size() > 1) {
            lf << "  Candidate models:\n";
            for (auto& c : fit_candidates[i]) {
                lf << "    " << c.model << ": R^2=" << c.result.r_squared
                   << ", chi^2=" << c.result.chi_squared << ", BIC=" << c.score << "\n";
            }
        }
        lf << "  Parameter     Value\n";
        lf << "  A1            " << std::setw(12) << fitted_params[i][0] << "\n";
        lf << "  B1            " << std::setw(12) << fitted_params[i][1] << "\n";
        lf << "  C1            " << std::setw(12) << fitted_params[i][2] << "\n";
        lf << "  A2            " << std::setw(12) << fitted_params[i][3] << "\n";
        lf << "  B2            " << std::setw(12) << fitted_params[i][4] << "\n";
        lf << "  C2            " << std::setw(12) << fitted_params[i][5] << "\n";
        lf << "  R^2 = " << fit_results[i].r_squared << "\n";
        lf << "  chi^2 = " << fit_results[i].chi_squared << "\n";
        lf << "  iter = " << fit_results[i].iterations
           << ", converged = " << (fit_results[i].converged ? "yes" : "no") << "\n\n";
    }

    // Re-run IAST for log output (regenerate the same results)
    lf << "--- IAST Calculation Results ---\n\n";
    if (!ternary) {
        auto results = binary_iast(y[0], pressures, fitted_params[0], fitted_params[1]);
        for (auto& r : results) {
            lf << "P = " << std::setw(7) << r.P << " kPa\n";
            lf << "  " << names[0] << ": q = " << r.uptake1 << " mmol/g, x = " << r.x1 << "\n";
            lf << "  " << names[1] << ": q = " << r.uptake2 << " mmol/g, x = " << r.x2 << "\n";
            lf << "  Selectivity = " << r.selectivity << "\n";
            lf << "  Separation potential = " << r.sep_potential << " mmol/g\n\n";
        }
    } else {
        auto results = ternary_iast(y, pressures, fitted_params[0], fitted_params[1], fitted_params[2]);
        for (auto& r : results) {
            lf << "P = " << std::setw(7) << r.P << " kPa\n";
            lf << "  " << names[0] << ": q = " << r.uptake1 << " mmol/g, x = " << r.x1 << "\n";
            lf << "  " << names[1] << ": q = " << r.uptake2 << " mmol/g, x = " << r.x2 << "\n";
            lf << "  " << names[2] << ": q = " << r.uptake3 << " mmol/g, x = " << r.x3 << "\n";
            lf << "  Selectivity = " << r.selectivity << "\n\n";
        }
    }

    lf << "--- Output Files ---\n";
    lf << "  " << csv_fname << "\n";
    lf << "  " << prefix << "_Exp_isotherms.svg\n";
    lf << "  " << prefix << "_IAST_validation.svg\n";
    lf << "  " << prefix << "_Selectivity.svg\n";
    if (!ternary) lf << "  " << prefix << "_Separation_Potential.svg\n";
    lf << "  " << log_fname << "\n";
    lf.close();

    std::cout << "  Saved: " << log_fname << "\n";

    std::cout << "\n============================================================\n";
    std::cout << "Done! Output files:\n";
    std::cout << "  " << csv_fname << "\n";
    std::cout << "  " << prefix << "_Exp_isotherms.svg\n";
    std::cout << "  " << prefix << "_IAST_validation.svg\n";
    std::cout << "  " << prefix << "_Selectivity.svg\n";
    if (!ternary) std::cout << "  " << prefix << "_Separation_Potential.svg\n";
    std::cout << "  " << log_fname << "\n";
    std::cout << "============================================================\n";

    return 0;
}
