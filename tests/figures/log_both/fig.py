# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0.002, 0.05, 1.3, 40, 900], [3, 0.4, 0.07, 0.009, 0.0004])
ax.set_xscale('log')
ax.set_yscale('log')
ax.set_xlabel('frequency')

if __name__ == "__main__":
    plt.show()
