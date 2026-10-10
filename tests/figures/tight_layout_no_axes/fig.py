# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig = plt.figure(figsize=(3.0, 2.0))
fig.suptitle('Only a title')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
