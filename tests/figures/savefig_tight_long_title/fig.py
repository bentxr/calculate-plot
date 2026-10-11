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
fig, ax = plt.subplots(figsize=(2.0, 1.6))
ax.set_title('A title much wider than the small figure it sits on')
ax.set_xlabel('and an x label that is also wider than the Axes')
ax.set_ylabel('a y label taller than the Axes are')

if __name__ == "__main__":
    plt.show()
