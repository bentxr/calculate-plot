# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(3.0, 3.0))
ax.set_title('High', y=1.1)
ax.set_ylim(1000000.0, 1000004.5)

if __name__ == "__main__":
    plt.show()
