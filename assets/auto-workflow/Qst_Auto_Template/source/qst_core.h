// qst_core.h — Isosteric Heat of Adsorption (Qst) via Virial Equation
// Pure C++17, zero external dependencies
// Method: fit multi-temperature isotherm data to virial equation
//   ln(P) = ln(N) + (1/T)*Σ(a_i N^i) + Σ(b_i N^i)
//   Qst(N) = -R * Σ(a_i N^i)
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <functional>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <filesystem>

namespace qst {

const double R = 8.314;  // J/(mol·K)

// ─── Data Structures ─────────────────────────────────────────

struct DataSet {
    double T;                               // temperature (K)
    std::vector<double> N;                  // loading (mmol/g)
    std::vector<double> lnP;                // ln(Pressure/kPa)
};

struct FitResult {
    std::vector<double> params;             // a_0..a_m, b_0..b_n
    std::vector<double> param_errors;       // standard errors (1-sigma)
    double r_squared = 0;
    double chi_squared = 0;
    int iterations = 0;
    int n_points = 0;
    bool converged = false;
};

struct LoadingRange {
    double from = 0.0;
    double to = 0.0;
    double step = 0.0;
    bool valid = false;
};

struct AutoFitCandidate {
    int m_order = 0;
    int n_order = 0;
    FitResult result;
    double bic = 0.0;
    double penalty = 0.0;
    double score = 0.0;
    bool unstable_qst = false;
};

struct AutoFitResult {
    int selected_m = 0;
    int selected_n = 0;
    FitResult result;
    LoadingRange range;
    std::vector<AutoFitCandidate> candidates;
    std::vector<std::string> diagnostics;
};

// ─── Virial Equation ─────────────────────────────────────────

// Evaluate ln(P) from N and T
inline double virial_lnP(double N, double T, const double* p, int m, int n) {
    // params layout: p[0..m] = a_0..a_m,  p[m+1..m+1+n] = b_0..b_n
    double a_sum = 0.0, b_sum = 0.0;
    double Npow = 1.0;
    for (int i = 0; i <= m; ++i) {
        a_sum += p[i] * Npow;
        Npow *= N;
    }
    Npow = 1.0;
    for (int i = 0; i <= n; ++i) {
        b_sum += p[m + 1 + i] * Npow;
        Npow *= N;
    }
    return std::log(N) + a_sum / T + b_sum;
}

// Build analytical Jacobian for one data point
inline void virial_jacobian_row(double N, double T, double* J_row, int m, int n) {
    int np = m + 1 + n + 1;  // total params
    // ∂(lnP)/∂a_j = N^j / T
    double Npow = 1.0 / T;
    for (int i = 0; i <= m; ++i) {
        J_row[i] = Npow;
        Npow *= N;
    }
    // ∂(lnP)/∂b_j = N^j
    Npow = 1.0;
    for (int i = 0; i <= n; ++i) {
        J_row[m + 1 + i] = Npow;
        Npow *= N;
    }
}

// ─── Utilities ────────────────────────────────────────────────

inline void clip_positive(std::vector<double>& v, double lo = 1e-12) {
    for (auto& x : v) x = std::max(x, lo);
}

inline void solve_linear(int m, std::vector<double>& A, std::vector<double>& b,
                          std::vector<double>& x) {
    auto am = [&](int r, int c) -> double& { return A[r * m + c]; };
    for (int j = 0; j < m; ++j) {
        int pivot = j;
        double maxv = std::abs(am(j, j));
        for (int k = j + 1; k < m; ++k)
            if (std::abs(am(k, j)) > maxv) { maxv = std::abs(am(k, j)); pivot = k; }
        if (maxv < 1e-15) { am(j, j) = 1e-8; }
        else if (pivot != j) {
            for (int k = 0; k < m; ++k) std::swap(am(j, k), am(pivot, k));
            std::swap(b[j], b[pivot]);
        }
        for (int k = j + 1; k < m; ++k) {
            double f = am(k, j) / am(j, j);
            for (int c = j; c < m; ++c) am(k, c) -= f * am(j, c);
            b[k] -= f * b[j];
        }
    }
    for (int j = m - 1; j >= 0; --j) {
        double s = b[j];
        for (int k = j + 1; k < m; ++k) s -= am(j, k) * x[k];
        x[j] = s / am(j, j);
    }
}

inline void compute_goodness(const std::vector<double>& y, double ssq,
                              double& r_squared, double& chi_squared) {
    int n = (int)y.size();
    double y_mean = std::accumulate(y.begin(), y.end(), 0.0) / n;
    double ss_tot = 0.0;
    for (int i = 0; i < n; ++i) ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
    r_squared = ss_tot > 0 ? 1.0 - ssq / ss_tot : 1.0;
    chi_squared = ssq;
}

inline int total_points(const std::vector<DataSet>& datasets) {
    int total = 0;
    for (const auto& ds : datasets) total += (int)ds.N.size();
    return total;
}

inline LoadingRange overlapping_loading_range(const std::vector<DataSet>& datasets) {
    LoadingRange range;
    if (datasets.empty()) return range;

    double from = -1e100;
    double to = 1e100;
    for (const auto& ds : datasets) {
        if (ds.N.empty()) return range;
        auto mm = std::minmax_element(ds.N.begin(), ds.N.end());
        from = std::max(from, *mm.first);
        to = std::min(to, *mm.second);
    }

    if (from > 0 && to > from) {
        range.from = from;
        range.to = to;
        range.step = std::max((to - from) / 40.0, 1e-6);
        range.valid = true;
    }
    return range;
}

inline double bic_score(double chi_squared, int n_points, int n_params) {
    double n = std::max(1, n_points);
    double rss_per_point = std::max(chi_squared / n, 1e-18);
    return n * std::log(rss_per_point) + n_params * std::log(n);
}

// ─── Parameter Standard Errors via Analytical Jacobian ──────
// sigma^2 = ssq/(n-np); cov = sigma^2 * (J^T J)^{-1}; err = sqrt(diag(cov))
// Uses analytical Jacobian (virial_jacobian_row) — no numerical diff overhead
inline std::vector<double> compute_virial_errors(
    const std::vector<double>& all_N, const std::vector<double>& all_T,
    const std::vector<double>& all_y, const std::vector<double>& p,
    int m_order, int n_order, double ssq) {
    int n_total = (int)all_N.size();
    int np = m_order + 1 + n_order + 1;
    if (n_total <= np) return std::vector<double>(np, 0.0);

    // Build unscaled analytical Jacobian
    std::vector<double> J_row(np);
    std::vector<double> H(np * np, 0.0);
    for (int i = 0; i < n_total; ++i) {
        virial_jacobian_row(all_N[i], all_T[i], J_row.data(), m_order, n_order);
        for (int j = 0; j < np; ++j)
            for (int k = 0; k <= j; ++k)
                H[j * np + k] += J_row[j] * J_row[k];
    }
    for (int j = 0; j < np; ++j)
        for (int k = j + 1; k < np; ++k)
            H[k * np + j] = H[j * np + k];

    // Invert H via Gauss-Jordan
    int d = np;
    std::vector<double> work(d * d), inv(d * d, 0.0);
    for (int i = 0; i < d * d; ++i) work[i] = H[i];
    for (int i = 0; i < d; ++i) inv[i * d + i] = 1.0;
    auto w = [&](int r, int c)->double& { return work[r * d + c]; };
    auto iv = [&](int r, int c)->double& { return inv[r * d + c]; };
    for (int col = 0; col < d; ++col) {
        int pivot = col; double mv = std::abs(w(col, col));
        for (int r = col + 1; r < d; ++r)
            if (std::abs(w(r, col)) > mv) { mv = std::abs(w(r, col)); pivot = r; }
        if (mv < 1e-15) { for (int c = 0; c < d; ++c) iv(c, col) = 0.0; continue; }
        if (pivot != col)
            for (int c = 0; c < d; ++c) { std::swap(w(col, c), w(pivot, c)); std::swap(iv(col, c), iv(pivot, c)); }
        double piv = w(col, col);
        for (int c = 0; c < d; ++c) { w(col, c) /= piv; iv(col, c) /= piv; }
        for (int r = 0; r < d; ++r) {
            if (r == col) continue;
            double f = w(r, col);
            for (int c = 0; c < d; ++c) { w(r, c) -= f * w(col, c); iv(r, c) -= f * iv(col, c); }
        }
    }
    double sigma2 = ssq / (n_total - np);
    std::vector<double> errors(np);
    for (int j = 0; j < np; ++j)
        errors[j] = std::sqrt(std::max(0.0, sigma2 * iv(j, j)));
    return errors;
}

// ─── Qst Calculation ─────────────────────────────────────────

inline double qst_at_loading(double N, const double* a, int m) {
    // Qst = -R * Σ(a_i * N^i) / 1000  → kJ/mol
    // Note: R = 8.314 J/(mol·K), divide by 1000 for kJ/mol
    double sum = 0.0;
    double Npow = 1.0;
    for (int i = 0; i <= m; ++i) {
        sum += a[i] * Npow;
        Npow *= N;
    }
    return -R * sum / 1000.0;
}

// ─── Multi-Temperature Virial Fit (LM with analytical Jacobian) ──

// Core single-start LM
inline FitResult virial_fit_once(const std::vector<DataSet>& datasets,
                                  std::vector<double> p,
                                  int m_order, int n_order,
                                  int max_iter = 60, double ftol = 1e-10) {
    int np = m_order + 1 + n_order + 1;  // a_0..a_m, b_0..b_n
    int n_total = 0;
    for (auto& ds : datasets) n_total += (int)ds.N.size();

    // Build flat data arrays
    std::vector<double> all_N(n_total), all_T(n_total), all_y(n_total);
    int idx = 0;
    for (auto& ds : datasets) {
        for (int i = 0; i < (int)ds.N.size(); ++i) {
            all_N[idx] = ds.N[i];
            all_T[idx] = ds.T;
            all_y[idx] = ds.lnP[i];
            ++idx;
        }
    }

    // Compute initial residuals
    std::vector<double> r(n_total);
    double ssq = 0.0;
    for (int i = 0; i < n_total; ++i) {
        r[i] = virial_lnP(all_N[i], all_T[i], p.data(), m_order, n_order) - all_y[i];
        ssq += r[i] * r[i];
    }

    double lambda = 1e-3;
    double nu = 2.0;

    std::vector<double> J(n_total * np), g_scaled(np), delta_scaled(np);
    std::vector<double> H_scaled(np * np), A(np * np), b_vec(np), delta(np);
    std::vector<double> p_try(np), r_try(n_total), param_scale(np);
    std::vector<double> J_row(np);

    for (int iter = 0; iter < max_iter; ++iter) {
        // Marquardt scaling
        for (int j = 0; j < np; ++j)
            param_scale[j] = std::max(std::abs(p[j]), 1e-3);

        // Build scaled analytical Jacobian
        for (int i = 0; i < n_total; ++i) {
            virial_jacobian_row(all_N[i], all_T[i], J_row.data(), m_order, n_order);
            for (int j = 0; j < np; ++j)
                J[i * np + j] = J_row[j] * param_scale[j];
        }

        // H_scaled = J^T J, g_scaled = J^T r
        for (int j = 0; j < np; ++j) {
            g_scaled[j] = 0.0;
            for (int i = 0; i < n_total; ++i) g_scaled[j] += J[i * np + j] * r[i];
            for (int k = 0; k <= j; ++k) {
                double s = 0.0;
                for (int i = 0; i < n_total; ++i) s += J[i * np + j] * J[i * np + k];
                H_scaled[j * np + k] = s;
                H_scaled[k * np + j] = s;
            }
        }

        // (H_scaled + λI) * δ_scaled = -g_scaled
        for (int j = 0; j < np; ++j) {
            for (int k = 0; k < np; ++k)
                A[j * np + k] = H_scaled[j * np + k];
            A[j * np + j] += lambda;
            b_vec[j] = -g_scaled[j];
        }

        solve_linear(np, A, b_vec, delta_scaled);

        // Unscale
        for (int j = 0; j < np; ++j)
            delta[j] = delta_scaled[j] * param_scale[j];

        // Backtracking line search
        double alpha = 1.0;
        bool found = false;
        for (int ls = 0; ls < 20; ++ls) {
            for (int j = 0; j < np; ++j)
                p_try[j] = p[j] + alpha * delta[j];

            double ssq_try = 0.0;
            for (int i = 0; i < n_total; ++i) {
                r_try[i] = virial_lnP(all_N[i], all_T[i], p_try.data(), m_order, n_order) - all_y[i];
                ssq_try += r_try[i] * r_try[i];
            }

            if (ssq_try < ssq) {
                double old_ssq = ssq;
                p = p_try;
                ssq = ssq_try;
                r.swap(r_try);
                lambda = std::max(lambda / nu, 1e-8);
                nu = 2.0;
                found = true;

                double max_delta = 0.0;
                for (int j = 0; j < np; ++j)
                    max_delta = std::max(max_delta, std::abs(alpha * delta[j]) / std::max(std::abs(p[j]), 1.0));
                double improvement = old_ssq - ssq;
                if (max_delta < ftol || improvement < ftol * std::max(old_ssq, 1.0)) {
                    FitResult res;
                    res.params = p;
                    compute_goodness(all_y, ssq, res.r_squared, res.chi_squared);
                    res.param_errors = compute_virial_errors(all_N, all_T, all_y, p, m_order, n_order, ssq);
                    res.iterations = iter + 1;
                    res.n_points = n_total;
                    res.converged = true;
                    return res;
                }
                break;
            }
            alpha *= 0.5;
        }

        if (!found) {
            lambda *= nu;
            nu *= 2.0;
        }
    }

    FitResult res;
    res.params = p;
    compute_goodness(all_y, ssq, res.r_squared, res.chi_squared);
    res.param_errors = compute_virial_errors(all_N, all_T, all_y, p, m_order, n_order, ssq);
    res.iterations = max_iter;
    res.n_points = n_total;
    res.converged = false;
    return res;
}

// Multi-start wrapper
inline FitResult virial_fit(const std::vector<DataSet>& datasets,
                             std::vector<double> init_params,
                             int m_order = 5, int n_order = 2,
                             int max_iter = 300, double ftol = 1e-10,
                             int num_starts = 5) {
    int np = m_order + 1 + n_order + 1;
    std::mt19937 rng(7654321);
    std::uniform_real_distribution<double> dist(-2.302585, 2.302585);

    FitResult best;
    double best_r2 = -1.0;
    int iter_per_start = std::max(max_iter / num_starts, 30);

    for (int start = 0; start < num_starts; ++start) {
        auto p = init_params;
        if (start > 0) {
            for (int j = 0; j < np; ++j)
                p[j] += dist(rng) * std::max(std::abs(p[j]), 1.0);
            // a_1..a_m should typically be negative for Qst>0, but we allow exploration
        }
        auto res = virial_fit_once(datasets, p, m_order, n_order, iter_per_start, ftol);
        if (res.r_squared > best_r2) {
            best_r2 = res.r_squared;
            best = res;
        }
        if (best_r2 > 0.9999) break;
    }

    if (best_r2 < 0.99 && best.iterations >= iter_per_start) {
        int remaining = max_iter - num_starts * iter_per_start;
        if (remaining > 20) {
            auto res2 = virial_fit_once(datasets, best.params, m_order, n_order, remaining, ftol);
            if (res2.r_squared > best_r2) best = res2;
        }
    }

    return best;
}

inline bool qst_curve_unstable(const FitResult& fit, int m_order, const LoadingRange& range) {
    if (!range.valid || fit.params.empty()) return true;
    double prev = qst_at_loading(range.from, fit.params.data(), m_order);
    if (!std::isfinite(prev) || prev < -50.0 || prev > 300.0) return true;
    int direction_changes = 0;
    int last_sign = 0;
    for (int i = 1; i <= 40; ++i) {
        double n = range.from + (range.to - range.from) * i / 40.0;
        double curr = qst_at_loading(n, fit.params.data(), m_order);
        if (!std::isfinite(curr) || curr < -50.0 || curr > 300.0) return true;
        double d = curr - prev;
        int sign = (d > 1e-5) ? 1 : ((d < -1e-5) ? -1 : 0);
        if (sign != 0 && last_sign != 0 && sign != last_sign) ++direction_changes;
        if (sign != 0) last_sign = sign;
        prev = curr;
    }
    return direction_changes > 3;
}

inline double error_penalty(const FitResult& fit) {
    if (fit.param_errors.empty() || fit.params.empty()) return 0.0;
    int noisy = 0;
    for (size_t i = 0; i < fit.param_errors.size() && i < fit.params.size(); ++i) {
        double denom = std::max(std::abs(fit.params[i]), 1.0);
        if (fit.param_errors[i] / denom > 10.0) ++noisy;
    }
    return noisy * 10.0;
}

inline AutoFitResult auto_virial_fit(const std::vector<DataSet>& datasets,
                                     int max_m_order = 5,
                                     int max_n_order = 2) {
    AutoFitResult out;
    out.range = overlapping_loading_range(datasets);
    if (!out.range.valid) {
        out.diagnostics.push_back("No shared loading range across all temperature datasets.");
    }

    int n_total = total_points(datasets);
    max_m_order = std::max(1, max_m_order);
    max_n_order = std::max(0, max_n_order);

    std::vector<std::pair<int, int>> orders;
    for (int m = 1; m <= max_m_order; ++m) {
        for (int n = 0; n <= std::min(max_n_order, m); ++n) {
            int np = m + 1 + n + 1;
            if (n_total > np + 2) orders.push_back({m, n});
        }
    }
    if (orders.empty()) {
        orders.push_back({1, 0});
        out.diagnostics.push_back("Data points are limited; using the lowest available virial order.");
    }

    bool has_best = false;
    double best_score = 0.0;
    for (auto [m, n] : orders) {
        int np = m + 1 + n + 1;
        std::vector<double> init(np, 0.0);
        init[0] = -1000.0;

        FitResult fit = virial_fit(datasets, init, m, n, 600, 1e-10, 10);
        double bic = bic_score(fit.chi_squared, fit.n_points, np);
        bool unstable = qst_curve_unstable(fit, m, out.range);
        double penalty = error_penalty(fit) + (unstable ? 100.0 : 0.0);
        if (!fit.converged) penalty += 25.0;
        if (fit.r_squared < 0.99) penalty += (0.99 - fit.r_squared) * 1000.0;
        double score = bic + penalty;

        out.candidates.push_back({m, n, fit, bic, penalty, score, unstable});
        if (!has_best || score < best_score ||
            (std::abs(score - best_score) < 1e-9 && fit.r_squared > out.result.r_squared)) {
            has_best = true;
            best_score = score;
            out.selected_m = m;
            out.selected_n = n;
            out.result = fit;
        }
    }

    if (out.result.r_squared < 0.99) {
        out.diagnostics.push_back("Best fit R^2 is below 0.99; inspect data quality and low-loading coverage.");
    }
    if (!out.candidates.empty() && qst_curve_unstable(out.result, out.selected_m, out.range)) {
        out.diagnostics.push_back("Selected Qst curve may be unstable; consider lower orders or smoother data.");
    }
    return out;
}

// ─── CSV I/O ─────────────────────────────────────────────────

inline DataSet read_isotherm_csv(const std::string& path, double T) {
    DataSet ds;
    ds.T = T;
    std::ifstream f(path);
    if (!f.is_open()) return ds;
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::stringstream ss(line);
        double P, N;
        if (ss >> P >> N) {
            if (P > 0 && N > 0) {
                ds.N.push_back(N);
                ds.lnP.push_back(std::log(P));
            }
        }
    }
    return ds;
}

