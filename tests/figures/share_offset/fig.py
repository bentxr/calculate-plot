# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 2, sharex=True, sharey=True, figsize=(6.0, 4.0))
axs[0, 0].plot([10000, 10001], [1000000, 3000000])
axs[1, 1].plot([10000.5, 10002], [2000000, 5000000])
axs[0, 1].grid(True)

if __name__ == "__main__":
    plt.show()
