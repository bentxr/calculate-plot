// Derived from Matplotlib 3.11.2: lib/matplotlib/image.py (_ImageBase, AxesImage, _resample, composite_images,
// _draw_list_compositing_images), lib/matplotlib/colorizer.py (to_rgba of images) and lib/matplotlib/cbook.py
// (safe_masked_invalid).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
//
// The arithmetic follows NumPy's dtypes step by step: float32 arrays round each result to float32, a Python
// number takes the array's dtype, and arrays of different float dtypes meet in float64.

#include "engine/image.hpp"

#include "engine/axes.hpp"
#include "engine/figure.hpp"
#include "render/resample.hpp"
#include "rc/rc_params.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace calculate_plot::engine {

namespace {

using colors::DType;
using transforms::Affine2D;
using transforms::TransformPtr;

double f32(double v) { return double(float(v)); }
double store(DType t, double v) { return t == DType::Float32 ? f32(v) : v; }
using colors::isFloat;
bool isInteger(DType t) { return !isFloat(t) && t != DType::Bool; }

// a float cast to uint8 as NumPy does it on x86 (through a 32-bit integer, keeping the low byte)
double toUint8(double v) {
    if (!(std::fabs(v) < 2147483648.0)) return 0;
    return double(std::uint8_t(std::int32_t(v)));
}

TransformPtr affine(const Affine2D& a) { return std::make_shared<Affine2D>(a); }

// matplotlib.image._interpd_: the resampler's filter of an interpolation name
render::Interpolation filterOf(const std::string& name) {
    if (name == "auto" || name == "none" || name == "nearest" || name == "antialiased") return render::Interpolation::Nearest;
    std::string upper = name;
    for (char& c : upper) c = char(std::toupper((unsigned char)c));
    return render::interpolationNamed(upper).value_or(render::Interpolation::Nearest);
}

// _resample(image_obj, data, out_shape, transform, resample=, alpha=): `data` grey (2-D) or RGBA, of uint8,
// float32 or float64; the output starts as zeros
bool resampleImage(const AxesImage& im, const ImageArray& in, int outRows, int outCols, TransformPtr t,
                   std::optional<bool> resampleArg, double alpha, ImageArray* out, std::string* error) {
    if (in.cols > (1 << 23) || in.rows > (1 << 24))
        return *error = "images of more than 2**23 columns or 2**24 rows are not supported yet", false;
    std::string interpolation = im.interpolation;
    if (interpolation == "antialiased" || interpolation == "auto") {
        // nearest when upsampling by an integer factor of 1 or 2, or by more than 3 in both directions
        const auto p0 = t->transform(0, 0), p1 = t->transform(in.cols, in.rows);
        const double dispx = std::fabs(p1[0] - p0[0]), dispy = std::fabs(p1[1] - p0[1]);
        const double c = in.cols, r = in.rows;
        if ((dispx > 3 * c || dispx == c || dispx == 2 * c) && (dispy > 3 * r || dispy == r || dispy == 2 * r))
            interpolation = "nearest";
        else
            interpolation = "hanning";
    }
    const int channels = in.channels ? in.channels : 1;
    ImageArray data = in;
    if (interpolation == "nearest") {
        // an output pixel on the edge between two input pixels takes the left one: flip the data and the transform
        Affine2D flipX;
        flipX.translate(-in.cols, 0).scale(-1, 1);
        t = transforms::compose(affine(flipX), t);
        for (int r = 0; r < in.rows; ++r)
            for (int c = 0; c < in.cols; ++c)
                for (int k = 0; k < channels; ++k)
                    data.data[(std::size_t(r) * in.cols + c) * channels + k] =
                        in.data[(std::size_t(r) * in.cols + (in.cols - 1 - c)) * channels + k];
        if (im.origin == "upper") {
            Affine2D flipY;
            flipY.translate(0, -in.rows).scale(1, -1);
            t = transforms::compose(affine(flipY), t);
            const ImageArray flipped = data;
            for (int r = 0; r < in.rows; ++r)
                for (int c = 0; c < in.cols * channels; ++c)
                    data.data[std::size_t(r) * in.cols * channels + c] =
                        flipped.data[std::size_t(in.rows - 1 - r) * in.cols * channels + c];
        }
    }
    const char* dtypeName = colors::dtypeName(in.dtype);
    const auto type = render::pixelTypeOf(dtypeName, in.channels == 4);
    if (!type || (in.channels != 0 && in.channels != 4)) return *error = "arrays must be of dtype byte, short, float32 or float64", false;
    render::ResampleParams p;
    p.interpolation = filterOf(interpolation);
    p.resample = resampleArg.value_or(im.resample);
    p.alpha = alpha;
    p.norm = im.filternorm;
    p.radius = im.filterrad;
    std::vector<double> mesh;
    if (t->isAffine()) {
        p.affine = t->matrix();
    } else {
        // _get_transform_mesh: each output pixel centre through the inverse transform
        const TransformPtr inverse = t->inverted();
        for (int y = 0; y < outRows; ++y)
            for (int x = 0; x < outCols; ++x) {
                const auto q = inverse->transform(x + 0.5, y + 0.5);
                mesh.push_back(q[0]);
                mesh.push_back(q[1]);
            }
        p.mesh = &mesh;
    }
    out->rows = outRows;
    out->cols = outCols;
    out->channels = in.channels;
    out->dtype = in.dtype;
    out->mask.clear();
    const std::size_t n = std::size_t(outRows) * outCols * channels;
    out->data.assign(n, 0.0);
    // the buffers in the arrays' own types
    const auto run = [&](auto zero) {
        using T = decltype(zero);
        std::vector<T> src(data.data.size()), dst(n, T(0));
        for (std::size_t i = 0; i < src.size(); ++i) src[i] = T(data.data[i]);
        if (!render::resample(*type, src.data(), in.cols, in.rows, dst.data(), outCols, outRows, p, error)) return false;
        for (std::size_t i = 0; i < n; ++i) out->data[i] = double(dst[i]);
        return true;
    };
    switch (in.dtype) {
    case DType::UInt8: return run(std::uint8_t(0));
    case DType::UInt16: return run(std::uint16_t(0));
    case DType::Float32: return run(float(0));
    case DType::Float64: return run(double(0));
    default: return *error = "arrays must be of dtype byte, short, float32 or float64", false;
    }
}

// Colorizer._pass_image_data(x, alpha=None, bytes=, norm=) of an RGB or RGBA array
bool passImageData(const ImageArray& x, bool bytes, bool norm, ImageArray* out, std::string* error) {
    const std::size_t pixels = std::size_t(x.rows) * x.cols;
    ImageArray xx;
    xx.rows = x.rows;
    xx.cols = x.cols;
    xx.channels = 4;
    xx.dtype = x.dtype;
    xx.data.assign(pixels * 4, 0.0);
    for (std::size_t i = 0; i < pixels; ++i) {
        for (int k = 0; k < x.channels; ++k) xx.data[i * 4 + k] = x.data[i * x.channels + k];
        if (x.channels == 3) xx.data[i * 4 + 3] = x.dtype == DType::UInt8 ? 255 : 1;  // alpha 1, np.uint8(255)
    }
    if (isFloat(xx.dtype)) {
        for (std::size_t i = 0; i < pixels; ++i) {
            bool nan = false;
            for (int k = 0; k < x.channels; ++k) nan = nan || std::isnan(x.data[i * x.channels + k]);
            if (nan)
                for (int k = 0; k < 4; ++k) xx.data[i * 4 + k] = 0;
        }
        if (norm) {
            const auto [mn, mx] = std::minmax_element(xx.data.begin(), xx.data.end());
            if (*mx > 1 || *mn < 0) return *error = "Floating point image RGB values must be in the [0,1] range", false;
        }
        if (bytes) {
            for (double& v : xx.data) v = toUint8(store(xx.dtype, v * 255));
            xx.dtype = DType::UInt8;
        }
    } else if (xx.dtype == DType::UInt8) {
        if (!bytes) {
            for (double& v : xx.data) v = f32(f32(v) / 255);
            xx.dtype = DType::Float32;
        }
    } else {
        return *error = std::string("Image RGB array must be uint8 or floating point; found ") + colors::dtypeName(xx.dtype), false;
    }
    if (x.anyMasked())  // a masked entry anywhere in a pixel makes it transparent
        for (std::size_t i = 0; i < pixels; ++i)
            for (int k = 0; k < x.channels; ++k)
                if (x.masked(i * x.channels + k)) xx.data[i * 4 + 3] = 0;
    *out = xx;
    return true;
}

// Colorizer.to_rgba(x, bytes=, norm=) of a 2-D array (through the norm unless `norm` is false, then the colormap)
bool mapGrey(const AxesImage& im, colors::Norm& norm, const ImageArray& a, bool bytes, bool useNorm, ImageArray* out,
             std::string* error) {
    colors::MaskedArray x;
    x.data = a.data;
    x.mask = a.mask;
    x.dtype = a.dtype;
    colors::MaskedArray normed = x;
    if (useNorm && !norm(x, &normed, error)) return false;
    std::vector<colors::Rgba> rgba;
    if (!im.cmap(normed, nullptr, bytes, &rgba, error)) return false;
    out->rows = a.rows;
    out->cols = a.cols;
    out->channels = 4;
    out->dtype = bytes ? DType::UInt8 : DType::Float64;
    out->mask.clear();
    out->data.clear();
    for (const auto& c : rgba) out->data.insert(out->data.end(), c.begin(), c.end());
    return true;
}

}  // namespace

