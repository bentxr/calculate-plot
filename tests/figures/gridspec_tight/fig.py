# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(figsize=(6.0, 4.0))
gs = fig.add_gridspec(2, 2)
a = fig.add_subplot(gs[:, 0])
a.set_ylabel('a label that is long')
sub = gs[0, 1].subgridspec(1, 2)
b = fig.add_subplot(sub[0, 0])
b.set_title('b')
fig.add_subplot(sub[0, 1])
c = fig.add_subplot(gs[1, 1])
c.set_xlabel('c')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
