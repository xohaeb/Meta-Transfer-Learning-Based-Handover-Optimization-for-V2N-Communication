# Instruction of using the ns3-ai-based HO evaluation

These files are placed related to a ns-3 root folder (where you call the "./waf xxxxx" command).

## What are these files 
contrib: this folder contains the ns3-ai module

FadingCh: this folder contains the generated fading traces (the xxxx.fad files)

scratch: this folder contains the source code for the main simulation scripts and necessary custom models

src: contains the models that have been modified

xxxxx.tcl files: the UE moving trajectory files

## How to use them

i. copy all the files to your ns-3 root folder

ii. call a terminal at the ns-3 root folder and run "./waf --configure", then "./waf" to rebuild ns-3
*you can call another terminal at ./scratch/rl_data_test_1 (for rural test) or ./scratch/rl_data_test_2 (for highway test)

iii. suppose rl_data_test_1 is to be used, run the following 
      "./waf --run \"/scratch/rl_data_test_1/rl_data_rest_1"
      " --traceFile=$PATH TO YOUR NS-3 ROOT FOLDER$/ns2mobility_test1.tcl"
      " --nodeNum=1 --simTime=1000.0 --logFile=ns2-mob_test1.log --hoType=myRL\" "

iv. run ./scratch/rl_data_test_1/test-rl-data.py on another terminal (or directly test-rl-data.py if already having a terminal opened there)

v. you should now see two programs running in parallel with essential data bridged and shared between the two process.