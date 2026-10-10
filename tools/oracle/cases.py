"""The reference figures of the renderer corpus (tests/corpus/<name>). Each function builds one
figure with matplotlib's default style; make_refs.py records it. Keep figures small and varied:
together they must exercise every drawing primitive and graphics-context field."""
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyBboxPatch, Polygon

CASES = {}


def case(fn):
    CASES[fn.__name__] = fn
    return fn


def figure(w=3.2, h=2.4):
    return plt.figure(figsize=(w, h), dpi=100)


@case
def line_basic():
    fig = figure()
    ax = fig.add_axes((0.15, 0.15, 0.8, 0.8))
    ax.plot([0, 1, 2, 3], [0, 1, 4, 9])
    return fig


@case
def line_styles():
    fig = figure()
    ax = fig.add_axes((0.1, 0.1, 0.85, 0.85))
    x = np.linspace(0, 10, 50)
    for i, (ls, lw) in enumerate([("-", 0.5), ("--", 1.5), (":", 2.0), ("-.", 3.0), ((0, (5, 2, 1, 2)), 1.0)]):
        ax.plot(x, np.sin(x + i) + 2.5 * i, linestyle=ls, linewidth=lw)
    return fig


@case
def caps_and_joins():
    fig = figure()
    ax = fig.add_axes((0.05, 0.05, 0.9, 0.9))
    ax.set_axis_off()
    for i, cap in enumerate(["butt", "round", "projecting"]):
        for j, join in enumerate(["miter", "round", "bevel"]):
            ax.plot([i * 3, i * 3 + 1, i * 3 + 2], [j * 2, j * 2 + 1.5, j * 2], linewidth=8,
                    solid_capstyle=cap, solid_joinstyle=join, color=f"C{i + 3 * j}")
    return fig


