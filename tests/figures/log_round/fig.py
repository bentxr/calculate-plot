# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'axes.autolimit_mode': 'round_numbers',
})
fig, ax = plt.subplots(figsize=(4.0, 3.0), dpi=90)
ax.plot([0.03, 0.4, 7, 33], [1.5e-05, 0.002, 0.3, 12])
ax.set_xscale('log')
ax.set_yscale('log')
ax.set_ylabel('$\\mathdefault{y_{0}}$ (m)')

if __name__ == "__main__":
    plt.show()
