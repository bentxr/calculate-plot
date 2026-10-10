# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2, 3, 4], [1, 10, 300, 2000, 50000], marker='o')
ax.set_yscale('log')

if __name__ == "__main__":
    plt.show()
