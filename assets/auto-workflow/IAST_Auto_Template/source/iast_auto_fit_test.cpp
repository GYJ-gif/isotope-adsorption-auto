#include "iast_core.h"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

int main() {
    using namespace iast;

    const double ssl_site[6] = {5.0, 0.2, 1.0, 0.0, 1.0, 1.0};
    const double p = 1.0;
    const double expected_uptake = (5.0 * 0.2 * p) / (1.0 + 0.2 * p);
    const double expected_pi = (5.0 / 1.0) * std::log(1.0 + 0.2 * p);
    assert(std::abs(uptake(p, ssl_site) - expected_uptake) < 1e-12);
    assert(std::abs(spreading_pressure(p, ssl_site) - expected_pi) < 1e-12);

    std::vector<double> identical(ssl_site, ssl_site + 6);
    auto rows = binary_iast(0.5, {10.0}, identical, identical);
    assert(rows.size() == 1);
    assert(std::abs(rows[0].x1 - 0.5) < 1e-6);
    assert(std::abs(rows[0].selectivity - 1.0) < 1e-6);
    assert(std::abs(rows[0].sep_potential) < 1e-6);

    std::vector<double> pressure{0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0};
    std::vector<double> uptake_values;
    std::vector<double> ssl_params{5.0, 0.2, 1.0, 1.0, 1.0, 1.0};
    auto ssl = make_model_fn("SSL");
    for (double p : pressure) uptake_values.push_back(ssl(p, ssl_params));

    auto auto_result = fit_model_auto(pressure, uptake_values, "Auto",
                                      {1.0, 0.1, 1.0, 1.0, 0.1, 1.0});

    assert(auto_result.selected_model == "SSL");
    assert(auto_result.result.r_squared > 0.999);
    assert(auto_result.result.params[3] == 0.0);
    assert(auto_result.candidates.size() == 4);

    std::string output = final_output_path("IAST_result_Selectivity.csv");
    assert(output == "IAST_result_Selectivity.csv");
    set_output_base_dir("IastExeForTest");
    output = final_output_path("IAST_result_Selectivity.csv");
    assert(std::filesystem::u8path(output) ==
           std::filesystem::path("IastExeForTest") / "IAST_result_Selectivity.csv");

    return 0;
}
