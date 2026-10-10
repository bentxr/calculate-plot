# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(5.0, 3.0))
ax.plot([2, 3, 5, 7.5], [0.31, 0.29, 0.6, 0.45], marker='o')
ax.set_xscale('log')
ax.set_yscale('log')

if __name__ == "__main__":
    plt.show()
