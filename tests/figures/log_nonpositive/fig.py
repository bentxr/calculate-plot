# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.set_yscale('log')
ax.plot([0, 1, 2, 3, 4, 5], [5, 0, 20, -3, 400, 7000], marker='s')
ax.set_ylim(bottom=0.5)
ax.set_xlim(5.5, -0.5)

if __name__ == "__main__":
    plt.show()
