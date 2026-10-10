# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.fill_between([0, 1, 2, 3, 4], [1, 3, 2, 4, 3], [0, 0.5, 1, 1.5, 1], alpha=0.4)

if __name__ == "__main__":
    plt.show()
