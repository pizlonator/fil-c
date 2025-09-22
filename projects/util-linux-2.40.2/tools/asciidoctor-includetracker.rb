# Copyright (C) 2023 Thomas Weißschuh <thomas@t-8ch.de>
# Extensions for asciidoctor to write dependency files for include directives.

require 'asciidoctor/extensions'

module IncludeTracker

  class Preprocessor < Asciidoctor::Extensions::Preprocessor
    def process document, reader
      document.attributes["include_dependencies"] = []
      reader
    end
  end

  class IncludeProcessor < Asciidoctor::Extensions::IncludeProcessor
    def process doc, reader, target, attributes
      docdir = doc.attributes["docdir"]
      path = target
      file = File.expand_path path, docdir
      data = File.read file
      reader.push_include data, file, path, 1, attributes
      doc.attributes["include_dependencies"] << file
      reader
    end
  end

  class Postprocessor < Asciidoctor::Extensions::Postprocessor
    def process document, output
      outfile = document.attributes["outfile"]
      fail if !outfile
      File.open outfile + '.deps', 'w' do |f|
        f.write outfile + ": " +
          document.attributes["include_dependencies"].join(' ')
      end
      output
    end
  end

  Asciidoctor::Extensions.register do
    preprocessor Preprocessor
    include_processor IncludeProcessor
    postprocessor Postprocessor
  end

end
