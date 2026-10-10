# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(5.1, 2.3))
ax.plot([0, 1, 2], [0, 1, 4])
ax.set_title('510 by 230 pixels')
ax.set_ylabel('y')

if __name__ == "__main__":
    plt.show()
