# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([-10, 20], [-10, 20], label='rising')
ax.plot([-10, 20], [20, -10], label='falling')
ax.plot([-10, 20], [5, 5], label='middle')
ax.set_xlim(0, 10)
ax.set_ylim(0, 10)
ax.legend()

if __name__ == "__main__":
    plt.show()
