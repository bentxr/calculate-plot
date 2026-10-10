# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 4], [0, 4])
ax.scatter([0, 1, 2], [1, 2, 3], c=['red', '#00ff00', 'C4'], edgecolors='k', linewidths=2)
ax.scatter([1, 2, 3], [0.5, 1, 1.5], marker='x', s=60)
ax.scatter([2, 3, 4], [3, 3.5, 2], marker='^', color='purple', alpha=0.5, s=[10, 40, 90])
ax.scatter([3.5], [0.2], facecolors='none', edgecolors='C1', s=150)
ax.scatter([0.5, 1.5], [3.8, 0.2], marker='+', linewidths=[1, 3], zorder=3)

if __name__ == "__main__":
    plt.show()