bool ImageArray::anyMasked() const { return std::any_of(mask.begin(), mask.end(), [](bool b) { return b; }); }

bool readArray(const support::Json& v, ImageArray* out, std::string* error) {
    std::optional<DType> dtype;
    const support::Json* data = &v;
    if (v.isObject() && v.members().size() == 1 && v.members()[0].first == "$array") {
        dtype = colors::dtypeNamed(v.members()[0].second.find("dtype")->asString());
        data = v.members()[0].second.find("data");
    }
    // the shape from the first elements, then every list checked against it
    std::vector<int> shape;
    for (const support::Json* d = data; d->isArray(); d = &(*d)[0]) {
        shape.push_back(int(d->size()));
        if (d->size() == 0) break;
    }
    if (shape.empty() || shape.size() > 3) return *error = "the image data must be an array of 2 or 3 dimensions", false;
    std::vector<double> values;
    bool allBool = true, allInt = true;
    std::string why;
    const std::function<bool(const support::Json&, std::size_t)> walk = [&](const support::Json& e, std::size_t depth) {
        if (depth < shape.size()) {
            if (!e.isArray() || int(e.size()) != shape[depth])
                return why = "setting an array element with a sequence: the lists have an inhomogeneous shape", false;
            for (const support::Json& item : e.items())
                if (!walk(item, depth + 1)) return false;
            return true;
        }
        double x = 0;
        if (e.isBool()) {
            x = e.asBool() ? 1 : 0;
        } else if (e.isNumber()) {
            x = e.asNumber();
            allBool = false;
            if (!e.isInteger()) allInt = false;
        } else if (e.isObject() && e.members().size() == 1 && e.members()[0].first == "$float") {
            const std::string s = e.members()[0].second.asString();
            x = s == "nan" ? std::numeric_limits<double>::quiet_NaN() : s == "inf" ? HUGE_VAL : -HUGE_VAL;
            allBool = allInt = false;
        } else {
            return why = "the image data must be numbers", false;
        }
        values.push_back(x);
        return true;
    };
    if (!walk(*data, 0)) return *error = why, false;
    ImageArray a;
    a.rows = shape[0];
    a.cols = shape.size() > 1 ? shape[1] : 1;
    a.channels = shape.size() > 2 ? shape[2] : 0;
    if (shape.size() == 1) return *error = "the image data must be an array of 2 or 3 dimensions", false;
    a.dtype = dtype ? *dtype : allBool ? DType::Bool : allInt ? DType::Int64 : DType::Float64;
    for (double& x : values) {
        if (a.dtype == DType::Float32) {
            x = f32(x);
        } else if (a.dtype != DType::Float64 && a.dtype != DType::Bool) {
            if (!std::isfinite(x)) return *error = "cannot convert float NaN or infinity to integer", false;
            x = std::trunc(x);
            const double lo = a.dtype == DType::UInt8 || a.dtype == DType::UInt16 ? 0 : a.dtype == DType::Int16 ? -32768
                              : a.dtype == DType::Int32 ? -2147483648.0 : -9223372036854775808.0;
            const double hi = a.dtype == DType::UInt8 ? 255 : a.dtype == DType::UInt16 ? 65535 : a.dtype == DType::Int16 ? 32767
                              : a.dtype == DType::Int32 ? 2147483647.0 : 9223372036854775807.0;
            if (x < lo || x > hi) return *error = std::string("a value is out of bounds for ") + colors::dtypeName(a.dtype), false;
        } else if (a.dtype == DType::Bool) {
            x = x != 0;
        }
    }
    a.data = std::move(values);
    *out = a;
    return true;
}

