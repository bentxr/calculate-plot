# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.figure(figsize=(5.0, 5.0))
plt.polar([0.0, 0.209439, 0.418879, 0.628319, 0.837758, 1.047198, 1.256637, 1.466076, 1.675516, 1.884956, 2.094395, 2.303834, 2.513274, 2.722713, 2.932153, 3.141592, 3.351032, 3.560471, 3.769911, 3.97935, 4.18879, 4.398229, 4.607669, 4.817108, 5.026548, 5.235987, 5.445427, 5.654866, 5.864306, 6.073745, 6.283185], [2.2, 2.066313, 1.716312, 1.283687, 0.933688, 0.8, 0.933688, 1.283687, 1.716312, 2.066312, 2.2, 2.066313, 1.716312, 1.283689, 0.933688, 0.8, 0.933688, 1.283687, 1.716312, 2.066311, 2.2, 2.066313, 1.716312, 1.28369, 0.933688, 0.8, 0.933688, 1.283687, 1.716311, 2.066311, 2.2], 'r--')
plt.polar([0.5, 1.5], [2.0, 1.0], marker='x')
plt.title('plt.polar')

if __name__ == "__main__":
    plt.show()
