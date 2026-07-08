// iast_core.h — Self-contained IAST calculation header
// Pure C++17, zero external dependencies
#pragma once

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

namespace iast {

// ─── Isotherm Models ─────────────────────────────────────────
// Parameter vector: {A1, B1, C1, A2, B2, C2}

inline double uptake(double P, const double* p) {
    double q = 0.0;
    if (p[0] > 0 && p[1] > 0 && p[2] > 0) {
        double bc = p[1] * std::pow(P, p[2]);
        q += p[0] * bc / (1.0 + bc);
    }
    if (p[3] > 0 && p[4] > 0 && p[5] > 0) {
        double bc = p[4] * std::pow(P, p[5]);
        q += p[3] * bc / (1.0 + bc);
    }
    return std::max(q, 0.0);
}

inline double spreading_pressure(double P, const double* p) {
    double pi = 0.0;
    if (p[0] > 0 && p[1] > 0 && p[2] > 0)
        pi += (p[0] / p[2]) * std::log(1.0 + p[1] * std::pow(P, p[2]));
    if (p[3] > 0 && p[4] > 0 && p[5] > 0)
        pi += (p[3] / p[5]) * std::log(1.0 + p[4] * std::pow(P, p[5]));
    return pi;
}

// ─── Levenberg-Marquardt Optimizer with positivity constraints ────

struct FitResult {
    std::vector<double> params;
    double r_squared = 0.0;
    double chi_squared = 0.0;
    int iterations = 0;
    bool converged = false;
};

struct AutoFitCandidate {
    std::string model;
    FitResult result;
    double score = 0.0;
};

struct AutoFitResult {
    std::string requested_model;
    std::string selected_model;
    FitResult result;
    std::vector<AutoFitCandidate> candidates;
};

using ModelFn = std::function<double(double, const std::vector<double>&)>;

inline ModelFn make_model_fn(const std::string& model_name);

inline void clip_positive(std::vector<double>& p, double lo = 1e-6) {
    for (auto& v : p) v = std::max(v, lo);
}

// Gaussian elimination solver
inline void solve_linear(int m, std::vector<double>& A, std::vector<double>& b, std::vector<double>& x) {
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

// ─── Compute goodness-of-fit from residuals ────────────────

inline void compute_goodness(const std::vector<double>& y, double ssq,
                              double& r_squared, double& chi_squared) {
    int n = (int)y.size();
    double y_mean = std::accumulate(y.begin(), y.end(), 0.0) / n;
    double ss_tot = 0.0;
    for (int i = 0; i < n; ++i) ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
    r_squared = ss_tot > 0 ? 1.0 - ssq / ss_tot : 1.0;
    chi_squared = ssq;
}

inline int active_param_count(const std::string& model_name) {
    if (model_name == "SSL") return 2;
    if (model_name == "DSL") return 4;
    if (model_name == "SSLF") return 3;
    return 6;
}

inline std::vector<double> model_init_params(const std::string& model_name,
                                             const std::vector<double>& user_params) {
    std::vector<double> p = user_params;
    if (p.size() < 6) p.resize(6, 1.0);
    if (model_name == "SSL")  { p[2] = 1.0; p[3] = 1.0; p[4] = 1.0; p[5] = 1.0; }
    if (model_name == "DSL")  { p[2] = 1.0; p[5] = 1.0; }
    if (model_name == "SSLF") { p[3] = 1.0; p[4] = 1.0; p[5] = 1.0; }
    clip_positive(p);
    return p;
}

inline double bic_score(double chi_squared, int n_points, int n_params) {
    double n = std::max(1, n_points);
    double rss_per_point = std::max(chi_squared / n, 1e-18);
    return n * std::log(rss_per_point) + n_params * std::log(n);
}

inline std::vector<double> materialize_model_params(const std::string& model_name,
                                                    const std::vector<double>& fitted_params) {
    std::vector<double> p = fitted_params;
    if (p.size() < 6) p.resize(6, 1.0);
    if (model_name == "SSL") {
        p[2] = 1.0;
        p[3] = 0.0;
        p[4] = 1.0;
        p[5] = 1.0;
    } else if (model_name == "DSL") {
        p[2] = 1.0;
        p[5] = 1.0;
    } else if (model_name == "SSLF") {
        p[3] = 0.0;
        p[4] = 1.0;
        p[5] = 1.0;
    }
    return p;
}

// Compute parameter standard errors from final Jacobian
inline std::vector<double> compute_param_errors(const std::vector<double>& x,
    const std::vector<double>& y, ModelFn model, const std::vector<double>& p) {
    int n = (int)x.size();
    int m = (int)p.size();
    double eps = std::sqrt(std::numeric_limits<double>::epsilon());
    std::vector<double> J(n * m);
    std::vector<double> p_try(m);

    for (int j = 0; j < m; ++j) {
        double h = eps * std::max(std::abs(p[j]), 1e-3);
        double orig = p[j];
        p_try = p;
        for (int i = 0; i < n; ++i) {
            p_try[j] = orig + h;
            double f_plus = model(x[i], p_try) - y[i];
            p_try[j] = orig - h;
            double f_minus = model(x[i], p_try) - y[i];
            J[i * m + j] = (f_plus - f_minus) / (2.0 * h);
        }
        p_try[j] = orig;
    }

    // J^T J
    std::vector<double> JTJ(m * m, 0.0);
    for (int j = 0; j < m; ++j) {
        for (int k = 0; k < m; ++k) {
            double s = 0.0;
            for (int i = 0; i < n; ++i) s += J[i * m + j] * J[i * m + k];
            JTJ[j * m + k] = s;
        }
    }

    // Gauss-Jordan inversion of JTJ
    std::vector<double> inv(m * m, 0.0);
    for (int j = 0; j < m; ++j) inv[j * m + j] = 1.0;

    std::vector<double> aug(m * m);
    for (int i = 0; i < m * m; ++i) aug[i] = JTJ[i];

    for (int col = 0; col < m; ++col) {
        int pivot = col;
        double maxv = std::abs(aug[col * m + col]);
        for (int row = col + 1; row < m; ++row) {
            if (std::abs(aug[row * m + col]) > maxv) {
                maxv = std::abs(aug[row * m + col]);
                pivot = row;
            }
        }
        if (maxv < 1e-15) continue;  // near-singular, skip
        if (pivot != col) {
            for (int k = 0; k < m; ++k) {
                std::swap(aug[col * m + k], aug[pivot * m + k]);
                std::swap(inv[col * m + k], inv[pivot * m + k]);
            }
        }
        double piv = aug[col * m + col];
        for (int k = 0; k < m; ++k) {
            aug[col * m + k] /= piv;
            inv[col * m + k] /= piv;
        }
        for (int row = 0; row < m; ++row) {
            if (row == col) continue;
            double f = aug[row * m + col];
            for (int k = 0; k < m; ++k) {
                aug[row * m + k] -= f * aug[col * m + k];
                inv[row * m + k] -= f * inv[col * m + k];
            }
        }
    }

    // Residuals for sigma^2
    double ssq = 0.0;
    for (int i = 0; i < n; ++i) {
        double ri = model(x[i], p) - y[i];
        ssq += ri * ri;
    }
    double sigma2 = ssq / std::max(1, n - m);

    std::vector<double> errors(m);
    for (int j = 0; j < m; ++j) {
        double v = inv[j * m + j];
        errors[j] = (v > 0) ? std::sqrt(sigma2 * v) : 0.0;
    }
    return errors;
}

// Core single-start LM with Marquardt parameter scaling
inline FitResult lm_fit_once(const std::vector<double>& x, const std::vector<double>& y,
                      ModelFn model, std::vector<double> p,
                      int max_iter = 60, double ftol = 1e-10) {
    int n = (int)x.size();
    int m = (int)p.size();

    std::vector<double> r(n);
    double ssq = 0.0;
    for (int i = 0; i < n; ++i) {
        r[i] = model(x[i], p) - y[i];
        ssq += r[i] * r[i];
    }

    double lambda = 1e-3;
    double nu = 2.0;
    double eps = std::sqrt(std::numeric_limits<double>::epsilon());

    std::vector<double> J(n * m), g_scaled(m), delta_scaled(m), p_try(m), r_try(n);
    std::vector<double> H_scaled(m * m), A(m * m), b(m), delta(m);
    std::vector<double> param_scale(m);

    for (int iter = 0; iter < max_iter; ++iter) {
        // Marquardt parameter scaling: normalize each column of J by param_scale
        for (int j = 0; j < m; ++j)
            param_scale[j] = std::max(std::abs(p[j]), 1e-3);

        // Build scaled Jacobian via central differences
        for (int j = 0; j < m; ++j) {
            double h = eps * param_scale[j];
            double orig = p[j];
            p[j] = orig + h;
            for (int i = 0; i < n; ++i) r_try[i] = model(x[i], p) - y[i];
            p[j] = orig - h;
            for (int i = 0; i < n; ++i) {
                double m2 = model(x[i], p) - y[i];
                J[i * m + j] = (r_try[i] - m2) / (2.0 * h) * param_scale[j];
            }
            p[j] = orig;
        }

        // H_scaled = J^T J,  g_scaled = J^T r
        for (int j = 0; j < m; ++j) {
            g_scaled[j] = 0.0;
            for (int i = 0; i < n; ++i) g_scaled[j] += J[i * m + j] * r[i];
            for (int k = 0; k <= j; ++k) {
                double s = 0.0;
                for (int i = 0; i < n; ++i) s += J[i * m + j] * J[i * m + k];
                H_scaled[j * m + k] = s;
                H_scaled[k * m + j] = s;
            }
        }

        // (H_scaled + lambda*I) * delta_scaled = -g_scaled
        for (int j = 0; j < m; ++j) {
            for (int k = 0; k < m; ++k)
                A[j * m + k] = H_scaled[j * m + k];
            A[j * m + j] += lambda;
            b[j] = -g_scaled[j];
        }

        solve_linear(m, A, b, delta_scaled);

        // Unscale: delta = delta_scaled * param_scale
        for (int j = 0; j < m; ++j)
            delta[j] = delta_scaled[j] * param_scale[j];

        // Backtracking line search
        double alpha = 1.0;
        bool found = false;
        for (int ls = 0; ls < 20; ++ls) {
            for (int j = 0; j < m; ++j)
                p_try[j] = p[j] + alpha * delta[j];
            clip_positive(p_try);

            double ssq_try = 0.0;
            for (int i = 0; i < n; ++i) {
                r_try[i] = model(x[i], p_try) - y[i];
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
                for (int j = 0; j < m; ++j)
                    max_delta = std::max(max_delta, std::abs(alpha * delta[j]) / std::max(std::abs(p[j]), 1.0));
                double improvement = old_ssq - ssq;
                if (max_delta < ftol || improvement < ftol * std::max(old_ssq, 1.0)) {
                    FitResult res;
                    res.params = p;
                    compute_goodness(y, ssq, res.r_squared, res.chi_squared);
                    res.iterations = iter + 1;
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
    compute_goodness(y, ssq, res.r_squared, res.chi_squared);
    res.iterations = max_iter;
    res.converged = false;
    return res;
}

// Multi-start LM: tries N perturbed starting points, picks best R²
// Perturbation is multiplicative: each param × rand in [0.1, 10]
inline FitResult lm_fit(const std::vector<double>& x, const std::vector<double>& y,
                 ModelFn model, std::vector<double> init_params,
                 int max_iter = 300, double ftol = 1e-10, int num_starts = 5) {
    int m = (int)init_params.size();
    clip_positive(init_params);

    // Seedable RNG for perturbation (log-uniform in [0.1, 10])
    std::mt19937 rng(1234567);
    std::uniform_real_distribution<double> dist(-2.302585, 2.302585);  // log(0.1) .. log(10)

    FitResult best;
    double best_r2 = -1.0;
    int iter_per_start = std::max(max_iter / num_starts, 30);

    for (int start = 0; start < num_starts; ++start) {
        auto p = init_params;
        if (start > 0) {
            for (int j = 0; j < m; ++j)
                p[j] *= std::exp(dist(rng));
            clip_positive(p);
        }

        auto res = lm_fit_once(x, y, model, p, iter_per_start, ftol);
        if (res.r_squared > best_r2) {
            best_r2 = res.r_squared;
            best = res;
        }
        if (best_r2 > 0.9999) break;  // excellent fit, stop early
    }

    // If best is still mediocre and we used all budget, polish it
    if (best_r2 < 0.99 && best.iterations >= iter_per_start) {
        int remaining = max_iter - num_starts * iter_per_start;
        if (remaining > 20) {
            auto res2 = lm_fit_once(x, y, model, best.params, remaining, ftol);
            if (res2.r_squared > best_r2) best = res2;
        }
    }

    return best;
}

inline AutoFitResult fit_model_auto(const std::vector<double>& x,
                                    const std::vector<double>& y,
                                    const std::string& requested_model,
                                    const std::vector<double>& init_params,
                                    int max_iter = 300,
                                    double ftol = 1e-10,
                                    int num_starts = 10) {
    const std::vector<std::string> all_models = {"SSL", "DSL", "SSLF", "DSLF"};
    std::vector<std::string> model_names;
    if (requested_model == "Auto" || requested_model == "AUTO" || requested_model == "auto") {
        model_names = all_models;
    } else {
        model_names = {requested_model};
    }

    AutoFitResult out;
    out.requested_model = requested_model;

    auto run_candidate = [&](const std::string& model_name) {
        auto fn = make_model_fn(model_name);
        auto ip = model_init_params(model_name, init_params);
        auto res = lm_fit(x, y, fn, ip, max_iter, ftol, num_starts);
        double score = bic_score(res.chi_squared, (int)y.size(), active_param_count(model_name));
        // A Freundlich exponent pinned to the positivity floor is a degenerate
        // solution: it behaves like a nearly pressure-independent offset and
        // makes the spreading-pressure root ill-conditioned.  Exclude that
        // candidate so Auto can fall back to a physically usable model.
        const double exponent_floor = 1.0001e-6;
        bool degenerate =
            (model_name == "SSLF" && res.params.size() > 2 && res.params[2] <= exponent_floor) ||
            (model_name == "DSLF" && res.params.size() > 5 &&
             (res.params[2] <= exponent_floor || res.params[5] <= exponent_floor));
        if (degenerate) score = std::numeric_limits<double>::infinity();
        out.candidates.push_back({model_name, res, score});
    };

    for (const auto& model_name : model_names) run_candidate(model_name);

    if (!(requested_model == "Auto" || requested_model == "AUTO" || requested_model == "auto") &&
        !out.candidates.empty() && out.candidates.front().result.r_squared < 0.99) {
        for (const auto& model_name : all_models) {
            if (model_name != requested_model) run_candidate(model_name);
        }
    }

    bool has_best = false;
    double best_score = 0.0;
    for (const auto& candidate : out.candidates) {
        if (!has_best || candidate.score < best_score ||
            (std::abs(candidate.score - best_score) < 1e-9 &&
             candidate.result.r_squared > out.result.r_squared)) {
            has_best = true;
            best_score = candidate.score;
            out.selected_model = candidate.model;
            out.result = candidate.result;
        }
    }
    out.result.params = materialize_model_params(out.selected_model, out.result.params);

    return out;
}

// ─── Root Finding ────────────────────────────────────────────

inline double brent_fzero(std::function<double(double)> f, double a, double b,
                   double tol = 1e-10, int max_iter = 100) {
    double fa = f(a), fb = f(b);
    if (fa * fb > 0) {
        for (int k = 0; k < 20; ++k) {
            a -= (b - a) * 0.5;
            b += (b - a) * 0.5;
            fa = f(a); fb = f(b);
            if (fa * fb < 0) break;
        }
        if (fa * fb > 0) return (a + b) * 0.5;
    }
    if (std::abs(fa) < std::abs(fb)) { std::swap(a, b); std::swap(fa, fb); }

    double c = a, fc = fa;
    bool mflag = true;
    double s = b, fs = fb, d = 0.0;

    for (int iter = 0; iter < max_iter; ++iter) {
        if (fb * fc > 0) { c = a; fc = fa; d = b - a; mflag = true; }
        if (std::abs(fc) < std::abs(fb)) { a = b; b = c; c = a; fa = fb; fb = fc; fc = fa; }

        double tol1 = 2.0 * std::numeric_limits<double>::epsilon() * std::abs(b) + 0.5 * tol;
        double xm = 0.5 * (c - b);
        if (std::abs(xm) <= tol1 || fb == 0.0) return b;

        if (std::abs(fa - fb) > 1e-16 && std::abs(fb - fc) > 1e-16) {
            s = (a * fb * fc) / ((fa - fb) * (fa - fc)) +
                (b * fa * fc) / ((fb - fa) * (fb - fc)) +
                (c * fa * fb) / ((fc - fa) * (fc - fb));
        } else {
            s = b - fb * (b - a) / (fb - fa);
        }

        if ((s < (3.0 * a + b) / 4.0 || s > b) ||
            (mflag && std::abs(s - b) >= std::abs(b - c) / 2.0) ||
            (!mflag && std::abs(s - b) >= std::abs(c - d) / 2.0) ||
            (mflag && std::abs(b - c) < tol) ||
            (!mflag && std::abs(c - d) < tol)) {
            s = (a + b) / 2.0;
            mflag = true;
        } else {
            mflag = false;
        }

        fs = f(s);
        d = c; c = b; fc = fb;
        if (fa * fs < 0) { b = s; fb = fs; }
        else { a = s; fa = fs; }
        if (std::abs(fa) < std::abs(fb)) { std::swap(a, b); std::swap(fa, fb); }
    }
    return b;
}

// ─── IAST Solvers ────────────────────────────────────────────

struct IASTBinary {
    double P, uptake1, uptake2, x1, x2, selectivity, sep_potential;
};

inline std::vector<IASTBinary> binary_iast(double y1,
    const std::vector<double>& pressures,
    const std::vector<double>& p1,
    const std::vector<double>& p2) {
    std::vector<IASTBinary> results;
    double y2 = 1.0 - y1;

    for (double P : pressures) {
        auto f = [&](double x1) -> double {
            if (x1 <= 1e-10 || x1 >= 0.9999999) return 1e10;
            double P1 = P * y1 / x1;
            double P2 = P * y2 / (1.0 - x1);
            return spreading_pressure(P1, p1.data()) - spreading_pressure(P2, p2.data());
        };

        double x1_sol;
        double a = 1e-4, b = 0.9999;
        double fa = f(a), fb = f(b);
        bool found_bracket = false;
        if (fa * fb < 0) {
            found_bracket = true;
        } else {
            for (int k = 1; k < 99; ++k) {
                double t = k * 0.01;
                double ft = f(t);
                if (fa * ft < 0) { b = t; fb = ft; found_bracket = true; break; }
                if (ft * fb < 0) { a = t; fa = ft; found_bracket = true; break; }
                a = t; fa = ft;
            }
        }
        x1_sol = found_bracket ? brent_fzero(f, a, b) : y1;
        x1_sol = std::max(0.0001, std::min(0.9999, x1_sol));

        double x2_sol = 1.0 - x1_sol;
        double P1 = P * y1 / x1_sol, P2 = P * y2 / x2_sol;
        double N1 = uptake(P1, p1.data()), N2 = uptake(P2, p2.data());
        double M1 = (N1 > 0 && N2 > 0) ? N1 * N2 / (N2 + N1 * (1.0 / x1_sol - 1.0)) : 0.0;
        double M2 = M1 * (1.0 / x1_sol - 1.0);
        double S = (x1_sol / x2_sol) * (y2 / y1);
        double Q = M1 * (y2 / y1) - M2;

        results.push_back({P, M1, M2, x1_sol, x2_sol, S, Q});
    }
    return results;
}

struct IASTTernary {
    double P, uptake1, uptake2, uptake3, x1, x2, x3, selectivity;
};

inline std::vector<IASTTernary> ternary_iast(const std::vector<double>& y,
    const std::vector<double>& pressures,
    const std::vector<double>& p1,
    const std::vector<double>& p2,
    const std::vector<double>& p3) {
    std::vector<IASTTernary> results;
    double y1 = y[0], y2 = y[1], y3 = y[2];
    auto x0 = std::vector<double>{0.34, 0.33, 0.33};
    double eps = 1e-6;

    for (double P : pressures) {
        auto F = [&](const std::vector<double>& x, std::vector<double>& out) {
            out[0] = x[0] + x[1] + x[2] - 1.0;
            out[1] = spreading_pressure(P * y1 / x[0], p1.data()) - spreading_pressure(P * y2 / x[1], p2.data());
            out[2] = spreading_pressure(P * y1 / x[0], p1.data()) - spreading_pressure(P * y3 / x[2], p3.data());
        };

        auto x = x0;
        for (int iter = 0; iter < 100; ++iter) {
            std::vector<double> f(3), f2(3), J(9);
            F(x, f);
            double max_f = 0.0;
            for (int i = 0; i < 3; ++i) max_f = std::max(max_f, std::abs(f[i]));
            if (max_f < 1e-10) break;

            for (int j = 0; j < 3; ++j) {
                double orig = x[j];
                double h = eps * std::abs(orig);
                if (h < 1e-8) h = 1e-6;
                x[j] = orig + h; F(x, f2);
                for (int i = 0; i < 3; ++i) J[i*3+j] = (f2[i] - f[i]) / h;
                x[j] = orig;
            }

            double det = J[0]*(J[4]*J[8]-J[5]*J[7]) - J[1]*(J[3]*J[8]-J[5]*J[6]) + J[2]*(J[3]*J[7]-J[4]*J[6]);
            if (std::abs(det) < 1e-15) break;
            double dx0 = (f[0]*(J[5]*J[7]-J[4]*J[8]) + f[1]*(J[1]*J[8]-J[2]*J[7]) + f[2]*(J[2]*J[4]-J[1]*J[5])) / det;
            double dx1 = (f[0]*(J[3]*J[8]-J[5]*J[6]) + f[1]*(J[2]*J[6]-J[0]*J[8]) + f[2]*(J[0]*J[5]-J[2]*J[3])) / det;
            double dx2 = (f[0]*(J[4]*J[6]-J[3]*J[7]) + f[1]*(J[0]*J[7]-J[1]*J[6]) + f[2]*(J[1]*J[3]-J[0]*J[4])) / det;

            x[0] -= dx0; x[1] -= dx1; x[2] -= dx2;
            x[0] = std::max(0.001, std::min(0.997, x[0]));
            x[1] = std::max(0.001, std::min(0.997, x[1]));
            x[2] = std::max(0.001, std::min(0.997, x[2]));
            double s = x[0] + x[1] + x[2];
            x[0] /= s; x[1] /= s; x[2] /= s;
        }
        x0 = x;

        double P1 = P * y1 / x[0], P2 = P * y2 / x[1], P3 = P * y3 / x[2];
        double N1 = uptake(P1, p1.data()), N2 = uptake(P2, p2.data()), N3 = uptake(P3, p3.data());
        double denom = N1*N2*x[2] + N1*N3*x[1] + N2*N3*x[0];
        double M1 = denom > 0 ? N1*N2*N3*x[0] / denom : 0.0;
        double M2 = M1 * (x[1] / x[0]);
        double M3 = M1 * (x[2] / x[0]);
        double S = (x[0] / x[1]) * (y2 / y1);
        results.push_back({P, M1, M2, M3, x[0], x[1], x[2], S});
    }
    return results;
}

// ─── CSV I/O ─────────────────────────────────────────────────

inline std::pair<std::vector<double>, std::vector<double>> read_isotherm_csv(const std::string& path) {
    std::ifstream f(std::filesystem::u8path(path));
    if (!f) { std::cerr << "ERROR: Cannot open " << path << "\n"; exit(1); }
    std::vector<double> press, uptake;
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        size_t pos = line.find(',');
        if (pos == std::string::npos) continue;
        press.push_back(std::stod(line.substr(0, pos)));
        uptake.push_back(std::stod(line.substr(pos + 1)));
    }
    return {press, uptake};
}

inline void write_csv(const std::string& path,
    const std::vector<std::string>& header,
    const std::vector<std::vector<double>>& rows) {
    std::ofstream f(std::filesystem::u8path(path));
    for (size_t i = 0; i < header.size(); ++i) { if (i > 0) f << ","; f << header[i]; }
    f << "\n";
    for (auto& r : rows) {
        for (size_t i = 0; i < r.size(); ++i) { if (i > 0) f << ","; f << std::fixed << std::setprecision(6) << r[i]; }
        f << "\n";
    }
}

inline std::filesystem::path& output_base_dir() {
    static std::filesystem::path dir;
    return dir;
}

inline void set_output_base_dir(const std::filesystem::path& dir) {
    output_base_dir() = dir;
}

inline std::string final_output_path(const std::string& name) {
    std::filesystem::path requested = std::filesystem::u8path(name);
    if (requested.has_parent_path()) {
        std::filesystem::create_directories(requested.parent_path());
        return requested.u8string();
    }
    std::filesystem::path dir = output_base_dir();
    if (!dir.empty()) std::filesystem::create_directories(dir);
    return dir.empty() ? requested.u8string() : (dir / requested).u8string();
}

inline std::string final_output_prefix(const std::string& prefix) {
    std::filesystem::path requested = std::filesystem::u8path(prefix);
    if (requested.has_parent_path()) {
        std::filesystem::create_directories(requested.parent_path());
        return requested.u8string();
    }
    std::filesystem::path dir = output_base_dir();
    if (!dir.empty()) std::filesystem::create_directories(dir);
    return dir.empty() ? requested.u8string() : (dir / requested).u8string();
}

// ─── Model Factory ──────────────────────────────────────────

inline ModelFn make_model_fn(const std::string& model_name) {
    return [model_name](double P, const std::vector<double>& p) -> double {
        double a1 = p[0], b1 = p[1], c1 = p[2];
        double a2 = p[3], b2 = p[4], c2 = p[5];
        if (model_name == "SSL")  { c1 = 1.0; a2 = 1.0; b2 = 1.0; c2 = 1.0; }
        else if (model_name == "DSL")  { c1 = 1.0; c2 = 1.0; }
        else if (model_name == "SSLF") { a2 = 1.0; b2 = 1.0; c2 = 1.0; }
        double arr[6] = {a1, b1, c1, a2, b2, c2};
        return uptake(P, arr);
    };
}

// ─── SVG Plot Generator ──────────────────────────────────────

inline void svg_scatter(const std::string& path,
    const std::vector<std::pair<std::string, std::pair<std::vector<double>, std::vector<double>>>>& series,
    const std::string& xlabel, const std::string& ylabel, const std::string& title,
    double w = 800, double h = 600) {
    if (series.empty()) return;
    double xmin = 1e30, xmax = -1e30, ymin = 1e30, ymax = -1e30;
    for (auto& s : series) {
        for (double v : s.second.first) { xmin = std::min(xmin, v); xmax = std::max(xmax, v); }
        for (double v : s.second.second) { ymin = std::min(ymin, v); ymax = std::max(ymax, v); }
    }
    double xpad = (xmax - xmin) * 0.05, ypad = (ymax - ymin) * 0.05;
    if (xpad < 1) xpad = 1; if (ypad < 1e-6) ypad = 0.02;
    xmin -= xpad; xmax += xpad; ymin -= ypad; ymax += ypad;
    auto tx = [&](double v){ return 60+(v-xmin)/(xmax-xmin)*(w-120); };
    auto ty = [&](double v){ return h-60-(v-ymin)/(ymax-ymin)*(h-120); };
    const char* fills[] = {"#2ecc71","#e74c3c","#3498db","#9b59b6","#f39c12","#1abc9c"};

    std::ofstream f(std::filesystem::u8path(path));
    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    f << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << w << " " << h << "\">\n";
    f << "  <rect width=\"" << w << "\" height=\"" << h << "\" fill=\"white\"/>\n";
    f << "  <text x=\"" << w/2 << "\" y=\"28\" text-anchor=\"middle\" font-size=\"16\" font-weight=\"bold\">" << title << "</text>\n";
    f << "  <line x1=\"60\" y1=\"" << ty(ymin) << "\" x2=\"60\" y2=\"" << ty(ymax) << "\" stroke=\"black\" stroke-width=\"1.5\"/>\n";
    f << "  <line x1=\"" << tx(xmin) << "\" y1=\"" << ty(ymin) << "\" x2=\"" << tx(xmax) << "\" y2=\"" << ty(ymin) << "\" stroke=\"black\" stroke-width=\"1.5\"/>\n";
    int nx = 5, ny = 5;
    for (int i = 0; i <= nx; ++i) {
        double v = xmin + (xmax - xmin) * i / nx;
        f << "  <line x1=\"" << tx(v) << "\" y1=\"" << ty(ymin) << "\" x2=\"" << tx(v) << "\" y2=\"" << ty(ymax) << "\" stroke=\"#e0e0e0\" stroke-width=\"0.5\"/>\n";
    }
    for (int i = 0; i <= ny; ++i) {
        double v = ymin + (ymax - ymin) * i / ny;
        f << "  <line x1=\"" << tx(xmin) << "\" y1=\"" << ty(v) << "\" x2=\"" << tx(xmax) << "\" y2=\"" << ty(v) << "\" stroke=\"#e0e0e0\" stroke-width=\"0.5\"/>\n";
    }
    f << "  <text x=\"" << w/2 << "\" y=\"" << h-10 << "\" text-anchor=\"middle\" font-size=\"14\">" << xlabel << "</text>\n";
    f << "  <text x=\"15\" y=\"" << h/2 << "\" text-anchor=\"middle\" font-size=\"14\" transform=\"rotate(-90 15," << h/2 << ")\">" << ylabel << "</text>\n";
    char buf[64];
    for (int i = 0; i <= nx; ++i) {
        double v = xmin + (xmax - xmin) * i / nx;
        sprintf(buf, "%.6g", v);
        f << "  <text x=\"" << tx(v) << "\" y=\"" << ty(ymin)+18 << "\" text-anchor=\"middle\" font-size=\"11\">" << buf << "</text>\n";
    }
    for (int i = 0; i <= ny; ++i) {
        double v = ymin + (ymax - ymin) * i / ny;
        sprintf(buf, "%.6g", v);
        f << "  <text x=\"52\" y=\"" << ty(v)+4 << "\" text-anchor=\"end\" font-size=\"11\">" << buf << "</text>\n";
    }
    for (size_t s = 0; s < series.size(); ++s) {
        auto& sx = series[s].second.first;
        auto& sy = series[s].second.second;
        for (size_t i = 0; i < sx.size(); ++i)
            f << "  <circle cx=\"" << tx(sx[i]) << "\" cy=\"" << ty(sy[i]) << "\" r=\"4\" fill=\"" << fills[s%6] << "\" opacity=\"0.8\"/>\n";
    }
    double ly = 45;
    for (size_t s = 0; s < series.size(); ++s) {
        f << "  <circle cx=\"" << w-120 << "\" cy=\"" << ly << "\" r=\"4\" fill=\"" << fills[s%6] << "\" opacity=\"0.8\"/>\n";
        f << "  <text x=\"" << w-110 << "\" y=\"" << ly+4 << "\" font-size=\"12\">" << series[s].first << "</text>\n";
        ly += 20;
    }
    f << "</svg>\n";
}

} // namespace iast
