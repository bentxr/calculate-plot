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
fig = plt.figure(figsize=(5.0, 3.0))
p = fig.add_subplot(121, projection='polar')
p.plot([0, 1, 2, 3], [1, 2, 1, 3], label='line')
p.fill([0, 1, 2], [0.5, 1, 0.5], alpha=0.4)
p.set_title('polar')
ax = fig.add_subplot(122)
ax.plot([0, 1], [1, 0])

if __name__ == "__main__":
    plt.show()
