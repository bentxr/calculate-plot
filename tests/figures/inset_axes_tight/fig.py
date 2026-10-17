# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'savefig.bbox': 'tight',
})
fig, ax = plt.subplots()
ax.plot([1, 2], [3, 1])
ins = ax.inset_axes([0.8, 0.8, 0.4, 0.4])
ins.set_title('over the edge')
ax.set_aspect(0.5)

if __name__ == "__main__":
    plt.show()
