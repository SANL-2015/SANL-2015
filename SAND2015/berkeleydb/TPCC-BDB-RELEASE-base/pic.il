.inline report_data_to_simulator,1
        sethi 50000, %g0
.end

.inline do_quit,0
	mov 0, %o0
        sethi 51000, %g0
.end

.inline exec_magic_200,0
	mov 1, %o0
        sethi 200, %g0
.end


.inline sysAtomicSwap,2
	swap [%o1],%o0
.end
	
.inline sysCompareAndSwap,3
	cas [%o2],%o1,%o0
.end

.inline sysLoad64,2
        ldx [%o0], %o0
.end

        .inline         sysCompareAndSwap64,3
        casx            [%o2],%o1,%o0
        .end

        .inline         membarstoreload,0
        membar          #StoreLoad
        .end

        .inline         testAndSetByte,1
        ldstub          [%o0], %o0
        .end

        .inline         prefetch,1
        prefetch        [%o0], 2
        .end



        .inline         sysPopc,1
        popc            [%o0], %o0
        .end

        .inline         atomic2WordStore,3
        std             %o0,[%o2]
        .end

        .inline         getCurrentCFrame,0
        mov             %sp, %o0
        .end

        .inline         getCallerCFrame,0
        mov             %fp, %o0
        .end

        .inline         getCallerCFramePC,0
        ! Warning: don't use in leaf methods - their ret addr is not in %i7.
        mov             %i7, %o0
        .end

        .inline         flushInstruction,1
        flush           %o0
        stbar
        .end

        .inline         direct_fastEE,0
        ld              [%fp + 64], %o0
        .end

        .inline         getThreadStruct,0
        mov             %g7, %o0
        .end
