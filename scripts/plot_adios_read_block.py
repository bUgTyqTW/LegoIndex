import matplotlib.pyplot as plt

# Initialize lists to store data
block_nums = []
counts = []
times = []

# Read the log file and extract data
# with open('./log/adios_read_block_position_bp9000_hydrogen.log', 'r') as log_file:
# with open('./log/adios_read_block_position_bp10000_electrons.log', 'r') as log_file:
with open('./log/adios_read_block_position_bp10000_hydrogen.log', 'r') as log_file:
# with open('./log/adios_read_log_bp10000.txt', 'r') as log_file:
    for line in log_file:
        # std::cout << "The blockSeq is " << i << ", The Start is " << var_info1.Start[0] << ", The Count is " << var_info1.Count[0] << std::endl;
        # The blockSeq is 1, The Start is 88582, The Count is 75907
        # The code execution took 0.131693 seconds.
        if "The blockSeq is" in line and "The Start is" in line and "The Count is" in line:
            block_num = int(line.split()[3][:-1])
            count = int(line.split()[-1])
            block_nums.append(block_num)
            counts.append(count)

        elif "The code execution took" in line:
            time = float(line.split()[-2])
            times.append(time)

# Create two subplots
plt.figure(figsize=(18, 6))

# Plot 1: blockNum - Count
plt.subplot(1, 3, 1)
plt.scatter(block_nums, counts, marker='o', linestyle='-', color='b')
plt.title('blockSeq - Count')
plt.xlabel('blockSeq')
plt.ylabel('Count')

# Plot 2: blockNum - time
plt.subplot(1, 3, 2)
plt.scatter(block_nums, times, marker='o', linestyle='-', color='r')
plt.title('blockSeq - Time')
plt.xlabel('blockSeq')
plt.ylabel('Time (seconds)')

# Plot 3: Count - time
plt.subplot(1, 3, 3)
plt.scatter(counts, times, marker='o', linestyle='-', color='g')
plt.title('Count - Time')
plt.xlabel('Count')
plt.ylabel('Time (seconds)')

# Save the blockNum - Time plot as an image
plt.savefig('output/blockSeq-Count_time_plot_bp10000_hydrogen.png')

# Close the plots
plt.close()

