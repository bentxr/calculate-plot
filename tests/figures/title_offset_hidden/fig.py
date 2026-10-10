# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'ytick.labelleft': False,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1], [0, 3000000])
ax.set_title('The offset text is not shown', loc='left')

if __name__ == "__main__":
    plt.show()