AxesImage::AxesImage(const rc::Params& rc, Axes& axes) : Artist(rc), axes_(axes) {
    zorder = 0;
    origin = rc.text("image.origin");
    interpolation = rc.text("image.interpolation");
    interpolationStage = rc.text("image.interpolation_stage");
    resample = rc.boolean("image.resample");
}

bool AxesImage::setData(const ImageArray& in, std::string* error) {
    ImageArray a = in;
    // cbook.safe_masked_invalid: masked where not finite (no mask when all are)
    a.mask.assign(a.size(), false);
    for (std::size_t i = 0; i < a.size(); ++i) a.mask[i] = !std::isfinite(a.data[i]);
    if (!a.anyMasked()) a.mask.clear();
    if (a.channels == 1) a.channels = 0;  // (M, N, 1): scalar data
    if (!(a.channels == 0 || a.channels == 3 || a.channels == 4))
        return *error = "Invalid shape (" + std::to_string(a.rows) + ", " + std::to_string(a.cols) + ", " +
                        std::to_string(a.channels) + ") for image data",
               false;
    if (a.channels) {
        // RGB(A): clipped to [0, 1] for floats or [0, 255] for integers (masked values do not count), integers
        // other than uint8 cast to uint8
        const double high = isInteger(a.dtype) || a.dtype == DType::Bool ? 255 : 1;
        std::optional<double> mn, mx;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a.masked(i)) continue;
            if (!mn || a.data[i] < *mn) mn = a.data[i];
            if (!mx || a.data[i] > *mx) mx = a.data[i];
        }
        if (mn && (*mn < 0 || high < *mx))
            for (double& v : a.data) v = std::isnan(v) ? v : std::clamp(v, 0.0, high);
        if (a.dtype != DType::UInt8 && isInteger(a.dtype)) a.dtype = DType::UInt8;
        if (a.dtype == DType::Bool) return *error = "Image RGB array must be uint8 or floating point; found bool", false;
    }
    a_ = a;
    return true;
}

