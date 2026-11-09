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
fig = plt.figure(figsize=(6.0, 5.0))
top, bottom = fig.subfigures(2, 1, height_ratios=[1, 2], edgecolor='k', linewidth=2, facecolor='lightyellow', frameon=True)
taxs = top.subplots(1, 3, sharey=True)
taxs[1].bar([1, 2, 3], [3, 1, 2])
inner = bottom.subfigures(1, 2, squeeze=False)
a = inner[0, 0].add_subplot()
a.set_title('a')
a.set_ylabel('far to the left', labelpad=40)
inner[0, 1].set_facecolor('lightblue')
b = inner[0, 1].subplots(subplot_kw={'projection': 'polar'})
inner[0, 1].suptitle('nested')
plt.plot([0, 1], [1, 0])

if __name__ == "__main__":
    plt.show()
