suggest-00-special-ultra: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=ultra" suggest/00-special.tab tmp/00-special-ultra-actual
	suggest/comp suggest/00-special-ultra-expect.res tmp/00-special-ultra-actual.res 1 > tmp/00-special-ultra.diff
	rm tmp/00-special-ultra.diff
	echo "ok (suggest reg. 00-special ultra)" >> test-res
suggest-00-special-fast: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=fast" suggest/00-special.tab tmp/00-special-fast-actual
	suggest/comp suggest/00-special-fast-expect.res tmp/00-special-fast-actual.res 1 > tmp/00-special-fast.diff
	rm tmp/00-special-fast.diff
	echo "ok (suggest reg. 00-special fast)" >> test-res
suggest-00-special-normal: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=normal" suggest/00-special.tab tmp/00-special-normal-actual
	suggest/comp suggest/00-special-normal-expect.res tmp/00-special-normal-actual.res 1 > tmp/00-special-normal.diff
	rm tmp/00-special-normal.diff
	echo "ok (suggest reg. 00-special normal)" >> test-res
suggest-00-special-slow: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=slow" suggest/00-special.tab tmp/00-special-slow-actual
	suggest/comp suggest/00-special-slow-expect.res tmp/00-special-slow-actual.res 1 > tmp/00-special-slow.diff
	rm tmp/00-special-slow.diff
	echo "ok (suggest reg. 00-special slow)" >> test-res
suggest-00-special-bad-spellers: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=bad-spellers" suggest/00-special.tab tmp/00-special-bad-spellers-actual
	suggest/comp suggest/00-special-bad-spellers-expect.res tmp/00-special-bad-spellers-actual.res 1 > tmp/00-special-bad-spellers.diff
	rm tmp/00-special-bad-spellers.diff
	echo "ok (suggest reg. 00-special bad-spellers)" >> test-res
suggest-00-special-ultra-nokbd: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --keyboard=none --sug-mode=ultra" suggest/00-special.tab tmp/00-special-ultra-nokbd-actual
	suggest/comp suggest/00-special-ultra-nokbd-expect.res tmp/00-special-ultra-nokbd-actual.res 1 > tmp/00-special-ultra-nokbd.diff
	rm tmp/00-special-ultra-nokbd.diff
	echo "ok (suggest reg. 00-special ultra nokbd)" >> test-res
suggest-00-special-normal-nokbd: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --keyboard=none --sug-mode=normal" suggest/00-special.tab tmp/00-special-normal-nokbd-actual
	suggest/comp suggest/00-special-normal-nokbd-expect.res tmp/00-special-normal-nokbd-actual.res 1 > tmp/00-special-normal-nokbd.diff
	rm tmp/00-special-normal-nokbd.diff
	echo "ok (suggest reg. 00-special normal nokbd)" >> test-res
suggest-02-orig-ultra: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=ultra" suggest/02-orig.tab tmp/02-orig-ultra-actual
	suggest/comp suggest/02-orig-ultra-expect.res tmp/02-orig-ultra-actual.res 1 > tmp/02-orig-ultra.diff
	rm tmp/02-orig-ultra.diff
	echo "ok (suggest reg. 02-orig ultra)" >> test-res
suggest-02-orig-fast: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=fast" suggest/02-orig.tab tmp/02-orig-fast-actual
	suggest/comp suggest/02-orig-fast-expect.res tmp/02-orig-fast-actual.res 1 > tmp/02-orig-fast.diff
	rm tmp/02-orig-fast.diff
	echo "ok (suggest reg. 02-orig fast)" >> test-res
suggest-02-orig-normal: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=normal" suggest/02-orig.tab tmp/02-orig-normal-actual
	suggest/comp suggest/02-orig-normal-expect.res tmp/02-orig-normal-actual.res 1 > tmp/02-orig-normal.diff
	rm tmp/02-orig-normal.diff
	echo "ok (suggest reg. 02-orig normal)" >> test-res
