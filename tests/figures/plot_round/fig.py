# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'axes.autolimit_mode': 'round_numbers',
    'axes.xmargin': 0,
    'axes.ymargin': 0,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0.3, 1.7, 4.2], [13, -2.5, 7.75])

if __name__ == "__main__":
    plt.show()
