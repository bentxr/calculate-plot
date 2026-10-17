# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'ytick.labelleft': False,
    'xtick.labelbottom': False,
    'ytick.direction': 'out',
    'xtick.direction': 'in',
})
fig, ax = plt.subplots(figsize=(2.5, 4.0))
ax.set_xlim(0, 1)
ax.set_ylim(-3, 41)
ax.set_xlabel('x')
ax.set_ylabel('y')

if __name__ == "__main__":
    plt.show()