std::array<double, 4> AxesImage::getExtent() const {
    if (extent) return *extent;
    const double rows = a_.rows, cols = a_.cols;
    if (origin == "upper") return {-0.5, cols - 0.5, rows - 0.5, -0.5};
    return {-0.5, cols - 0.5, -0.5, rows - 0.5};
}

double AxesImage::scalarAlpha() const { return alphaArray || !alpha ? 1.0 : *alpha; }

bool AxesImage::canComposite() const {
    // can_composite: not 'none', an affine and separable transform (rectilinear Axes)
    return interpolation != "none" && transform->isAffine();
}

std::optional<AxesImage::Made> AxesImage::makeImage(Renderer& renderer, double magnification, bool unsampled) {
    std::string error;
    const auto fail = [&]() -> std::optional<Made> {
        renderer.unsupported(error);
        return std::nullopt;
    };
    // AxesImage.make_image: the extent through the transform, clipped to the clip box (or the Axes, or the figure)
    const auto e = getExtent();
    const auto inBbox = std::make_shared<transforms::Bbox>(std::array<double, 4>{e[0], e[2], e[1], e[3]});
    const auto outBbox = transforms::TransformedBbox(inBbox, transform).points();
    const auto clip = (clipOn ? (clipBox ? clipBox : transforms::BboxPtr(axes_.bbox)) : transforms::BboxPtr(axes_.figure.bbox))->points();
    // Bbox.intersection
    const double x0 = std::max(std::min(outBbox[0], outBbox[2]), std::min(clip[0], clip[2]));
    const double x1 = std::min(std::max(outBbox[0], outBbox[2]), std::max(clip[0], clip[2]));
    const double y0 = std::max(std::min(outBbox[1], outBbox[3]), std::min(clip[1], clip[3]));
    const double y1 = std::min(std::max(outBbox[1], outBbox[3]), std::max(clip[1], clip[3]));
    if (!(x0 <= x1 && y0 <= y1)) return std::nullopt;
    std::array<double, 4> m{x0 * magnification, y0 * magnification, x1 * magnification, y1 * magnification};
    if (!unsampled) {  // to the nearest output pixel, with a fudge away from Agg's anchor (x0, y1)
        m = {std::floor(m[0] + 0.5), std::ceil(m[1] - 0.5 - 1e-8), std::floor(m[2] + 0.5 + 1e-8), std::ceil(m[3] - 0.5)};
    }
    const double mwidth = m[2] - m[0], mheight = m[3] - m[1];
    if (mwidth == 0 || mheight == 0) return std::nullopt;
    TransformPtr t0 = std::make_shared<transforms::IdentityTransform>();
    if (origin == "upper") {
        Affine2D flip;
        flip.translate(0, -a_.rows).scale(1, -1);
        t0 = affine(flip);
    }
    Affine2D place;
    place.scale((e[1] - e[0]) / a_.cols, (e[3] - e[2]) / a_.rows).translate(e[0], e[2]);
    t0 = transforms::compose(t0, transforms::compose(affine(place), transform));
    Affine2D toOutput;
    toOutput.scale(magnification, magnification).translate(-m[0], -m[1]);
    TransformPtr t = transforms::compose(t0, affine(toOutput));
    const int outRows = int(mheight), outCols = int(mwidth);
    Made made;
    made.x = m[0] / magnification;
    made.y = m[1] / magnification;
    ImageArray output;
    if (!unsampled) {
        std::string stage = interpolationStage;
        if (stage == "antialiased" || stage == "auto") {
            const auto p0 = t->transform(0, 0), p1 = t->transform(a_.cols, a_.rows);
            const double dispx = std::fabs(p1[0] - p0[0]) / a_.cols, dispy = std::fabs(p1[1] - p0[1]) / a_.rows;
            stage = dispx < 3 || dispy < 3 ? "rgba" : "data";
        }
        ImageArray A = a_;
        ImageArray outAlpha;
        if (A.channels == 0 && stage == "data") {
            // the data resampled in float32 or float64, then normalised and mapped
            DType scaled;
            if (isFloat(A.dtype)) {
                scaled = A.dtype;
            } else {
                double mn = 0, mx = 0;
                bool first = true;
                for (std::size_t i = 0; i < A.size(); ++i)
                    if (!A.masked(i)) {
                        if (first || A.data[i] < mn) mn = A.data[i];
                        if (first || A.data[i] > mx) mx = A.data[i];
                        first = false;
                    }
                scaled = mx - mn > 1e8 ? DType::Float64 : DType::Float32;
            }
            ImageArray cast = A;
            cast.dtype = scaled;
            for (double& v : cast.data) v = store(scaled, v);
            cast.mask.clear();
            ImageArray resampled;
            if (!resampleImage(*this, cast, outRows, outCols, t, std::nullopt, 1.0, &resampled, &error)) return fail();
            if (norm->spec().kind == "NoNorm") {  // cast back to the data's own dtype
                resampled.dtype = A.dtype;
                for (double& v : resampled.data)
                    v = isFloat(A.dtype) ? store(A.dtype, v) : A.dtype == DType::Bool ? double(v != 0) : std::trunc(v);
            }
            // out_alpha: how much of each screen pixel the data cover (NaN: bad data)
            ImageArray mask;
            mask.rows = A.rows;
            mask.cols = A.cols;
            mask.dtype = DType::Float32;
            mask.data.assign(A.size(), 1.0);
            for (std::size_t i = 0; i < A.size(); ++i)
                if (A.masked(i)) mask.data[i] = std::numeric_limits<double>::quiet_NaN();
            if (!resampleImage(*this, mask, outRows, outCols, t, true, 1.0, &outAlpha, &error)) return fail();
            std::vector<bool> outMask(outAlpha.size());
            for (std::size_t i = 0; i < outAlpha.size(); ++i) {
                outMask[i] = std::isnan(outAlpha.data[i]);
                if (outMask[i]) outAlpha.data[i] = 1;
            }
            if (alphaArray) {
                ImageArray ra;
                if (!resampleImage(*this, *alphaArray, outRows, outCols, t, true, 1.0, &ra, &error)) return fail();
                for (std::size_t i = 0; i < outAlpha.size(); ++i) outAlpha.data[i] = f32(outAlpha.data[i] * ra.data[i]);
            }
            resampled.mask = outMask;
            colors::MaskedArray res;
            res.data = resampled.data;
            res.mask = outMask;
            res.dtype = resampled.dtype;
            colors::MaskedArray normed;
            if (!(*norm)(res, &normed, &error)) return fail();
            std::vector<colors::Rgba> rgba;
            if (!cmap(normed, nullptr, true, &rgba, &error)) return fail();
            output.rows = outRows;
            output.cols = outCols;
            output.channels = 4;
            output.dtype = DType::UInt8;
            for (const auto& c : rgba) output.data.insert(output.data.end(), c.begin(), c.end());
            // the alpha channel times out_alpha and the scalar alpha, in float32, cast back to uint8
            const double alphaScalar = f32(scalarAlpha());
            for (std::size_t i = 0; i < outAlpha.size(); ++i) {
                double& a = output.data[i * 4 + 3];
                a = toUint8(f32(f32(f32(a) * outAlpha.data[i]) * alphaScalar));
            }
        } else {
            if (A.channels == 0) {  // stage 'rgba': mapped first
                colors::MaskedArray raw;
                raw.data = A.data;
                raw.mask = A.mask;
                raw.dtype = A.dtype;
                norm->autoscaleNone(colors::processValue(raw));
                ImageArray mapped;
                if (!mapGrey(*this, *norm, A, false, true, &mapped, &error)) return fail();
                A = mapped;
            }
            if (A.dtype == DType::UInt8) {  // np.divide(A, 0xff, dtype=np.float32)
                A.dtype = DType::Float32;
                for (double& v : A.data) v = f32(f32(v) / 255);
            }
            const std::size_t pixels = std::size_t(A.rows) * A.cols;
            const auto withAlpha = [&](DType dtype, const std::function<double(std::size_t)>& alphaOf, bool keepColour) {
                ImageArray r;
                r.rows = A.rows;
                r.cols = A.cols;
                r.channels = 4;
                r.dtype = dtype;
                r.data.resize(pixels * 4);
                for (std::size_t i = 0; i < pixels; ++i) {
                    for (int k = 0; k < 3; ++k) r.data[i * 4 + k] = A.data[i * A.channels + k];
                    r.data[i * 4 + 3] = keepColour ? alphaOf(i) : alphaOf(i);
                }
                return r;
            };
            bool postApplyAlpha = false;
            if (!alpha && !alphaArray) {
                // no alpha given: RGB gets an alpha of ones (float64: np.dstack with np.ones)
                if (A.channels == 3) A = withAlpha(DType::Float64, [](std::size_t) { return 1.0; }, true);
            } else if (alphaArray) {
                const ImageArray& al = *alphaArray;
                if (A.channels == 3) {
                    A = withAlpha(DType::Float64, [&](std::size_t i) { return al.data[i]; }, true);
                } else {
                    const ImageArray src = A;
                    A = withAlpha(DType::Float64, [&](std::size_t i) { return src.data[i * 4 + 3] * al.data[i]; }, true);
                }
            } else {
                if (A.channels == 3) {
                    const double a32 = f32(*alpha);  // np.full(..., alpha, np.float32)
                    A = withAlpha(A.dtype, [&](std::size_t) { return a32; }, true);
                } else {
                    postApplyAlpha = true;
                }
            }
            // resampled with premultiplied alpha
            bool premultiply = false;
            for (std::size_t i = 0; i < pixels; ++i) premultiply = premultiply || A.data[i * 4 + 3] != 1;
            if (premultiply)
                for (std::size_t i = 0; i < pixels; ++i)
                    for (int k = 0; k < 3; ++k) A.data[i * 4 + k] = store(A.dtype, A.data[i * 4 + k] * A.data[i * 4 + 3]);
            A.mask.clear();
            ImageArray res;
            if (!resampleImage(*this, A, outRows, outCols, t, std::nullopt, 1.0, &res, &error)) return fail();
            const std::size_t outPixels = std::size_t(outRows) * outCols;
            for (std::size_t i = 0; i < outPixels; ++i) {
                const double a = res.data[i * 4 + 3];
                const double divisor = a != 0 ? a : 1;
                for (int k = 0; k < 4; ++k) res.data[i * 4 + k] = store(res.dtype, res.data[i * 4 + k] / divisor);
                res.data[i * 4 + 3] = a;
                if (postApplyAlpha) res.data[i * 4 + 3] = store(res.dtype, a * store(res.dtype, *alpha));
            }
            if (!passImageData(res, true, false, &output, &error)) return fail();
        }
    } else {
        // unsampled: the whole image mapped once, cut to the part the clip box shows
        ImageArray full;
        if (a_.channels == 0) {
            if (!mapGrey(*this, *norm, a_, true, true, &full, &error)) return fail();
        } else if (!passImageData(a_, true, true, &full, &error)) {
            return fail();
        }
        const TransformPtr inverse = t0->inverted();
        const auto clipBoxPtr = std::make_shared<transforms::Bbox>(std::array<double, 4>{clip[0], clip[1], clip[2], clip[3]});
        const auto s = transforms::TransformedBbox(clipBoxPtr, inverse).points();
        const double sxmin = std::min(s[0], s[2]), sxmax = std::max(s[0], s[2]);
        const double symin = std::min(s[1], s[3]), symax = std::max(s[1], s[3]);
        const int r0 = int(std::max(symin, 0.0)), r1 = int(std::min(symax + 1, double(full.rows)));
        const int c0 = int(std::max(sxmin, 0.0)), c1 = int(std::min(sxmax + 1, double(full.cols)));
        output.rows = std::max(0, r1 - r0);
        output.cols = std::max(0, c1 - c0);
        output.channels = 4;
        output.dtype = DType::UInt8;
        for (int r = r0; r < r1; ++r)
            for (int c = c0; c < c1; ++c)
                for (int k = 0; k < 4; ++k) output.data.push_back(full.data[(std::size_t(r) * full.cols + c) * 4 + k]);
        Affine2D shift;
        shift.translate(c0, r0);
        t = transforms::compose(affine(shift), t);
    }
    made.image.width = output.cols;
    made.image.height = output.rows;
    for (double v : output.data) made.image.rgba.push_back((unsigned char)v);
    made.transform = t;
    return made;
}

