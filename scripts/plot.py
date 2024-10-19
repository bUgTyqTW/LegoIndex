# use pandas to plot the data: scripts/log_summary.csv

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Read the log summary file
log_summary = pd.read_csv('/home/4cv/project/GEOSIndex/scripts/log_summary.csv')

# remove nan values
log_summary = log_summary.dropna()

# only keep 'Index Type' == 'minmax'
log_summary = log_summary[log_summary['Index Type'] == 'minmax']

# only keep columns Threads,Blocks,Build Time (s),Elapsed Time (s)
log_summary = log_summary[['Threads', 'Blocks', 'Build Time (s)', 'Elapsed Time (s)']]

# get average Build Time (s),Elapsed Time (s) for each threads and blocks
log_summary = log_summary.groupby(['Blocks', 'Threads']).mean().reset_index()

log_summary['Elapsed Time (s)'] = log_summary['Elapsed Time (s)'].apply(lambda x: np.log10(x))

fig, ax = plt.subplots()

# set figure size
fig.set_size_inches(10, 6)

# plot bar chart, group by block and threads as x-axis, y-axis is the average elapsed time
log_summary.pivot(index='Blocks', columns='Threads', values='Elapsed Time (s)').plot(kind='bar', ax=ax)

for p in ax.patches:
    ax.annotate(str(round(10**p.get_height(), 2)), (p.get_x() * 1, p.get_height() * 1.02))

plt.xlabel('Block Num')
plt.ylabel('Time (seconds)')

# x-axis rotation
plt.xticks(rotation=0)

plt.show()

print()

