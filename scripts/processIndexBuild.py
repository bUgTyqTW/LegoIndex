import os
import re
import argparse
import pandas as pd

def extract_data_from_log(log_file):
    """
    Extracts build time and elapsed time from a log file.
    """
    build_time = None
    elapsed_time = None
    
    io_time = 0

    with open(log_file, 'r') as file:
        lines = file.readlines()
        for line in lines:
            build_time_match = re.search(r'IO and CPU Finished in (.*?)s', line)
            elapsed_time_match = re.search(r'Elapsed time: (.*?) s', line)
            # Read Batch finished in 3.32284s
            batch_time_match = re.search(r'Batch finished in (.*?)s', line)
            
            if build_time_match:
                build_time = float(build_time_match.group(1))
                
            if elapsed_time_match:
                elapsed_time = float(elapsed_time_match.group(1))

            if batch_time_match:
                io_time += float(batch_time_match.group(1))

    print(f'Extracted build time: {build_time}, elapsed time: {elapsed_time}, I/O time: {io_time} from {log_file}')
    return build_time, elapsed_time, io_time

def parse_filename(filename):
    """
    Parses the filename to extract details such as index, minmax, threads, and blocks.
    """
    filename_pattern = re.compile(r'index_build_(rtree|minmax)_threads_(\d+)_blocks_(\d+)_(\d+)\.log')
    match = filename_pattern.match(filename)
    
    if match:
        index_type = match.group(1)
        threads = int(match.group(2))
        blocks = int(match.group(3))
        run_number = int(match.group(4))
        return index_type, threads, blocks, run_number
    
    return None, None, None, None

def process_log_files(log_directory):
    """
    Processes all log files in the specified directory and returns a DataFrame with the extracted data.
    """
    data = []

    for log_file in os.listdir(log_directory):
        if log_file.endswith('.log'):
            file_path = os.path.join(log_directory, log_file)
            index_type, threads, blocks, run_number = parse_filename(log_file)
            build_time, elapsed_time, io_time = extract_data_from_log(file_path)
            
            if index_type and threads is not None and blocks is not None:
                data.append({
                    'File Name': log_file,
                    'Index Type': index_type,
                    'Threads': threads,
                    'Blocks': blocks,
                    'I/O Time (s)': io_time,
                    'Build Time (s)': build_time,
                    'CPU Time (s)': build_time - io_time if build_time is not None else None,
                    'Elapsed Time (s)': elapsed_time
                })
    return pd.DataFrame(data)

def main():
    parser = argparse.ArgumentParser(description='Process log files to extract build and elapsed times.')
    parser.add_argument('--log_directory', type=str, help='Directory containing log files.')
    parser.add_argument('--output_file', type=str, help='Output file')

    args = parser.parse_args()
    
    log_directory = args.log_directory
    output_file = args.output_file

    result_df = process_log_files(log_directory)
    result_df = result_df.sort_values(by=['Index Type', 'Threads', 'Blocks'])
    if output_file is not None:
        result_df.to_csv(output_file, index=False)
        print(f'Summary written to {output_file}')

if __name__ == '__main__':
    main()
