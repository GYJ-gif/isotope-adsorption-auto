#include "iast_core.h"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

int main() {
    using namespace iast;

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
