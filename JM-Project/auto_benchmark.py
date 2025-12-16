import subprocess
import re
import matplotlib.pyplot as plt
import pandas as pd
import sys

# 很重要 ：先make 再跑這個腳本。
# Important: run make first before running this script.
C_PROGRAM_PATH = "./bin/lencod" 

def run_c_program():
    print(f"Running C program: {C_PROGRAM_PATH} ...")
    try:
        # subprocess.run : executes the command in the terminal
        # capture_output=True : captures stdout/stderr instead of just printing to screen
        result = subprocess.run([C_PROGRAM_PATH], capture_output=True, text=True)
        
        if result.returncode != 0:
            print("Error: C program execution failed!")
            print(result.stderr)
            sys.exit(1)
            
        print("C program execution completed! Analyzing data...")
        return result.stdout
    except FileNotFoundError:
        print(f"Error: File {C_PROGRAM_PATH} not found. Please verify you have run 'make'.")
        sys.exit(1)

# Tools to extract numbers
def parse_output(output_text):
    data = []
    
    # Update Regex to capture Loss (optional group at the end)
    # Group 1: Mode
    # Group 2: Points
    # Group 3: Time
    # Group 4: SAD
    # Group 5: Loss (Optional, because FS doesn't have it)
    pattern = re.compile(r"(FS baseline|\[DS \w+\]).*?Points:\s*(\d+).*?Time:\s*([\d\.]+)\s*ms.*?Avg SAD:\s*([\d\.]+)(?:\s*\|\s*Avg Loss vs FS:\s*([\d\.]+)%)?")
    
    print("\n--- Captured Data Details ---")
    for line in output_text.split('\n'):
        match = pattern.search(line)
        if match:
            mode = match.group(1).replace('[', '').replace(']', '') # Remove brackets
            points = int(match.group(2))
            time = float(match.group(3))
            sad = float(match.group(4))
            
            # Loss might be None for FS baseline
            loss_str = match.group(5) 
            loss_val = float(loss_str) if loss_str else 0.0
            
            # Format the output string to look like the original C output
            loss_display = f"| Loss: {loss_str}%" if loss_str else ""
            
            data.append({
                'Mode': mode,
                'Points': points,
                'Time (ms)': time,
                'Avg SAD': sad,
                'Loss (%)': loss_str if loss_str else "-" # Put '-' for FS in CSV
            })
            
            # Print exactly like the original C output style
            print(f"  -> {mode:<12} | Points: {points:<7} | Time: {time:>6} ms | Avg SAD: {sad} {loss_display}")
            
    return pd.DataFrame(data)

# Auto Plotting 
def plot_charts(df):
    if df.empty:
        print("Error: No data captured. Please check C program output format.")
        return

    modes = df['Mode'].tolist()
    times = df['Time (ms)'].tolist()
    points = df['Points'].tolist()
    
    #  Time Comparison chart
    plt.figure(figsize=(10, 6))
    bars = plt.bar(modes, times, color=['#4E79A7', '#E15759', '#F28E2B'])
    for bar in bars:
        plt.text(bar.get_x() + bar.get_width()/2, bar.get_height(), f'{bar.get_height()} ms', ha='center', va='bottom', fontweight='bold')
    plt.title('Execution Time', fontsize=14)
    plt.ylabel('Time (ms)')
    plt.yscale('log') # Logarithmic scale
    plt.savefig('chart_time.png', dpi=300)
    print("\nChart generated: chart_time.png")

    # Points Comparison chart 
    plt.figure(figsize=(10, 6))
    bars = plt.bar(modes, points, color=['#4E79A7', '#E15759', '#F28E2B'])
    for bar in bars:
        plt.text(bar.get_x() + bar.get_width()/2, bar.get_height(), f'{bar.get_height()}', ha='center', va='bottom')
    plt.title('Search Points', fontsize=14)
    plt.ylabel('Points')
    plt.yscale('log')
    plt.savefig('chart_points.png', dpi=300)
    print("Chart generated: chart_points.png")
    
    # Export CSV
    df.to_csv('result.csv', index=False)
    print("Table generated: result.csv")

if __name__ == "__main__":
    raw_output = run_c_program() 
    df = parse_output(raw_output) 
    plot_charts(df)