inline void write_qst_csv(const std::string& path,
                           const std::vector<double>& loadings,
                           const std::vector<double>& qst_vals) {
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "Loading (mmol/g),Qst (kJ/mol)\n";
    for (size_t i = 0; i < loadings.size(); ++i)
        f << loadings[i] << "," << qst_vals[i] << "\n";
}

inline void write_qst_csv(const std::string& path,
                           const std::vector<double>& loadings,
                           const std::vector<double>& qst_vals,
                           const std::vector<double>& qst_errs) {
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "Loading (mmol/g),Qst (kJ/mol),Qst Std Error (kJ/mol)\n";
    for (size_t i = 0; i < loadings.size(); ++i) {
        double err = i < qst_errs.size() ? qst_errs[i] : 0.0;
        f << loadings[i] << "," << qst_vals[i] << "," << err << "\n";
    }
}

inline std::filesystem::path& output_base_dir() {
    static std::filesystem::path dir;
    return dir;
}

inline void set_output_base_dir(const std::filesystem::path& dir) {
    output_base_dir() = dir;
}

inline std::filesystem::path output_dir(const char* folder) {
    std::filesystem::path dir = output_base_dir().empty()
        ? std::filesystem::path(folder)
        : (output_base_dir() / folder);
    std::filesystem::create_directories(dir);
    return dir;
}

