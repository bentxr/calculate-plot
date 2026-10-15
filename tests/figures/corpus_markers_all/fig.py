# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'backend_fallback': False,
})
fig1 = plt.figure(figsize=(4, 3), dpi=100)
ax1 = fig1.add_axes((0.05, 0.05, 0.9, 0.9))
ax1.set_axis_off()
ax1.plot([0], [0], marker='.', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([1], [0], marker=',', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([2], [0], marker='o', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([3], [0], marker='v', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([4], [0], marker='^', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([5], [0], marker='<', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([6], [0], marker='>', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([7], [0], marker='1', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([0], [1], marker='2', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([1], [1], marker='3', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([2], [1], marker='4', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([3], [1], marker='8', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([4], [1], marker='s', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([5], [1], marker='p', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([6], [1], marker='P', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([7], [1], marker='*', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([0], [2], marker='h', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([1], [2], marker='H', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([2], [2], marker='+', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([3], [2], marker='x', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([4], [2], marker='X', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([5], [2], marker='D', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([6], [2], marker='d', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([7], [2], marker='|', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([0], [3], marker='_', markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([1], [3], marker=0, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([2], [3], marker=1, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([3], [3], marker=2, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([4], [3], marker=3, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([5], [3], marker=4, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([6], [3], marker=5, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([7], [3], marker=6, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([0], [4], marker=7, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([1], [4], marker=8, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([2], [4], marker=9, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([3], [4], marker=10, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')
ax1.plot([4], [4], marker=11, markersize=10, markerfacecolor='C1', markeredgecolor='C0', markeredgewidth=1.2, linestyle='none')

if __name__ == "__main__":
    plt.show()
