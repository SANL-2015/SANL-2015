RCL="rcl"

benchs="$RCL mcs spinlock posix"
#benchs="mcsmit mcs"
#benchs="spinlock mcs"

RUNS=30

bench_definitions=(
    'posix'         'POSIX'          '[posix] liblock: '           'gl_posix' 'lc rgb "#9bbb59" lw 5 pointsize 2  pt 12 '     'lc rgb "#9bbb59" lw 5 pointsize 2  pt 12'     '-F posix'
    'spinlock'      'SL'             '[spinlock] liblock: '        'gl_sl'    'lc rgb "#1f497d" lw 5 pointsize 2  pt 1 '      'lc rgb "#1f497d" lw 5 pointsize 2  pt 2'      '-F spinlock'
    'spinlockb'     'SL'             '[spinlock] bench: '          '?'        '?'                               '?'                               '-L'
    'mcs'           'MCS'            '[mcs] liblock: '             'gl_mcsl'  'lc rgb "#4bacc6" lw 5 pointsize 2  pt 9 '      'lc rgb "#4bacc6" lw 5 pointsize 2  pt 8'      '-F mcs'
    'mwait'         'MWAIT'          '[mwait] liblock: '            'gl_mwait'  'lc rgb "#004a4a" lw 5 pointsize 2  pt 9 '      'lc rgb "#8beded" lw 5 pointsize 2  pt 8'      '-F mwait'
    'mcsb'          'MCS'            '[mcs] bench: '               '?'        '?'                               '?'                               '-M'
    'mcstp'           'MCSTP'            '[mcstp] liblock: '             'gl_mcstpl'  'lc rgb "#009494" lw 5 pointsize 2  pt 9 '      'lc rgb "#8beded" lw 5 pointsize 2  pt 8'      '-F mcstp'
    'flat'          'FC'             '[flat combining] liblock: '  'gl_fc'    'lc rgb "#f79646" lw 5 pointsize 2  pt 4 '      'lc rgb "#f79646" lw 5 pointsize 2  pt 4'      '-F flat'
    'rclb'          'RCL'            '[rclb] bench: '              '?'        '?'                               '?'                               '-m -o'
    'rcl'           'RCL'            '[rcl] liblock: '             'gl_rcl'   'lc rgb "#c0504d" lw 5 pointsize 2  pt 7 ' 'lc rgb "#c0504d" lw 5 pointsize 2  pt 6' '-F rcl'
    'eercl'           'EERCL'            '[eercl] liblock: '             'gl_eercl'   'lc rgb "#4a0000" lw 5 pointsize 2  pt 6 ' 'lc rgb "#ff94ff" lw 5 pointsize 2  pt 6' '-F eercl'
)

on_bench() {
		bench=$1
    shift
    let N=0
		F=0
		
    while [ $N -lt ${#bench_definitions[*]} ]; do
				if [ ${bench_definitions[$N]} = $bench ]; then
						$@ "${bench_definitions[$N]}" "${bench_definitions[$N+1]}" "${bench_definitions[$N+2]}" "${bench_definitions[$N+3]}" "${bench_definitions[$N+4]}" "${bench_definitions[$N+5]}" ${bench_definitions[$N+6]}
						F=1
				fi
        let N=$N+7
    done

		if [ $F = 0 ]; then
				echo "FATAL: unable to find benchmark '$bench'"
				exit 42
		fi
}

gen_var() {
		point=$1
		let T=0
		A=0
		V=0
		while read line; do
				let T=$T+1
				A="$A+$line"
				V="$V+($line - a)^2"
		done
		avg=$(echo "($A)/$T" | bc -l)
		var=$(echo "a=$avg; sqrt($V)" | bc -l)
		echo "$point,$avg,$var"
}

