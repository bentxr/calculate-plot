# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.scatter([0, 1, 2, 3, 4, 5], [1, 3, 2, 5, 4, 6], c=[0.5, 2.25, 1, 7, 3.5, 0], s=80)
ax.scatter([0.5, 1.5, 2.5, 3.5], [5, 5.5, 6, 0.5], c=[10, 20, 30, 40], vmin=15, vmax=35, marker='D', alpha=0.6, edgecolors='k')
ax.scatter([4.5, 5], [1, 2], c=[1, 2], facecolors='none', marker='o', s=200)

if __name__ == "__main__":
    plt.show()
