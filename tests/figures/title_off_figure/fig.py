# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'figure.subplot.top': -0.2,
    'figure.subplot.bottom': -0.6,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.set_title('Below the figure', pad=-20)
ax.set_title('left', loc='left', pad=-20)

if __name__ == "__main__":
    plt.show()
