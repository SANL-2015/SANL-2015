#!/bin/bash
for i in 103 138 185 247 331 443 592 791 1058 1414 1890 2526 3377 4514 6034 8065 10781 14410 19261 25746 34414 46000 61486 82187 109856 146840 196275 262352 350676 468734 626538 837467 1119409 1496268 2000000 2673317 

do

    for k in 0 1 5 10 20 35 50 100 200 300 500 1000 10000 100000
    do                
        echo -n $i	          
        echo -n ","
        echo -n $k
        echo -n ","

			
        for j in `seq 1 20`                  
        do
            sudo ./benchmark   -m -1 -n 20000 -d $i -A 1 -c 39 -s 1 -u -g 1 -l 0 -W $k -F saml -o -N -x custom_random > tmp
	cat tmp | grep -v percent | grep -v lib_exe | tr -d '\n' >> tmp2                       
	cat tmp | grep lib_exe | tr -s "lib_exe" " " | awk '{sum+=$1} END {print sum/NR, ","}' | tr -d '\n' >> tmp3
        done
     cat tmp2 |  tr -s "," "\n" | awk '{sum+=$1} END {print sum/NR, ","}' | tr -d '\n'
     cat tmp3 |  tr -s "," "\n" | awk '{sum+=$1} END {print sum/NR, ","}' | tr -d '\n'
     rm tmp2 tmp3
     echo
     done
echo
done

