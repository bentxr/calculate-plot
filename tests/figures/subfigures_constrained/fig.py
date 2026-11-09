# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig = plt.figure(figsize=(7.0, 4.5), layout='constrained')
sfigs = fig.subfigures(1, 2, width_ratios=[3, 2], wspace=0.07)
sfigs[0].set_facecolor('0.92')
axl = sfigs[0].subplots(2, 2, sharey='row')
axl[0, 0].plot([0, 1, 2, 3], [10, 30, 20, 40], label='first')
axl[0, 1].plot([0, 1, 2, 3], [40, 20, 30, 10], label='second', color='C1')
axl[1, 0].bar([1, 2, 3], [3, 1, 2])
axl[1, 0].set_xlabel('a long label below')
axl[1, 1].set_ylabel('values')
sfigs[0].suptitle('Left side')
sfigs[0].legend(loc='outside lower center', ncols=2)
rtop, rbottom = sfigs[1].subfigures(2, 1, height_ratios=[1, 2])
ax1 = rtop.subplots()
ax1.plot([1, 2, 3], [3, 1, 2])
ax1.set_title('top right')
p = rbottom.add_subplot(projection='polar')
p.plot([0, 1, 2], [1, 2, 1])
rbottom.suptitle('polar', fontsize='small')
fig.suptitle('Constrained subfigures')

if __name__ == "__main__":
    plt.show()
