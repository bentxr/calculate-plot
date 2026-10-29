# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axd = plt.subplot_mosaic('AAB;CDB;CEE', figsize=(9.0, 6.0), width_ratios=[2, 1, 1], height_ratios=[1, 2, 1], sharex=True)
axd['A'].plot([0.3, 1.7, 2.2, 4.9, 6.1], [1.1, -0.4, 2.8, 0.9, 3.7])
axd['A'].set_title('A')
axd['B'].plot([0.6, 3.4, 4.4, 9.8, 12.2], [1.1, -0.4, 2.8, 0.9, 3.7])
axd['B'].set_title('B')
axd['C'].plot([0.8999999999999999, 5.1, 6.6000000000000005, 14.700000000000001, 18.299999999999997], [1.1, -0.4, 2.8, 0.9, 3.7])
axd['C'].set_title('C')
axd['D'].plot([1.2, 6.8, 8.8, 19.6, 24.4], [1.1, -0.4, 2.8, 0.9, 3.7])
axd['D'].set_title('D')
axd['E'].plot([1.5, 8.5, 11.0, 24.5, 30.5], [1.1, -0.4, 2.8, 0.9, 3.7])
axd['E'].set_title('E')

if __name__ == "__main__":
    plt.show()
