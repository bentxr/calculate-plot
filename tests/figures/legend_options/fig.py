# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2], [0, 1, 4], label='a', marker='^')
ax.plot([0, 1, 2], [0, 2, 3], label='bb')
ax.plot([0, 1, 2], [1, 1, 1], label='ccc', ls=':')
ax.scatter([0.5, 1.5], [3, 3.5], c=[1, 2], label='mapped')
ax.legend(loc='center right', ncols=2, fancybox=False, markerfirst=False, framealpha=0.5, fontsize='small', borderpad=1.0, edgecolor='red', title='Two\ncolumns')

if __name__ == "__main__":
    plt.show()