inline std::string final_output_path(const std::string& name) {
    std::filesystem::path dir = output_dir("Qst_final");
    std::filesystem::path file = std::filesystem::path(name).filename();
    return (dir / file).generic_string();
}

inline std::string final_output_prefix(const std::string& prefix) {
    std::filesystem::path dir = output_dir("Qst_final");
    std::filesystem::path file = std::filesystem::path(prefix).filename();
    return (dir / file).generic_string();
}

inline double qst_error_at_loading(double N, const FitResult& fit, int m_order) {
    if (fit.param_errors.empty()) return 0.0;
    double variance = 0.0;
    double Npow = 1.0;
    for (int i = 0; i <= m_order && i < (int)fit.param_errors.size(); ++i) {
        double term = fit.param_errors[i] * Npow;
        variance += term * term;
        Npow *= N;
    }
    return R * std::sqrt(std::max(0.0, variance)) / 1000.0;
}

// ─── SVG Plot Generation ─────────────────────────────────────

inline void svg_virial_fit(const std::string& path,
                            const std::vector<DataSet>& datasets,
                            const FitResult& fit,
                            int m_order, int n_order,
                            int w = 800, int h = 600) {
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " << w << " " << h << "'>\n";
    f << "<rect width='100%' height='100%' fill='white'/>\n";

    // Collect all N values for axis range
    double N_min = 1e100, N_max = -1e100, L_min = 1e100, L_max = -1e100;
    for (auto& ds : datasets) {
        for (size_t i = 0; i < ds.N.size(); ++i) {
            N_min = std::min(N_min, ds.N[i]);
            N_max = std::max(N_max, ds.N[i]);
            L_min = std::min(L_min, ds.lnP[i]);
            L_max = std::max(L_max, ds.lnP[i]);
        }
    }
    double pad = 0.05;
    double N_rng = N_max - N_min;
    double L_rng = L_max - L_min;
    N_min -= N_rng * pad; N_max += N_rng * pad;
    L_min -= L_rng * pad; L_max += L_rng * pad;

    auto tx = [&](double N) { return 80 + (N - N_min) / (N_max - N_min) * (w - 140); };
    auto ty = [&](double L) { return h - 60 - (L - L_min) / (L_max - L_min) * (h - 120); };

    // Axes
    f << "<line x1='70' y1='" << h-50 << "' x2='" << w-50 << "' y2='" << h-50 << "' stroke='black'/>\n";
    f << "<line x1='70' y1='50' x2='70' y2='" << h-50 << "' stroke='black'/>\n";

    // Axis labels
    f << "<text x='" << w/2 << "' y='" << h-8 << "' text-anchor='middle' font-size='14'>Loading (mmol/g)</text>\n";
    f << "<text x='15' y='" << h/2 << "' text-anchor='middle' font-size='14' transform='rotate(-90,15," << h/2 << ")'>ln(P / kPa)</text>\n";

    // Tick labels
    for (int i = 0; i <= 5; ++i) {
        double Nv = N_min + i * (N_max - N_min) / 5.0;
        f << "<text x='" << tx(Nv) << "' y='" << h-32 << "' text-anchor='middle' font-size='11'>" << std::fixed << std::setprecision(1) << Nv << "</text>\n";
    }
    for (int i = 0; i <= 5; ++i) {
        double Lv = L_min + i * (L_max - L_min) / 5.0;
        f << "<text x='62' y='" << ty(Lv)+4 << "' text-anchor='end' font-size='11'>" << std::fixed << std::setprecision(1) << Lv << "</text>\n";
    }

    // Colors for different temperatures
    const char* colors[] = {"#1f77b4","#d62728","#2ca02c","#ff7f0e","#9467bd"};

    // Fitted curves first, so hollow experimental points stay visible on top.
    for (int di = 0; di < (int)datasets.size(); ++di) {
        f << "<polyline fill='none' stroke='" << colors[di%5] << "' stroke-width='2.6' stroke-linejoin='round' stroke-linecap='round' points='";
        int npts = 200;
        for (int i = 0; i <= npts; ++i) {
            double Nv = N_min + i * (N_max - N_min) / npts;
            double Lv = virial_lnP(Nv, datasets[di].T, fit.params.data(), m_order, n_order);
            f << tx(Nv) << "," << ty(Lv) << " ";
        }
        f << "'/>\n";
    }

    // Experimental data points
    for (int di = 0; di < (int)datasets.size(); ++di) {
        for (size_t i = 0; i < datasets[di].N.size(); ++i) {
            double cx = tx(datasets[di].N[i]), cy = ty(datasets[di].lnP[i]);
            f << "<circle cx='" << cx << "' cy='" << cy << "' r='4' fill='white' stroke='" << colors[di%5] << "' stroke-width='1.8'/>\n";
        }
    }

    // Legend
    for (int di = 0; di < (int)datasets.size(); ++di) {
        int lx = w - 190, ly = 70 + di * 24;
        f << "<line x1='" << lx << "' y1='" << ly << "' x2='" << lx+18 << "' y2='" << ly << "' stroke='" << colors[di%5] << "' stroke-width='2.6'/>\n";
        f << "<circle cx='" << lx+9 << "' cy='" << ly << "' r='4' fill='white' stroke='" << colors[di%5] << "' stroke-width='1.6'/>\n";
        f << "<text x='" << lx+25 << "' y='" << ly+4 << "' font-size='12'>" << (int)datasets[di].T << " K Exp + Fit</text>\n";
    }

    // R² annotation
    f << "<text x='" << w-20 << "' y='" << h-70 << "' text-anchor='end' font-size='13'>R² = "
      << std::fixed << std::setprecision(6) << fit.r_squared << "</text>\n";

    f << "</svg>\n";
}

