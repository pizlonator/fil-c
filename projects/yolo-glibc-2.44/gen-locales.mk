# defines target $(gen-locales) that generates the locales given in $(LOCALES)

LOCALE_SRCS := $(shell echo "$(LOCALES)"|sed 's/\([^ .]*\)[^@ ]*\(@[^ ]*\)\?/\1\2/g')
# The CHARMAPS dependency handling must be able to process:
# 1. No character map e.g. eo, en_US
# 2. Character maps e.g. en_US.UTF-8
# 3. Character maps with modifier e.g. tt_RU.UTF-8@iqtelif
# This complicates the processing slightly so we do it in multiple edits,
# the first captures the character map with the anchoring period while
# the rest of the edits remove the period to get a valid file or adjust
# the name to match the true name.
CHARMAPS := $(shell echo "$(LOCALES)" | \
		    sed -e 's/\([^ .]*\)\([^@ ]*\)\(@[^@ ]*\)*/\2/g' \
			-e 's/^\./ /g' \
			-e 's/ \./ /g' \
			-e s/SJIS/SHIFT_JIS/g)
CTYPE_FILES = $(addsuffix /LC_CTYPE,$(LOCALES))
gen-locales := $(addprefix $(common-objpfx)localedata/,$(CTYPE_FILES))

# Dependency for the locale files.  We actually make it depend only on
# one of the files.
$(addprefix $(common-objpfx)localedata/,$(CTYPE_FILES)): %: \
  ../localedata/gen-locale.sh \
  $(common-objpfx)locale/localedef \
  ../localedata/Makefile \
  $(addprefix ../localedata/charmaps/,$(CHARMAPS)) \
  $(addprefix ../localedata/locales/,$(LOCALE_SRCS))
	@$(SHELL) ../localedata/gen-locale.sh $(common-objpfx) \
		  '$(built-program-cmd-before-env)' '$(run-program-env)' \
		  '$(built-program-cmd-after-env)' $@; \
	$(evaluate-test)
