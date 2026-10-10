# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([1, 10, 100, 1000], [1, 3, 2, 5])
ax.set_xscale('log')
ax.grid(which='both', axis='x', color='0.6', ls=':', lw=0.5)
ax.grid(axis='y', color='#ff000080', linewidth=1.5)

if __name__ == "__main__":
    plt.show()
