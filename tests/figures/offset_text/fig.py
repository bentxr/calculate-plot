# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.set_xlim(100000, 100010)
ax.set_ylim(-1e-06, 3e-06)
ax.set_xlabel('position')

if __name__ == "__main__":
    plt.show()
