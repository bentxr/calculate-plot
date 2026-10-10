# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'xtick.labeltop': True,
    'xtick.top': True,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1], [0, 3])
ax.set_title('Labels above and below')

if __name__ == "__main__":
    plt.show()
