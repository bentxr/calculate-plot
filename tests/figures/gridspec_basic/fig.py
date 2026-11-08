# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(figsize=(7.0, 5.0))
gs = fig.add_gridspec(3, 3, width_ratios=[1, 2, 1], hspace=0.45, left=0.08, right=0.97, figure=None)
top = fig.add_subplot(gs[0, :])
top.plot([0, 1, 2, 3], [1, 3, 2, 4])
top.set_title('gs[0, :]')
mid = fig.add_subplot(gs[1, :-1])
mid.set_title('gs[1, :-1]')
fig.add_subplot(gs[1:, -1], projection='polar')
fig.add_subplot(gs[-1, 0])
flat = fig.add_subplot(gs[7])
flat.set_title('gs[7]')
plt.subplot(gs[0, :])
plt.plot([0, 3], [4, 1], color='C3')
plt.subplot(gs[1, 0:2:1])
plt.plot([0, 1], [0, 1])

if __name__ == "__main__":
    plt.show()
