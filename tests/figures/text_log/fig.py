# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(5.0, 3.5))
ax.plot([1, 10, 100], [1, 100, 10000])
ax.set_xscale('log')
ax.set_yscale('log')
ax.text(3.0, 300.0, '$y=x^2$ on log axes', rotation=35)
ax.text(0.0, 10.0, 'at x = 0: nowhere')
ax.text(10.0, -5.0, 'at y < 0: nowhere')
ax.text(200.0, 2.0, 'outside, to the right')

if __name__ == "__main__":
    plt.show()
