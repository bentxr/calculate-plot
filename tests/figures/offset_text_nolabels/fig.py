# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'xtick.labelbottom': False,
    'ytick.labelleft': False,
})
fig, ax = plt.subplots()
ax.plot([10000, 10001], [1000000, 3000000])

if __name__ == "__main__":
    plt.show()
