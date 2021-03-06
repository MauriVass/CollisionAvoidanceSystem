#to install
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

model = 0
run = 4
# losses = pd.read_csv(f'run.losses_{model}_{run}.csv')
rewards = pd.read_csv(f'run.rewards_{model}_{run}.csv')

plot_losses = False

if(plot_losses is True):
	fig = plt.figure(figsize=(24,8))
	ax = plt.axes()
	
	y_avg = losses.rolling(400).mean()
	x = np.arange(len(losses))
	y = np.array(losses)
	#y = np.clip(y, 0, 0.01)

	ax.scatter(x, y, s=0.1)
	#ax.plot(x,y,linewidth=.05, marker='o', markersize=0.5)
	ax.plot(x,y_avg,linewidth=1,c='r')
else:
	# for i in range(5):
	for i in range(2):
		colors = []
		for r in rewards['IsClockwise']:
			if(r==0):
				c = 'r'
			else:
				c = 'b'
			colors.append(c)
		fig, axs = plt.subplots(2,figsize=(24,8), sharex=True)
		# rolling = 1 + i*2
		rolling = 1 + i * 8
		rewards['rolling_avg_reward'] = rewards['totalReward'].rolling(rolling).mean()
		rewards['rolling_avg_speed'] = rewards['averageSpeed'].rolling(rolling).mean()
		x = np.arange(len(rewards))
		y1 = np.array(rewards['rolling_avg_reward'])
		y2 = np.array(rewards['rolling_avg_speed'])

		fig.suptitle(f'Rolling: {rolling}')

		#ax.scatter(x, y, s=0.1)
		axs[0].plot(x,y1,linewidth=.5, markersize=2)
		axs[0].scatter(x,y1,c=colors, s=2)
		axs[0].set_ylabel('Total Reward')

		axs[1].plot(x,y2,linewidth=.5, markersize=2)
		axs[1].scatter(x,y2,c=colors, s=2)
		axs[1].set_xlabel('Epoch')
		axs[1].set_ylabel('Avg Velocity')
		plt.savefig(f'Plots/rewards_{rolling}')
plt.show()