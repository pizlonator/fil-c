# Copyright (C) 2023 Thomas Weißschuh <thomas@t-8ch.de>
# Extension for asciidoctor to remove unicode dash in first section of manpage

require 'asciidoctor/extensions'

module UnicodeConverter
  BEFORE_NAME_SECTION = 1
  IN_NAME_SECTION = 2
  AFTER_NAME_SECTION = 3

  class Preprocessor < Asciidoctor::Extensions::Preprocessor
    include Asciidoctor::Logging

    def process document, reader
      lines = reader.read_lines
      state = BEFORE_NAME_SECTION
      command = document.attributes['docname'].rpartition('.')[0]
      lines.map! do |line|
        if state == IN_NAME_SECTION
          if line.sub! " \u2013 ", " - " or line.sub! " \u2014 ", " - "
            logger.warn "replacing unicode dash in name section of #{document.attributes['docfile']}"
          end

          if line.start_with? command and not line.include? ',' and not line.start_with? "#{command} - "
            logger.warn "adding dash to name section of #{document.attributes['docfile']}"
            line.sub! command, "#{command} - "
          end
        end

        if line.start_with? '== '
          if state == BEFORE_NAME_SECTION
            state = IN_NAME_SECTION
          else if state == IN_NAME_SECTION
            state = AFTER_NAME_SECTION
          end
        end
      end

        line
      end
      reader.restore_lines lines
      reader
    end
  end

  Asciidoctor::Extensions.register do
    preprocessor Preprocessor
  end

end