@case
def markers_all():
    fig = figure(4, 3)
    ax = fig.add_axes((0.05, 0.05, 0.9, 0.9))
    ax.set_axis_off()
    markers = [".", ",", "o", "v", "^", "<", ">", "1", "2", "3", "4", "8", "s", "p", "P", "*", "h", "H",
               "+", "x", "X", "D", "d", "|", "_", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
    for i, m in enumerate(markers):
        ax.plot([i % 8], [i // 8], marker=m, markersize=10, markerfacecolor="C1", markeredgecolor="C0",
                markeredgewidth=1.2, linestyle="none")
    return fig


@case
def scatter_sizes_colors():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    rng = np.random.default_rng(1)
    x, y = rng.random(60), rng.random(60)
    ax.scatter(x, y, s=rng.random(60) * 120, c=rng.random(60), alpha=0.6, edgecolors="k", linewidths=0.5)
    return fig


@case
def fill_between_alpha():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    x = np.linspace(0, 4 * np.pi, 200)
    ax.fill_between(x, np.sin(x), np.sin(x) + 1, alpha=0.4)
    ax.fill_between(x, np.cos(x) - 1, np.cos(x), alpha=0.4, hatch="//", edgecolor="C3")
    ax.plot(x, np.sin(x) + 0.5, color="k", linewidth=0.8)
    return fig


@case
def bars_hatched():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    hatches = ["/", "\\", "|", "-", "+", "x", "o", "O", ".", "*"]
    for i, h in enumerate(hatches):
        ax.bar(i, i + 1, hatch=h, color="w", edgecolor=f"C{i}")
    return fig


@case
def text_rotations():
    fig = figure()
    for i, angle in enumerate([0, 15, 45, 90, 180, 270]):
        fig.text(0.1 + 0.15 * i, 0.5, f"Angle {angle}", rotation=angle, fontsize=8 + i)
    fig.text(0.5, 0.9, "Centered Title", ha="center", va="top", fontsize=14, fontweight="bold")
    fig.text(0.02, 0.05, "italic small", style="italic", fontsize=7)
    fig.text(0.6, 0.05, "Mono 123", family="monospace", fontsize=9)
    return fig


@case
def mathtext_labels():
    fig = figure()
    ax = fig.add_axes((0.2, 0.2, 0.75, 0.7))
    ax.plot([1, 2, 3], [1, 4, 9])
    ax.set_xlabel(r"$\alpha_i^2 + \frac{\beta}{\gamma}$")
    ax.set_ylabel(r"$\sqrt{x} \cdot \int_0^\infty e^{-t} dt$")
    ax.set_title(r"$\mathrm{Re}(z) \leq \sum_{n=1}^{N} \hat{x}_n$")
    return fig


@case
def image_nearest():
    fig = figure()
    ax = fig.add_axes((0.1, 0.1, 0.8, 0.8))
    ax.imshow(np.arange(48).reshape(6, 8), interpolation="nearest")
    return fig


@case
def image_resampled():
    fig = figure()
    ax = fig.add_axes((0.1, 0.1, 0.8, 0.8))
    yy, xx = np.mgrid[-3:3:40j, -3:3:50j]
    ax.imshow(np.sin(xx * yy), interpolation="bilinear", cmap="magma")
    return fig


@case
def image_antialiased_downsample():
    fig = figure()
    ax = fig.add_axes((0.1, 0.1, 0.8, 0.8))
    rng = np.random.default_rng(2)
    ax.imshow(rng.random((500, 600)), interpolation="antialiased")
    return fig


@case
def pcolormesh_grid():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    x = np.linspace(0, 1, 11)
    y = np.linspace(0, 1, 8) ** 2
    ax.pcolormesh(x, y, np.add.outer(y[:-1], x[:-1]), edgecolors="w", linewidth=0.3)
    return fig


@case
def gouraud_triangles():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    rng = np.random.default_rng(3)
    x, y = rng.random(30), rng.random(30)
    ax.tripcolor(x, y, np.sin(4 * x) * np.cos(3 * y), shading="gouraud")
    return fig


@case
def clipped_by_patch():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    x = np.linspace(0, 10, 300)
    lines = ax.plot(x, np.sin(3 * x) * x / 2, linewidth=2)
    clip = Circle((5, 0), 3.5, transform=ax.transData, facecolor="none")
    lines[0].set_clip_path(clip)
    ax.add_patch(Circle((5, 0), 3.5, fill=False, linestyle="--"))
    ax.set_xlim(0, 10)
    ax.set_ylim(-5, 5)
    return fig


@case
def log_axes():
    fig = figure()
    ax = fig.add_axes((0.18, 0.15, 0.77, 0.75))
    x = np.logspace(-2, 3, 40)
    ax.loglog(x, x ** 1.5, marker="o", markersize=3)
    ax.grid(True, which="both", linewidth=0.3)
    return fig


@case
def errorbars_caps():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    x = np.arange(8)
    ax.errorbar(x, x ** 0.5, yerr=0.2 + 0.05 * x, xerr=0.2, fmt="o-", capsize=3, elinewidth=1)
    return fig


@case
def legend_box():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    for i in range(3):
        ax.plot([0, 1], [i, i + 1], label=f"series {i}", marker="os^"[i])
    ax.legend(title="Legend", shadow=True, fancybox=True, framealpha=0.7)
    return fig


@case
def annotate_arrow():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    ax.plot([0, 1, 2], [0, 1, 0])
    ax.annotate("peak", xy=(1, 1), xytext=(1.4, 0.5), arrowprops=dict(arrowstyle="->", connectionstyle="arc3,rad=.3"))
    ax.annotate("box", xy=(0.3, 0.3), xytext=(0.2, 0.8), bbox=dict(boxstyle="round", fc="0.9"),
                arrowprops=dict(arrowstyle="fancy", fc="C2"))
    return fig


@case
def polar_plot():
    fig = figure(3, 3)
    ax = fig.add_subplot(projection="polar")
    t = np.linspace(0, 2 * np.pi, 100)
    ax.plot(t, 1 + 0.5 * np.sin(5 * t))
    return fig


@case
def contour_filled():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    yy, xx = np.mgrid[-2:2:60j, -3:3:80j]
    z = np.exp(-xx ** 2 - yy ** 2) - np.exp(-(xx - 1) ** 2 - (yy - 0.5) ** 2)
    ax.contourf(xx, yy, z, levels=9, cmap="coolwarm")
    ax.contour(xx, yy, z, levels=9, colors="k", linewidths=0.5)
    return fig


@case
def sketched_lines():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    line, = ax.plot([0, 1, 2, 3], [0, 2, 1, 3], linewidth=2)
    line.set_sketch_params(scale=2, length=50, randomness=10)
    patch = ax.add_patch(Polygon([[0.5, 0.5], [2.5, 0.5], [1.5, 2.5]], alpha=0.3))
    patch.set_sketch_params(1, 30, 8)
    return fig


@case
def not_antialiased():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    x = np.linspace(0, 6, 40)
    ax.plot(x, np.sin(x), antialiased=False, linewidth=1.3)
    ax.plot(x, np.cos(x), antialiased=False, linestyle="--", linewidth=2.2)
    ax.text(1, 0, "no antialiasing", antialiased=False)
    return fig


@case
def subplots_grid():
    fig, axs = plt.subplots(2, 2, figsize=(4, 3), dpi=100)
    for i, ax in enumerate(axs.flat):
        ax.plot(np.arange(10) ** (i + 1))
        ax.set_title(f"panel {i}", fontsize=8)
    fig.suptitle("Grid")
    fig.tight_layout()
    return fig


@case
def histogram_steps():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    rng = np.random.default_rng(4)
    data = rng.normal(size=1000)
    ax.hist(data, bins=20, alpha=0.5)
    ax.hist(data * 0.7 + 1, bins=20, histtype="step", linewidth=1.5)
    return fig


@case
def long_random_walk():
    fig = figure()
    ax = fig.add_axes((0.12, 0.12, 0.83, 0.83))
    rng = np.random.default_rng(5)
    ax.plot(np.cumsum(rng.normal(size=20000)), linewidth=0.6)
    return fig


@case
def default_figure():
    fig = plt.figure()  # the full default size and dpi
    ax = fig.add_subplot()
    x = np.linspace(0, 2 * np.pi, 100)
    ax.plot(x, np.sin(x), label="sin")
    ax.plot(x, np.cos(x), label="cos")
    ax.set_xlabel("x")
    ax.set_ylabel("value")
    ax.set_title("Default figure")
    ax.legend()
    return fig
