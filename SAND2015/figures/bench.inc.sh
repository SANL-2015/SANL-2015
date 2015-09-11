RCL="rcl"

benchs="$RCL mcs spinlock flat posix saml cohort"
#benchs="mcsmit mcs" 

RUNS=30

bench_definitions=(
    'posix'         'POSIX'          '[posix] liblock: '           'gl_posix' 'lc rgb "#4a004a" lt 4 pt 12 pointsize 1.4'     'lc rgb "#94ff94" lt 4 pt 12'     '-F posix'
    'spinlock'      'SL'             '[spinlock] liblock: '        'gl_sl'    'lc rgb "#000094" lt 3 pt 1 pointsize 1.5'      'lc rgb "#9494ff" lt 3 pt 2'      '-F spinlock'
    'spinlockb'     'SL'             '[spinlock] bench: '          '?'        '?'                               '?'                               '-L'
    'mcs'           'MCS'            '[mcs] liblock: '             'gl_mcsl'  'lc rgb "#004a4a" lt 2 pt 9 pointsize 1.4'      'lc rgb "#8beded" lt 2 pt 8'      '-F mcs'
    'mwait'         'MWAIT'          '[mwait] liblock: '            'gl_mwait'  'lc rgb "#004a4a" lt 2 pt 9 pointsize 1.4'      'lc rgb "#8beded" lt 2 pt 8'      '-F mwait'
    'ticklock'          'TICK'            '[tick] liblock: '               'gl_tick'        'lc rgb "#009400" lt 5 pt 4 pointsize 1'      'lc rgb "#ffc994" lt 5 pt 4'                            '-F ticklcok'
    'mcstp'           'MCSTP'            '[mcstp] liblock: '             'gl_mcstpl'  'lc rgb "#009494" lt 2 pt 9 pointsize 1.4'      'lc rgb "#8beded" lt 2 pt 8'      '-F mcstp'
    'flat'          'FC'             '[flat combining] liblock: '  'gl_fc'    'lc rgb "#009400" lt 5 pt 4 pointsize 1'      'lc rgb "#ffc994" lt 5 pt 4'      '-F flat'
    'k42'          'K42'            '[k42] liblock: '              'gl_k42'        'lc rgb "#4a0000" lt 1 pt 6 pointsize 1.3' 'lc rgb "#ff94ff" lt 1 pt 6' '-F k42'
    'rcl'           'RCL'            '[rcl] liblock: '             'gl_rcl'   'lc rgb "#4a0000" lt 1 pt 7 pointsize 1.3' 'lc rgb "#ff9494" lt 1 pt 6' '-F rcl'
    'eercl'           'EERCL'            '[eercl] liblock: '             'gl_eercl'   'lc rgb "#4a0000" lt 1 pt 6 pointsize 1.3' 'lc rgb "#ff94ff" lt 1 pt 6' '-F eercl'
    'saml'           'SAML'            '[saml] bench: '               'gl_saml'   'lc rgb "#4a0000" lt 1 pt 6 pointsize 1.3' 'lc rgb "#ff94ff" lt 1 pt 6' '-F saml'
    'cohort'           'COHORT'            '[cohort] bench: '               'gl_cohort'   'lc rgb "#006575" lt 9 pt 3 pointsize 1.3' 'lc rgb "#ff8282" lt 9 pt 3' '-F cohort'
    # FIXME: pick better colors/line or point types
    'ccsynch'           'CCSYNCH'            '[ccsynch] liblock: '             'gl_ccsynch'   'lc rgb "#4a0000" lt 1 pt 13 pointsize 1.3' 'lc rgb "#ff9494" lt 1 pt 6' '-F ccsynch'
    'fccsynch'           'FCCSYNCH'            '[ccsynch] liblock: '             'gl_ccsynch'   'lc rgb "#000000" lt 1 pt 13 pointsize 1.3' 'lc rgb "#000000" lt 1 pt 6' '-F fccsynch'
    'dsmsynch'           'DSMSYNCH'            '[dsmsynch] liblock: '             'gl_dsmsynch'   'lc rgb "#4a0000" lt 1 pt 14 pointsize 1.3' 'lc rgb "#ff94ff" lt 1 pt 6' '-F dsmsynch'
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

