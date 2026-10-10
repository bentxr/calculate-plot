# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([-1, 0, 0.5, 3, 40], [1, 2, 3, 2.5, 1.5], marker='^')
ax.set_xscale('log')

if __name__ == "__main__":
    plt.show()
