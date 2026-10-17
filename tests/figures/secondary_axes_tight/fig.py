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
fig, ax = plt.subplots(figsize=(6.0, 4.0))
ax.plot([0, 20, 40, 60, 80, 100], [1, 3, 2, 5, 4, 6])
ax.set_title('remaining')
sec = ax.secondary_xaxis('top', functions=(lambda x: 100 - x, lambda x: 100 - x))
sec.set_xticks([0, 25, 50, 75, 100])
sec.tick_params(labelsize=8, direction='in')
sec.set_color('tab:red')
sec.set_xlabel('left over')
far = ax.secondary_yaxis(1.15, functions=(lambda x: x * 1000, lambda x: x / 1000))
far.set_ylabel('thousands')

if __name__ == "__main__":
    plt.show()
