# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2, 3, 4, 5, 6, 7], [1, 2, -1, -2, 1, 3, -1, 0.5])
ax.fill_between([0, 1, 2, 3, 4, 5, 6, 7], [1, 2, -1, -2, 1, 3, -1, 0.5], 0, where=[True, True, False, False, True, True, False, True], color='green', alpha=0.3)
ax.fill_between([0, 2, 4, 6], [-3, -2.5, -3.5, -3], facecolor='none', edgecolor='C3', linewidth=2)
ax.fill_between([1, 2, 3], [2.5, 3.5, 2.5], [2, 2, 2], label='band', zorder=3)

if __name__ == "__main__":
    plt.show()
