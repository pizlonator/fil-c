# Nasty hack so that make clean works without configure being run
TOP?=		.
_CONFIG_MK!=	test -e ${TOP}/config.mk && \
		    echo config.mk || echo config-null.mk
_CONFIG_MK?=	$(shell test -e ${TOP}/config.mk && \
		    echo config.mk || echo config-null.mk)
CONFIG_MK?=	${_CONFIG_MK}
include		${TOP}/${CONFIG_MK}
