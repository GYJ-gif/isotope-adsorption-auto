#include "qst_core.h"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

int main() {
    using namespace qst;

    const std::vector<double> loadings{0.2, 0.5, 1.0, 1.5, 2.0};
    std::vector<double> params{-1000.0, -50.0, 2.0};

    DataSet ds273;
    ds273.T = 273.0;
    DataSet ds298;
    ds298.T = 298.0;

    for (double n : loadings) {
        ds273.N.push_back(n);
        ds273.lnP.push_back(virial_lnP(n, ds273.T, params.data(), 1, 0));
        ds298.N.push_back(n);
        ds298.lnP.push_back(virial_lnP(n, ds298.T, params.data(), 1, 0));
    }

    auto range = overlapping_loading_range({ds273, ds298});
    assert(range.valid);
    assert(std::abs(range.from - 0.2) < 1e-12);
    assert(std::abs(range.to - 2.0) < 1e-12);

    auto fit = auto_virial_fit({ds273, ds298}, 3, 2);
    assert(fit.selected_m <= 3);
    assert(fit.selected_n <= 2);
    assert(fit.result.r_squared > 0.999);
    assert(!fit.candidates.empty());

    std::string output = final_output_path("Qst_result_Qst.csv");
    assert(output == "Qst_final/Qst_result_Qst.csv");
    set_output_base_dir("QstExeForTest");
    output = final_output_path("Qst_result_Qst.csv");
    assert(output == "QstExeForTest/Qst_final/Qst_result_Qst.csv");

    return 0;
}