void AxesImage::draw(Renderer& renderer) {
    if (!visible || a_.size() == 0) return;
    GC gc = renderer.newGc();
    setGcClip(gc);
    gc.setAlpha(scalarAlpha());
    gc.url = url;
    gc.gid = gid;
    if (renderer.optionScaleImage() && interpolation == "none" && transform->isAffine()) {
        const auto made = makeImage(renderer, 1.0, true);
        if (made && made->image.width > 0 && made->image.height > 0) {
            Affine2D size;
            size.scale(made->image.width, made->image.height);
            renderer.drawImage(gc, made->x, made->y, made->image, transforms::compose(affine(size), made->transform)->matrix());
        }
    } else {
        const auto made = makeImage(renderer, renderer.imageMagnification(), false);
        if (made) renderer.drawImage(gc, made->x, made->y, made->image, std::nullopt);
    }
}

void drawListCompositingImages(Renderer& renderer, const transforms::BboxPtr& parentBbox, const std::vector<Artist*>& artists) {
    const bool hasImages = std::any_of(artists.begin(), artists.end(), [](Artist* a) { return dynamic_cast<AxesImage*>(a); });
    if (renderer.optionImageNocomposite() || !hasImages) {
        for (Artist* a : artists) a->draw(renderer);
        return;
    }
    std::vector<AxesImage*> group;
    const double mag = renderer.imageMagnification();
    const auto flush = [&]() {
        if (group.size() == 1) {
            group[0]->draw(renderer);
        } else if (group.size() > 1) {
            // composite_images: each image made at the magnification, put on one canvas by nearest resampling
            struct Part {
                AxesImage::Made made;
                double alpha;
            };
            std::vector<Part> parts;
            std::vector<transforms::Bbox> boxes;
            for (AxesImage* im : group) {
                auto made = im->makeImage(renderer, mag, false);
                if (!made) continue;
                made->x *= mag;
                made->y *= mag;
                boxes.push_back(transforms::Bbox({made->x, made->y, made->x + made->image.width, made->y + made->image.height}));
                parts.push_back({*made, im->scalarAlpha()});
            }
            if (!parts.empty()) {
                const transforms::Bbox box = transforms::unionOf(boxes);
                const auto b = box.points();
                const int h = int(b[3] - b[1]), w = int(b[2] - b[0]);
                std::vector<unsigned char> output(std::size_t(h) * w * 4, 0);
                for (const Part& p : parts) {
                    render::ResampleParams rp;
                    rp.interpolation = render::Interpolation::Nearest;
                    rp.resample = false;
                    rp.alpha = p.alpha;
                    rp.affine = transforms::Matrix{1, 0, p.made.x - b[0], 0, 1, p.made.y - b[1]};
                    std::string error;
                    if (!render::resample(render::PixelType::Rgba8, p.made.image.rgba.data(), p.made.image.width,
                                          p.made.image.height, output.data(), w, h, rp, &error))
                        renderer.unsupported(error);
                }
                if (h > 0 && w > 0) {
                    GC gc = renderer.newGc();
                    const auto pb = parentBbox->points();
                    gc.cliprect = std::array<double, 4>{pb[0], pb[1], pb[2], pb[3]};
                    RgbaImage im;
                    im.width = w;
                    im.height = h;
                    im.rgba = output;
                    // round(l), round(b): Python rounds halves to even
                    renderer.drawImage(gc, std::nearbyint(b[0] / mag), std::nearbyint(b[1] / mag), im, std::nullopt);
                }
            }
        }
        group.clear();
    };
    for (Artist* a : artists) {
        auto* im = dynamic_cast<AxesImage*>(a);
        if (im && im->canComposite() && im->clipOn && !im->clipPath) {
            group.push_back(im);
        } else {
            flush();
            a->draw(renderer);
        }
    }
    flush();
}

}  // namespace calculate_plot::engine
