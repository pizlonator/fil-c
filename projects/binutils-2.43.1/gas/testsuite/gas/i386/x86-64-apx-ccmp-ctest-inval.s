# Check APX_F ccmp ctest instructions with illegal instructions.

	.text
	ccmpb {dfv=ct} $0x7b,%r18
	ctestb {dfv=sae} $0x7b,%r18
	ccmpb {dfv=of $0x7b,%r18
	ccmpb {dfv=of
	ccmpb {dfv=cf, cf, of, of} $0x7b,%r18
	ccmpb {dfv dfv=cf} $0x7b,%r18
	ccmpb {dfv=cf, ,cf} $0x7b,%r18
	ccmpb {dfv=cf,,} $0x7b,%r18
	ccmpb {dfv=,cf} $0x7b,%r18
	/* SCC insns don't support p/pe and np/po cc.  */
	ccmpp {dfv=cf} %r15w,%ax
	ccmppe {dfv=cf} %r15w,%ax
	ctestnp {dfv=cf} %r15w,%ax
	ctestpo {dfv=cf} %r15w,%ax
	/* Normal CC insns don't support t and f.  */
	sett %r8b
	setf %r8b
