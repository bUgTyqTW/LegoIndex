import matplotlib.pyplot as plt

# Initialize lists to store data
block_nums = []
counts = []
times = []

# Read the log file and extract data
# with open('../cmake-build-debug/adios_read_log.txt', 'r') as log_file:
with open('./log/adios_read_log_bp10000.txt', 'r') as log_file:
    for line in log_file:
        if "The blockNum is" in line and "The Count is" in line:
            parts = line.split()
            block_num = int(parts[3][:len(parts[3]) - 1])
            count = int(parts[-1])
            block_nums.append(block_num)
            counts.append(count)
        elif "The code execution took" in line:
            time = float(line.split()[-2])
            times.append(time)

# Create two subplots
plt.figure(figsize=(18, 6))

# Plot 1: blockNum - Count
plt.subplot(1, 3, 1)
plt.plot(block_nums, counts, marker='o', linestyle='-', color='b')
plt.title('blockNum - Count')
plt.xlabel('blockNum')
plt.ylabel('Count')

# Plot 2: blockNum - time
plt.subplot(1, 3, 2)
plt.plot(block_nums, times, marker='o', linestyle='-', color='r')
plt.title('blockNum - Time')
plt.xlabel('blockNum')
plt.ylabel('Time (seconds)')

# Plot 3: Count - time
plt.subplot(1, 3, 3)
plt.plot(counts, times, marker='o', linestyle='-', color='g')
plt.title('Count - Time')
plt.xlabel('Count')
plt.ylabel('Time (seconds)')

# Save the blockNum - Time plot as an image
plt.savefig('output/blockNum_time_plot_bp10000.png')

# Close the plots
plt.close()