suggest-02-orig-slow: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=slow" suggest/02-orig.tab tmp/02-orig-slow-actual
	suggest/comp suggest/02-orig-slow-expect.res tmp/02-orig-slow-actual.res 1 > tmp/02-orig-slow.diff
	rm tmp/02-orig-slow.diff
	echo "ok (suggest reg. 02-orig slow)" >> test-res
suggest-02-orig-bad-spellers: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=bad-spellers" suggest/02-orig.tab tmp/02-orig-bad-spellers-actual
	suggest/comp suggest/02-orig-bad-spellers-expect.res tmp/02-orig-bad-spellers-actual.res 1 > tmp/02-orig-bad-spellers.diff
	rm tmp/02-orig-bad-spellers.diff
	echo "ok (suggest reg. 02-orig bad-spellers)" >> test-res
suggest-02-orig-ultra-nokbd: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --keyboard=none --sug-mode=ultra" suggest/02-orig.tab tmp/02-orig-ultra-nokbd-actual
	suggest/comp suggest/02-orig-ultra-nokbd-expect.res tmp/02-orig-ultra-nokbd-actual.res 1 > tmp/02-orig-ultra-nokbd.diff
	rm tmp/02-orig-ultra-nokbd.diff
	echo "ok (suggest reg. 02-orig ultra nokbd)" >> test-res
suggest-02-orig-normal-nokbd: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --keyboard=none --sug-mode=normal" suggest/02-orig.tab tmp/02-orig-normal-nokbd-actual
	suggest/comp suggest/02-orig-normal-nokbd-expect.res tmp/02-orig-normal-nokbd-actual.res 1 > tmp/02-orig-normal-nokbd.diff
	rm tmp/02-orig-normal-nokbd.diff
	echo "ok (suggest reg. 02-orig normal nokbd)" >> test-res
suggest-05-common-ultra: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=ultra" suggest/05-common.tab tmp/05-common-ultra-actual
	suggest/comp suggest/05-common-ultra-expect.res tmp/05-common-ultra-actual.res 1 > tmp/05-common-ultra.diff
	rm tmp/05-common-ultra.diff
	echo "ok (suggest reg. 05-common ultra)" >> test-res
suggest-05-common-fast: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=fast" suggest/05-common.tab tmp/05-common-fast-actual
	suggest/comp suggest/05-common-fast-expect.res tmp/05-common-fast-actual.res 1 > tmp/05-common-fast.diff
	rm tmp/05-common-fast.diff
	echo "ok (suggest reg. 05-common fast)" >> test-res
suggest-05-common-normal: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=normal" suggest/05-common.tab tmp/05-common-normal-actual
	suggest/comp suggest/05-common-normal-expect.res tmp/05-common-normal-actual.res 1 > tmp/05-common-normal.diff
	rm tmp/05-common-normal.diff
	echo "ok (suggest reg. 05-common normal)" >> test-res
suggest-05-common-slow: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --sug-mode=slow" suggest/05-common.tab tmp/05-common-slow-actual
	suggest/comp suggest/05-common-slow-expect.res tmp/05-common-slow-actual.res 1 > tmp/05-common-slow.diff
	rm tmp/05-common-slow.diff
	echo "ok (suggest reg. 05-common slow)" >> test-res
suggest-05-common-ultra-nokbd: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --keyboard=none --sug-mode=ultra" suggest/05-common.tab tmp/05-common-ultra-nokbd-actual
	suggest/comp suggest/05-common-ultra-nokbd-expect.res tmp/05-common-ultra-nokbd-actual.res 1 > tmp/05-common-ultra-nokbd.diff
	rm tmp/05-common-ultra-nokbd.diff
	echo "ok (suggest reg. 05-common ultra nokbd)" >> test-res
suggest-05-common-normal-nokbd: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --keyboard=none --sug-mode=normal" suggest/05-common.tab tmp/05-common-normal-nokbd-actual
	suggest/comp suggest/05-common-normal-nokbd-expect.res tmp/05-common-normal-nokbd-actual.res 1 > tmp/05-common-normal-nokbd.diff
	rm tmp/05-common-normal-nokbd.diff
	echo "ok (suggest reg. 05-common normal nokbd)" >> test-res
