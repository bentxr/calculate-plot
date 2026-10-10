# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'savefig.bbox': 'tight',
})
fig = plt.figure(figsize=(2.0, 1.5), facecolor='0.8')

if __name__ == "__main__":
    plt.show()