inline void svg_qst_curve(const std::string& path,
                           const std::vector<double>& loadings,
                           const std::vector<double>& qst_vals,
                           int w = 800, int h = 600) {
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " << w << " " << h << "'>\n";
    f << "<rect width='100%' height='100%' fill='white'/>\n";

    double N_min = loadings.front(), N_max = loadings.back();
    double Q_min = 1e100, Q_max = -1e100;
    for (auto& q : qst_vals) {
        Q_min = std::min(Q_min, q);
        Q_max = std::max(Q_max, q);
    }
    double QR = Q_max - Q_min; if (QR < 0.1) QR = 1.0;
    Q_min -= QR * 0.1; Q_max += QR * 0.1;
    double NR = N_max - N_min;

    auto tx = [&](double N) { return 80 + (N - N_min) / NR * (w - 140); };
    auto ty = [&](double Q) { return h - 60 - (Q - Q_min) / (Q_max - Q_min) * (h - 120); };

    f << "<line x1='70' y1='" << h-50 << "' x2='" << w-50 << "' y2='" << h-50 << "' stroke='black'/>\n";
    f << "<line x1='70' y1='50' x2='70' y2='" << h-50 << "' stroke='black'/>\n";
    f << "<text x='" << w/2 << "' y='" << h-8 << "' text-anchor='middle' font-size='14'>Loading</text>\n";
    f << "<text x='15' y='" << h/2 << "' text-anchor='middle' font-size='14' transform='rotate(-90,15," << h/2 << ")'>Qst (kJ/mol)</text>\n";

    for (int i = 0; i <= 5; ++i) {
        double Nv = N_min + i * NR / 5.0;
        f << "<text x='" << tx(Nv) << "' y='" << h-32 << "' text-anchor='middle' font-size='11'>" << std::fixed << std::setprecision(2) << Nv << "</text>\n";
    }
    for (int i = 0; i <= 5; ++i) {
        double Qv = Q_min + i * (Q_max - Q_min) / 5.0;
        f << "<text x='62' y='" << ty(Qv)+4 << "' text-anchor='end' font-size='11'>" << std::fixed << std::setprecision(1) << Qv << "</text>\n";
    }

    // Qst curve
    f << "<polyline fill='none' stroke='#d62728' stroke-width='2' points='";
    for (size_t i = 0; i < loadings.size(); ++i)
        f << tx(loadings[i]) << "," << ty(qst_vals[i]) << " ";
    f << "'/>\n";

    // Fill area under curve
    f << "<polygon fill='#d62728' fill-opacity='0.15' points='";
    f << tx(loadings[0]) << "," << ty(Q_min) << " ";
    for (size_t i = 0; i < loadings.size(); ++i)
        f << tx(loadings[i]) << "," << ty(qst_vals[i]) << " ";
    f << tx(loadings.back()) << "," << ty(Q_min) << "'/>\n";

    f << "</svg>\n";
}

} // namespace qst