suggest-00-special-ultra-camel: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --camel-case --sug-mode=ultra" suggest/00-special.tab tmp/00-special-ultra-camel-actual
	suggest/comp suggest/00-special-ultra-camel-expect.res tmp/00-special-ultra-camel-actual.res 1 > tmp/00-special-ultra-camel.diff
	rm tmp/00-special-ultra-camel.diff
	echo "ok (suggest reg. 00-special ultra camel)" >> test-res
suggest-00-special-fast-camel: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --camel-case --sug-mode=fast" suggest/00-special.tab tmp/00-special-fast-camel-actual
	suggest/comp suggest/00-special-fast-camel-expect.res tmp/00-special-fast-camel-actual.res 1 > tmp/00-special-fast-camel.diff
	rm tmp/00-special-fast-camel.diff
	echo "ok (suggest reg. 00-special fast camel)" >> test-res
suggest-00-special-normal-camel: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --camel-case --sug-mode=normal" suggest/00-special.tab tmp/00-special-normal-camel-actual
	suggest/comp suggest/00-special-normal-camel-expect.res tmp/00-special-normal-camel-actual.res 1 > tmp/00-special-normal-camel.diff
	rm tmp/00-special-normal-camel.diff
	echo "ok (suggest reg. 00-special normal camel)" >> test-res
suggest-00-special-slow-camel: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --camel-case --sug-mode=slow" suggest/00-special.tab tmp/00-special-slow-camel-actual
	suggest/comp suggest/00-special-slow-camel-expect.res tmp/00-special-slow-camel-actual.res 1 > tmp/00-special-slow-camel.diff
	rm tmp/00-special-slow-camel.diff
	echo "ok (suggest reg. 00-special slow camel)" >> test-res
suggest-00-special-bad-spellers-camel: prep
	suggest/run-batch "${ASPELL_WRAP} ${ASPELL} --camel-case --sug-mode=bad-spellers" suggest/00-special.tab tmp/00-special-bad-spellers-camel-actual
	suggest/comp suggest/00-special-bad-spellers-camel-expect.res tmp/00-special-bad-spellers-camel-actual.res 1 > tmp/00-special-bad-spellers-camel.diff
	rm tmp/00-special-bad-spellers-camel.diff
	echo "ok (suggest reg. 00-special bad-spellers camel)" >> test-res
.PHONY:  suggest-00-special-ultra suggest-00-special-fast suggest-00-special-normal suggest-00-special-slow suggest-00-special-bad-spellers suggest-00-special-ultra-nokbd suggest-00-special-normal-nokbd suggest-02-orig-ultra suggest-02-orig-fast suggest-02-orig-normal suggest-02-orig-slow suggest-02-orig-bad-spellers suggest-02-orig-ultra-nokbd suggest-02-orig-normal-nokbd suggest-05-common-ultra suggest-05-common-fast suggest-05-common-normal suggest-05-common-slow suggest-05-common-ultra-nokbd suggest-05-common-normal-nokbd suggest-00-special-ultra-camel suggest-00-special-fast-camel suggest-00-special-normal-camel suggest-00-special-slow-camel suggest-00-special-bad-spellers-camel
suggest:  suggest-00-special-ultra suggest-00-special-fast suggest-00-special-normal suggest-00-special-slow suggest-00-special-bad-spellers suggest-00-special-ultra-nokbd suggest-00-special-normal-nokbd suggest-02-orig-ultra suggest-02-orig-fast suggest-02-orig-normal suggest-02-orig-slow suggest-02-orig-bad-spellers suggest-02-orig-ultra-nokbd suggest-02-orig-normal-nokbd suggest-05-common-ultra suggest-05-common-fast suggest-05-common-normal suggest-05-common-slow suggest-05-common-ultra-nokbd suggest-05-common-normal-nokbd suggest-00-special-ultra-camel suggest-00-special-fast-camel suggest-00-special-normal-camel suggest-00-special-slow-camel suggest-00-special-bad-spellers-camel
