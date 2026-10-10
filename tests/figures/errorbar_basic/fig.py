# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.errorbar([0, 1, 2, 3], [1, 2.5, 2, 3], yerr=[0.2, 0.4, 0.3, 0.5], fmt='o', capsize=3)

if __name__ == "__main__":
    plt.show()
