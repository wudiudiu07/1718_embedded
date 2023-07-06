Corey Kirschbaum
Zishu Wu

Usage for the runall script:
Usage : ./runall.sh 1|2|3 <absolute path to image file>
        ./runall.sh clean
        ./runall.sh submit

Notes:
1. Please use the absolute path to the input image file when running the script.
2. The script assumes that you have placed DE1_SoC_Computer.rbf and Edge_Detector_System.rbf in the top level directory.
3. There is nothing to submit for part 3; you can run the code as-is to see hardware accelerated edge detection.

Part1:
Example instructions
./runall.sh 1 /home/root/lab10/e10_template/images/copper_640_480.bmp

Part2: 
Example instructions
./runall.sh 2 /home/root/lab10/e10_template/images/copper_640_480.bmp 

Part3: 
Example instructions
./runall.sh 3 /home/root/lab10/e10_template/images/copper_640_480.bmp