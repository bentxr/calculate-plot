// Derived from Matplotlib 3.11.2: lib/matplotlib/mlab.py (GaussianKDE) and lib/matplotlib/cbook.py
// (violin_stats, delete_masked_points).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "engine/violin.hpp"

#include "support/histogram.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace calculate_plot::engine {

namespace {

// mlab.GaussianKDE of one-dimensional data, evaluated at `points`
std::vector<double> gaussianKde(const std::vector<double>& data, const Bandwidth& bw, const std::vector<double>& points) {
    const double n = double(data.size());
    // covariance_factor: scotts_factor np.power(n, -1/(d + 4)), silverman_factor np.power(n*(d + 2)/4, -1/(d + 4))
    double factor;
    if (bw.factor) factor = *bw.factor;
    else if (bw.rule == "silverman") factor = std::pow(n * (1 + 2.0) / 4.0, -1. / (1 + 4));
    else factor = std::pow(n, -1. / (1 + 4));
    const double dataCovariance = support::npCovRow(data);
    const double dataInvCov = 1.0 / dataCovariance;  // np.linalg.inv of [[c]]
    const double square = factor * factor;            // factor ** 2
    const double covariance = dataCovariance * square;
    const double invCov = dataInvCov / square;
    const double normFactor = std::sqrt(support::npDet1(2 * M_PI * covariance)) * n;
    std::vector<double> result(points.size(), 0.0);
    if (points.size() >= data.size()) {
        // over the data: result += exp(-energy) at every point
        for (double xi : data)
            for (std::size_t j = 0; j < points.size(); ++j) {
                const double diff = xi - points[j];
                const double tdiff = invCov * diff;  // np.dot(inv_cov, diff)
                const double energy = diff * tdiff / 2.0;
                result[j] = result[j] + std::exp(-energy);
            }
    } else {
        // over the points: the sum over the data (np.sum: pairwise)
        std::vector<double> terms(data.size());
        for (std::size_t j = 0; j < points.size(); ++j) {
            for (std::size_t i = 0; i < data.size(); ++i) {
                const double diff = data[i] - points[j];
                const double tdiff = invCov * diff;
                const double energy = diff * tdiff / 2.0;
                terms[i] = std::exp(-energy);
            }
            result[j] = support::npSum(terms);
        }
    }
    for (double& r : result) r = r / normFactor;
    return result;
}

}  // namespace

bool violinStats(std::vector<double> x, bool integer, const Bandwidth& bw, long long points, const std::vector<double>& quantiles,
                 ViolinStats* out, std::string* error) {
    // delete_masked_points: the finite values
    x.erase(std::remove_if(x.begin(), x.end(), [](double v) { return !std::isfinite(v); }), x.end());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (x.empty()) {
        *out = ViolinStats{{}, {}, {}, nan, nan, nan, nan};
        return true;
    }
    if (points < 0) return *error = "Number of samples, " + std::to_string(points) + ", must be non-negative.", false;
    out->min = support::npMin(x);
    out->max = support::npMax(x);
    out->coords = support::npLinspace(out->min, out->max, points);
    // _kde_method: all values equal give 1 where a coordinate is that value; otherwise the KDE
    if (std::all_of(x.begin(), x.end(), [&](double v) { return v == x[0]; })) {
        out->vals.clear();
        for (double c : out->coords) out->vals.push_back(c == x[0] ? 1.0 : 0.0);
    } else {
        out->vals = gaussianKde(x, bw, out->coords);
    }
    out->mean = support::npMean(x, integer);
    out->median = support::npMedian(x, integer);
    out->quantiles.clear();
    for (double q : quantiles) out->quantiles.push_back(support::npPercentile(x, 100 * q));  // np.percentile(x, 100 * quantile)
    return true;
}

}  // namespace calculate_plot::engine
