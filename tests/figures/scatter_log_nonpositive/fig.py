# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots()
ax.scatter([0, 3, 40, -2], [5, 0, 2, 7])
ax.scatter([1, -1, 20, 0], [3, 4, -5, 0], s=[10, 40, 90, 160])
ax.set_xscale('log')
ax.set_yscale('log')

if __name__ == "__main__":
    plt.show()
