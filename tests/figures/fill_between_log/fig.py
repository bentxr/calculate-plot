# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.fill_between([1, 2, 3, 4, 5], [10, 100, 50, 500, 200], [1, 5, 2, 20, 8], alpha=0.5)
ax.set_yscale('log')

if __name__ == "__main__":
    plt.show()
