import subprocess
import argparse
import json
import os

governer=""
max_freq=0
min_freq=0


def main():
    cores_avail = subprocess.check_output("lscpu | grep 'CPU(s):' | awk '{print $2}' | head -n 1", shell=True).decode('utf-8').strip()
    parser = argparse.ArgumentParser(prog='cpufreq', description='This\
                            program will adjusts the scaling_min_freq for benhmarking')
    parser.add_argument("CPU", help="Specify the CPU core on which benchmarking will be performed",
                            type=int, choices = range(0,(int)(cores_avail)))

    parser.add_argument("-c", "--core", help="Set/ Reset the scaling_min_freq",
                            type = str, choices =['s','r'])

    args = parser.parse_args()

    print("CPU-Core:",args.CPU)

    if args.core == 's':
        print("enabled [SET mode]")
        #check if min_freq == max_freq
        min_freq= subprocess.check_output("cat /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_min_freq", shell = True).decode('utf-8').strip()
        max_freq= subprocess.check_output("cat /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_max_freq", shell = True).decode('utf-8').strip()
        print("MAX_FREQ:",max_freq)
        print("MIN_FREQ:",min_freq)
        if min_freq == max_freq:
            print("CPU core already set")
        else:
            previous_state['max_freq'] = max_freq
            previous_state['min_freq'] = min_freq
            governer = subprocess.check_output("cat /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_governor", shell = True).decode('utf-8').strip()
            previous_state['governer'] = governer
            with open(str(args.CPU)+'_core.txt', 'w') as f:
                f.write(json.dumps(previous_state))
            #setting the min_freq to max_freq
            min_freq = max_freq
            cmd = f"echo {min_freq} > /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_min_freq"
            status = subprocess.run(cmd,shell = True)
            if status.returncode == 0:
                print("Set Min Freq: SUCCESSFUL")
                print("MAX_FREQ:",max_freq)
                print("MIN_FREQ:",min_freq)
            else:
                print("Set Min Freq: FAILED")



    if args.core ==  'r':
        print("enabled [RESET mode]")
        min_freq= subprocess.check_output("cat /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_min_freq", shell = True).decode('utf-8').strip()
        max_freq= subprocess.check_output("cat /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_max_freq", shell = True).decode('utf-8').strip()

        if min_freq == max_freq:
            print("MAX_FREQ:",max_freq)
            print("MIN_FREQ:",min_freq)

            if not os.path.exists(str(args.CPU)+'_core.txt'):
                print("file not found.. Reset Manually")
                print("Reset FAILED")

            else:
                with open(str(args.CPU)+'_core.txt','r') as f:
                    new_state = json.load(f)

                    cmd = f"echo {new_state['min_freq']} > /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_min_freq"
                    status=subprocess.run(cmd,shell= True)
                    if status.returncode == 0:
                        min_freq= subprocess.check_output("cat /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_min_freq", shell = True).decode('utf-8').strip()
                        max_freq= subprocess.check_output("cat /sys/devices/system/cpu/cpu"+str(args.CPU)+"/cpufreq/scaling_max_freq", shell = True).decode('utf-8').strip()
                        print("Reset Min Freq: SUCCESSFUL")
                        print("MAX_FREQ:",max_freq)
                        print("MIN_FREQ:",min_freq)
                    else:
                        print("Reset FAILED")
        else:
            print("Running at default Core Frequency")


if __name__=="__main__":
    main()

